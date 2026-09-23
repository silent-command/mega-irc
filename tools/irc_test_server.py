#!/usr/bin/env python3
"""A small IRC server for the spike: enough of RFC 1459 to register,
join a channel, chat, and be pinged, with the behaviour the long run
needs to prove and a log with timestamps.

    python3 tools/irc_test_server.py [--port 6667] [--ping 90] [--chat 60]
                                     [--names N] [--flood N] [--log FILE]

  --ping S    PING every S seconds; a client that misses two is dropped
              with ERROR, as real servers do (0: never)
  --chat S    a bot line into every channel every S seconds (0: never)
  --names N   N extra nicks in the NAMES reply on JOIN (a big 353)
  --flood N   N lines in one burst to a channel every --flood-every seconds
              (0: never)
  --sasl U:P  offer SASL PLAIN in CAP LS and accept account U with
              password P (903), refusing anything else (904); without
              it CAP LS lists nothing, as a server with no SASL does

Every line in and out is logged with the time, and the PING round trip
is measured. Plaintext only: the spike's question is the transport.
"""
import argparse, asyncio, base64, sys, time

SERVER = "mega-irc.test"
log_file = None


def log(*a):
    line = time.strftime("%H:%M:%S ") + " ".join(str(x) for x in a)
    print(line, flush=True)
    if log_file:
        log_file.write(line + "\n"); log_file.flush()


class Client:
    def __init__(self, reader, writer, srv):
        self.r, self.w, self.srv = reader, writer, srv
        self.nick = None; self.user = None; self.registered = False
        self.negotiating = False      # CAP LS seen and no CAP END yet: registration waits
        self.channels = set(); self.ping_sent = None; self.missed = 0
        self.peer = writer.get_extra_info("peername")

    def send(self, line):
        log(f"-> {self.nick or self.peer}: {line}")
        self.w.write((line + "\r\n").encode("utf-8", "replace"))

    def numeric(self, n, text):
        self.send(f":{SERVER} {n} {self.nick or '*'} {text}")


class Server:
    def __init__(self, args):
        self.args = args; self.clients = []; self.channels = {}   # name -> set of clients

    def prefix(self, c): return f":{c.nick}!{c.user or 'u'}@{c.peer[0]}"

    def broadcast(self, chan, line, exclude=None):
        for c in self.channels.get(chan, ()):
            if c is not exclude: c.send(line)

    async def handle(self, reader, writer):
        c = Client(reader, writer, self); self.clients.append(c)
        log(f"connect from {c.peer}")
        try:
            while True:
                data = await reader.readline()
                if not data: break
                line = data.decode("utf-8", "replace").rstrip("\r\n")
                log(f"<- {c.nick or c.peer}: {line}")
                self.on_line(c, line)
        except (ConnectionResetError, asyncio.IncompleteReadError):
            pass
        finally:
            log(f"disconnect {c.nick or c.peer}")
            for ch in list(c.channels): self.part(c, ch, "connection closed")
            if c in self.clients: self.clients.remove(c)
            writer.close()

    def on_line(self, c, line):
        parts = line.split(" ", 1); cmd = parts[0].upper(); rest = parts[1] if len(parts) > 1 else ""
        if cmd == "NICK":
            want = rest.strip().lstrip(":")
            if any(o.nick == want for o in self.clients if o is not c):
                c.numeric("433", f"{want} :Nickname is already in use"); return
            old = c.nick; c.nick = want
            if c.registered and old:
                for ch in c.channels: self.broadcast(ch, f":{old}!{c.user}@{c.peer[0]} NICK :{want}")
        elif cmd == "USER":
            c.user = rest.split(" ")[0]
        elif cmd == "PING":
            c.send(f":{SERVER} PONG {SERVER} :{rest.lstrip(':')}")
        elif cmd == "PONG":
            if c.ping_sent is not None:
                log(f"   pong from {c.nick} after {time.time() - c.ping_sent:.2f} s")
                c.ping_sent = None; c.missed = 0
        elif cmd == "JOIN":
            for ch in rest.split(" ")[0].split(","):
                if not ch: continue
                self.channels.setdefault(ch, set()).add(c); c.channels.add(ch)
                self.broadcast(ch, f"{self.prefix(c)} JOIN :{ch}")
                names = [o.nick for o in self.channels[ch]] + [f"bot{i}" for i in range(self.args.names)]
                for i in range(0, len(names), 20):
                    c.numeric("353", f"= {ch} :{' '.join(names[i:i + 20])}")
                c.numeric("366", f"{ch} :End of /NAMES list.")
        elif cmd == "PART":
            for ch in rest.split(" ")[0].split(","): self.part(c, ch, "leaving")
        elif cmd == "PRIVMSG" or cmd == "NOTICE":
            target, _, text = rest.partition(" :")
            if target in self.channels: self.broadcast(target, f"{self.prefix(c)} {cmd} {target} :{text}", exclude=c)
            else:
                for o in self.clients:
                    if o.nick == target: o.send(f"{self.prefix(c)} {cmd} {target} :{text}")
        elif cmd == "QUIT":
            c.send(f"ERROR :Closing link ({rest.lstrip(':')})"); c.w.close(); return
        elif cmd == "CAP":
            sub, _, arg = rest.partition(" "); sub = sub.upper(); arg = arg.lstrip(":").strip()
            if sub == "LS":
                c.negotiating = True
                c.send(f":{SERVER} CAP * LS :{'sasl' if self.args.sasl else ''}")
            elif sub == "REQ":
                ok = arg == "sasl" and self.args.sasl
                c.send(f":{SERVER} CAP * {'ACK' if ok else 'NAK'} :{arg}")
            elif sub == "END":
                c.negotiating = False
        elif cmd == "AUTHENTICATE":
            if rest.strip().upper() == "PLAIN":
                c.send("AUTHENTICATE +")
            else:
                try:
                    authz, authn, pw = base64.b64decode(rest.strip()).decode("utf-8", "replace").split("\0")
                except ValueError:
                    authn = pw = None
                if self.args.sasl and f"{authn}:{pw}" == self.args.sasl:
                    c.numeric("900", f"{c.nick or '*'}!u@{c.peer[0]} {authn} :You are now logged in as {authn}")
                    c.numeric("903", ":SASL authentication successful")
                else:
                    c.numeric("904", ":SASL authentication failed")
        if c.nick and c.user and not c.registered and not c.negotiating:
            c.registered = True
            c.numeric("001", f":Welcome to the mega-irc test network, {c.nick}")
            c.numeric("002", f":Your host is {SERVER}, running version spike-1")
            c.numeric("003", ":This server was created just now")
            c.numeric("004", f"{SERVER} spike-1 o o")
            c.numeric("375", f":- {SERVER} Message of the day -")
            for l in ("- Welcome to the spike's server.", "- It pings, it chatters, it floods on request,", "- and it logs everything you send."):
                c.numeric("372", f":{l}")
            c.numeric("376", ":End of /MOTD command.")

    def part(self, c, ch, why):
        if ch in self.channels and c in self.channels[ch]:
            self.broadcast(ch, f"{self.prefix(c)} PART {ch} :{why}")
            self.channels[ch].discard(c); c.channels.discard(ch)

    async def pinger(self):
        while True:
            await asyncio.sleep(self.args.ping)
            for c in list(self.clients):
                if not c.registered: continue
                if c.ping_sent is not None:
                    c.missed += 1
                    if c.missed >= 2:
                        log(f"   {c.nick} missed two pings: dropping"); c.send("ERROR :Closing link (Ping timeout)"); c.w.close(); continue
                c.ping_sent = time.time(); c.send(f"PING :{SERVER}")

    async def chatter(self):
        n = 0
        while True:
            await asyncio.sleep(self.args.chat); n += 1
            for ch in list(self.channels):
                self.broadcast(ch, f":chatter!bot@{SERVER} PRIVMSG {ch} :line {n} at {time.strftime('%H:%M:%S')} — still talking, still listening")

    async def flooder(self):
        while True:
            await asyncio.sleep(self.args.flood_every)
            for ch in list(self.channels):
                log(f"   flood of {self.args.flood} lines into {ch}")
                for i in range(self.args.flood):
                    self.broadcast(ch, f":flood!bot@{SERVER} PRIVMSG {ch} :flood line {i + 1} of {self.args.flood} " + "x" * 60)

    async def run(self):
        srv = await asyncio.start_server(self.handle, "0.0.0.0", self.args.port)
        log(f"irc test server on :{self.args.port}, ping every {self.args.ping} s, chat every {self.args.chat} s, {self.args.names} extra names, flood {self.args.flood}")
        tasks = []
        if self.args.ping: tasks.append(asyncio.create_task(self.pinger()))
        if self.args.chat: tasks.append(asyncio.create_task(self.chatter()))
        if self.args.flood: tasks.append(asyncio.create_task(self.flooder()))
        async with srv: await srv.serve_forever()


def main():
    global log_file
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=6667)
    ap.add_argument("--ping", type=int, default=90)
    ap.add_argument("--chat", type=int, default=60)
    ap.add_argument("--names", type=int, default=0)
    ap.add_argument("--flood", type=int, default=0)
    ap.add_argument("--flood-every", type=int, default=600)
    ap.add_argument("--sasl", metavar="USER:PASS")
    ap.add_argument("--log")
    args = ap.parse_args()
    if args.log: log_file = open(args.log, "a")
    try:
        asyncio.run(Server(args).run())
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
