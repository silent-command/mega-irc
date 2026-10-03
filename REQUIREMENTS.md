# mega-irc: requirements, decisions, and record

## 1. Goal

An IRC client for the MEGA65 on mega-net: connect to a network, join
channels, read and write, over an evening. NOTES.md is the assessment
this started from.

## 2. Decisions

| Decision | Choice, and why |
|---|---|
| **The spike first** | Before any client: does an IRC session hold for hours over mega-net's TCP? Nothing in the family has kept a connection open that long. The spike is plaintext, one channel, no editor to speak of, and counts everything (NOTES.md section 3, risk 1). |
| **TLS in the MVP** | Settled by the user 2026-09-22, against the recommendation to defer it. The MVP carries TLS from the start, on the Gemini client's engine and CRYPTO bank. Plaintext 6667 stays for the fixture and for networks that still offer it. It also keeps NickServ's password off the wire, which a plaintext MVP would not have done. |
| **A root key or two, not a bundle** | Settled 2026-09-22. Gemini's trust on first use *by key* is wrong here: Let's Encrypt rotates every 60 to 90 days and every renewal would read as "key CHANGED", which is a false alarm on a schedule. Instead the client carries one or two root public keys, verifies the chain to them, and falls back to trust on first use only when no carried root matches, which is what self-hosted networks need. |
| **Both screen modes** | Settled 2026-09-22. The client reads whichever mode the machine is already in at launch and lays out for it, rather than forcing 25 or 50 rows. |
| **Scrollback as screen rows, not packed lines** | Settled 2026-09-22 as packed lines with an index; revised the same day, after the screen side was built (5.16), in the review of the MVP features: each view's log is fixed 80-byte rows in attic RAM, already wrapped and folded to screen codes, the same bytes `chat()` draws. Row N is at N x 80, so there is no index; a page is one DMA to the screen; a view switch and a scroll are the same redraw; and 25 and 50 rows need no re-wrap. The space it wastes is attic's, 1 MB a view for 13,000 rows, which no session fills. |
| **No default channel, and none required** | Settled by the user 2026-09-22, in two steps: the prompt supplies no channel, so a test run cannot land in a real channel by inertia (5.16 sent two lines to #mega65 on Libera that way); and it requires none either, since the user should be able to connect, read the MOTD in the status view and `/join` from there. A saved bookmark may carry channels. A name typed without its `#` gets one. |
| **Views on the function keys** | Settled 2026-09-22 in the review: eight views, the status window and up to seven channels, on F1, F3, F5, F7, F2, F4, F6, F8 in that order, so the first four need no SHIFT; a closed view's key is taken by the next one along, so the keys never skip (5.29). The claim first recorded here, that this is "the order the keyboard delivers them (PETSCII `$85` to `$8C`)", was wrong and shipped the keys dead: the MEGA65's queue gives `$F1` to `$FE` for F1 to F14 in label order, so the order is a mapping the client makes (5.23). Scrolling a page at a time on F9 and F11, HOME to the newest; the cursor keys walk the lines already sent (5.24). It was to be MEGA with the cursor keys, which cannot work on this keyboard: with MEGA held it sends cursor-down for both directions (5.26). Colours on MEGA-B and MEGA-F as mega-ftpc has them. Bookmarks in the family's SEQ-text shape with a deploy tool in the same commit; no stored passwords in the first version. |
| **Config files follow the family** | Settled 2026-09-22. The GEMMARKS / GEMHOSTS SEQ-text pattern, one record a line. |
| **Nick colours** | Settled 2026-09-23 (step 3). Each nick is painted in one of eight colours chosen by a hash of the name -- cyan, green, yellow, orange, pink, light green, light blue, light grey -- so a person keeps one colour for the session and the words around the name keep the text colour, which MEGA-F still changes all at once. The colour rides in the row's slot in the attic (3 bytes after its 80 screen codes), so it survives a scroll and a redraw; the one that matches the background is stepped past when drawn, so MEGA-B can hide no name. No per-nick table, no configuration (5.27). |
| **SASL PLAIN, with NickServ behind it** | Settled 2026-09-23 (step 3). One password, asked each session and never stored, serves both: with it the client sends `CAP LS` before `NICK`, and a server that lists `sasl` gets `CAP REQ`, `AUTHENTICATE PLAIN` and the account and password base64 in one line, `CAP END` after its 903. A server that lists nothing, refuses, or fails the login gets `CAP END` and the NickServ IDENTIFY on 001 as before, so the older networks still work; no password sends no CAP at all. Libera prefers SASL and Tor access requires it. The account name is the nick (5.27). |
| **Platform code** | The family's `m65_*` copies; the Gemini client's `netutil.c`; the NTP client's `ui.c`. |

## 3. Requirements

- **F-1** Connect by name or address, register, join a channel.
- **F-2** Show what comes, answer PINGs, send lines.
- **N-1** Every wait bounded on `$D7FA`; mega-net polled from every loop.
- **N-2** One `.d81` carrying the client and `MEGANET`.

## 4. Plan

| Step | What | Status |
|---|---|---|
| 1 | The spike: register, join, print, PING/PONG, a line on RETURN, counters; hours against the test server; a look at Libera | **Done** - 5.1, 5.2: two hours clean, 121 PINGs answered, no reconnects. 5.3: the PINGS counter bug found and fixed. 5.4: the ring stress and a Libera session both pass. Nothing left open |
| 2 | The MVP: TLS from the start with a carried root or two, a server and a channel window, scrollback in attic RAM, a non-blocking editor, NickServ, config on disk, and both screen modes | In progress. TLS is on the machine: Libera and OFTC joined over TLS from the MEGA65, the chain verified to a carried root, in both screen heights (5.15, 5.16). Both screen modes done (5.7). The views on the function keys, the scrollback in attic RAM, the slash commands and the colour keys (5.17); bookmarks in IRC.CFG with a deploy tool that carries it (5.18). The editor, its history and NickServ (5.24, 5.25); the scrollback keys settled (5.26). **Done** |
| 3 | Several windows, nick colours, SASL | The windows came with step 2 (5.17). Nick colours and SASL PLAIN with the NickServ fallback, both on the machine (5.27). **Done** |

## 5. Findings

### 5.1 The spike, built and stopped at the cable (2026-09-15)

**What exists.** `src/ircc.c`: DHCP, four prompts (server, port, nick,
channel; RETURN keeps the defaults 192.168.1.232, 6667, mega65,
#mega65), connect, `NICK`/`USER`, `433` answered with a digit on the
nick, `JOIN` on `001`, every line shown on rows 1-20 (`<nick> text`
for PRIVMSG, otherwise nick, command and the trailing text, UTF-8
folded, wrapped at 79 columns), `PING` answered, a `PING :alive` of
its own after 150 s without a byte and the link declared dead and
reconnected after 300 s or when mega-net says it is gone, a
`PRIVMSG` of its own every ten minutes ("still here after N min"),
RETURN sends the typed line, RUN/STOP sends `QUIT` and leaves. Row 22
counts: uptime, lines and KB received, lines sent, pings answered, the
current quiet gap and its maximum in seconds, reconnects. 15,047
bytes; `bin/IRC.D81` with IRC first.

`tools/irc_test_server.py`: registration with numerics and a MOTD,
JOIN with NAMES (`--names N` pads it), PRIVMSG and NOTICE relayed,
`PING` every `--ping` seconds with the client dropped after two
misses, a bot line every `--chat` seconds, `--flood N` lines every ten
minutes, every line in and out logged with the time and each PONG's
round trip. Proven from the Mac with a Python probe: 001-004, the
MOTD, JOIN, 353/366, PONG, QUIT.

**What stopped it.** With the disk built and the server up, the
MEGA65's USB-serial adapter vanished from the Mac — not a busy port,
not a stale process (two sixteen-day-old `m65 -F -4` were found and
stopped first): `/dev/cu.usbserial-23201` gone and nothing serial on
the USB bus. The cable was the user's to check, and it came back at
16:01:37; the spike registered and joined at 16:02:47. That first run
was not clean: a fixture server about ninety minutes old (started
before the cable dropped, the Mac likely asleep in between) stopped at
16:13:36, and the client's uptime showed it had restarted around
16:14 — an event not reconstructed. The client itself was healthy
throughout (three back-to-back reads of the counter advanced
monotonically; the ten-minute reads that looked like the uptime going
backwards were screen dumps tearing the once-a-second counter
rewrite). A clean soak was started at 16:29:47 against one fresh
server (`build/irc_soak.log`, ping 60 s, chat 120 s), instrumented
from the server log alone so the watching never touches the serial
port. The hours are 5.2.

Two lessons already, for the record: read the counter off the serial
port at most rarely and never trust one torn read (the row rewrites
whole each second); and the soak's instrument is the server's log, not
the machine's screen, because the log needs no serial and cannot tear
— a PONG for every server PING is the liveness signal, and its absence
for over 150 s is the alarm.

**Next, exactly.** With the link back:

    source ../mega-net/tools/m65lib.sh
    python3 tools/irc_test_server.py --ping 90 --chat 60 --log build/irc_srv.log   (on the Mac, if not running)
    put_d81 bin/IRC.D81 IRC.D81
    boot_prg irc.d81 irc 'erver:' 3
    type_line ''; type_line ''; type_line ''; type_line ''
    wait_for 'joined' 60
    row 22                                          then no more than once a minute

Leave it for hours; the questions are in row 22 and the server's log:
does every server PING get its PONG (the log times them), does the
quiet gap ever reach 150 s (our own PING) or 300 s (a reconnect), do
the ten-minute lines all arrive, does the count of reconnects stay at
zero. Then `--flood 200` and `--names 300` for the 4 KB ring, and a
few minutes on Libera (`irc.libera.chat`, 6667) for interop. Record
each as a finding here.

### 5.2 The soak: one clean hour (2026-09-15, in progress)

The clean run of 5.1, joined 16:29:47, at the one-hour mark (17:25):
the client's own counters, five reads two seconds apart, all
monotonic — up 0:56, 96 lines in, 8 out, 56 pings answered, the
longest quiet gap 60 s (the server's ping interval; a byte never more
than a ping apart), reconnects zero. The server's log agrees: 56
PONGs, one connect and no disconnect, the last PONG 0.01 s round trip.
Every server PING got its PONG for an hour; the send path carried the
client's line every ten minutes; mega-net's TCP held the session with
nothing retransmitted that the client noticed. At two hours (18:29) it stood the same: up 2:00, 192 lines in, 15 out,
reconnects zero, and the server 121 PONGs to 121 PINGs, one connect and
no disconnect, the worst round trip 0.17 s. Two hours is the evening's
start; mega-net's TCP holds an IRC session.

One bookkeeping bug, not a transport fault: the client's on-screen
PINGS counter read 1 at two hours where the server had answered 121.
Every other counter was right and the two instruments agree the
session never dropped, so `pings` (a global at $6041) is being zeroed
by something -- a stray write, most likely a buffer's edge, since it
alone is wrong. It does not touch the transport (the PONGs went out;
the server counted them); it is the spike's own display, to find before
the MVP reuses these counters. The run continues; then the ring stress
(`--flood`, `--names`) and a few minutes on Libera.

**The soak ended at two hours, not on its own.** At about 18:33 the
machine stopped running this spike: a screen dump showed a different
IRC client — tabbed windows (`[(SERVER)]`, `F1..F8: TABS`,
`CTRL+P/N`, `SHIFT+UP/DN: SCROLL`), a status line in another format,
its own uptime at two and a half minutes. This spike has none of
those; it was not deployed from here. Whatever put it there, the
machine is no longer ours to measure, so the instrumentation was
stopped and the machine left alone. The two-hour result above stands
on its own evidence (the client's counters and the server's log up to
18:32:09). Where the longer soak, the ring stress and Libera go from
here is the user's to say.

### 5.3 The PINGS counter: an unbounded write, and what it landed on (2026-09-22)

5.2 left `pings` reading 1 where the server had answered 121, and
guessed "a stray write, most likely a buffer's edge". It was exactly
that, and the edge is now named.

`draw_counts()` composed the status row into `static char t[80]`
through a pair of helpers with no end pointer:

    static void put_num(char **p, unsigned long v) { ui_put_ulong(*p, v); while (**p) (*p)++; }
    static void put_str(char **p, const char *s) { while (*s) *(*p)++ = *s++; }

Composed on the host with the counter values a run actually reaches,
the row is 77 characters in the first half hour, 79 for the next hour,
and 80 or 81 from about 1:40 onward. An 80-byte buffer holds indices
0..79, so both of those overrun it:

| composed length | what goes past the end | effect |
|---|---|---|
| 80 | the terminator alone | the next byte is zeroed |
| 81 or more | a digit, then the terminator | the next two bytes are written |

What the next byte is, read from the pre-fix build's own map rather
than assumed: `draw_counts.t` at `$5FF1`, size `$50`, so it ends at
`$6040`, and `pings` begins at `$6041`. The overflow lands on the low
byte of `pings` and on nothing else. `$6041` is the address 5.2
recorded for `pings` when the symptom was seen, which ties the two
observations together. At 80 characters the terminator zeroes that byte
outright and the next PING makes it 1 - the reading that was on the
screen. At two hours, 6600 plausible combinations of the other counters
compose to exactly 80.

A caution for whoever reads a map next: **the size column is hex.** A
first pass here read `$50` as fifty, put the buffer's end at `$6022`,
and reported the two symbols as not adjacent. `$5FF1 + $50 = $6041`
says otherwise, and two things in the same column settle the base -
`line[513]` appears as `201`, and `draw_input.t` at `$5FA1` plus `$50`
lands exactly on `draw_counts.t`. The family's rule about asking
whether a number is possible applies to a tool's output as much as to a
benchmark (mega-radio 5.4).

**The fix.** `put_s`/`put_n` take an end pointer and stop at it, and
every caller passes one. Four sites were unbounded, not one:

- `draw_counts()`, the buffer now 112 bytes.
- `handle_line()` building into `shown[256]`. Its fold bound,
  `(unsigned char)(sizeof shown - 1 - (o - shown))`, went negative and
  wrapped to a large length once `o` was already past the end, so a
  long `params` compounded its own overflow. It is now clamped and
  cannot be reached with `o` past the end.
- `send3()`, which a long server PING trailing could have run past
  `tmp[513]`.
- the RETURN handler, safe by construction today (input caps at 200,
  nick at 16) but written the same unbounded way.

Two further changes with it. The row was shortened - `rx N (K KB)`,
`tx`, `ping`, `idle Ns max M`, `rc N` - because even bounded it did not
fit the 79 columns `ui_line` shows, and the tail was cut off at
`RECONNECT` on screen. That truncation was noticed during 5.2 and set
aside as cosmetic; it was the visible half of this same defect. It now
composes to 65 characters at two hours and 77 after a simulated week.
And the ten-minute keepalive PRIVMSG is off by default (`chatter_s` is
a variable set to 0, not a macro, so the counter stays read and
`-Werror` stays quiet): the send path is proved by 5.2 and by every
PONG, and an unattended line every ten minutes would be a nuisance on a
public network.

15,047 bytes before, 14,551 after.

### 5.4 The ring stress and Libera (2026-09-22)

**Ring stress**, against the fixture with `--names 300 --flood 200
--flood-every 30 --chat 20 --ping 60` (the fixture grew a
`--flood-every`; ten minutes was far too slow to test with). Twelve
minutes, and the two instruments agree:

- the client: rx 4068 lines / 496 KB, tx 3, pings 10, longest quiet gap
  23 s, reconnects 0;
- the server: 20 floods of 200 lines, 10 PINGs each answered, round
  trips 0.01-0.02 s *even with a flood in flight*, one connect, no
  disconnect, no errors in 5176 logged lines.

A 300-nick NAMES reply and 200-line bursts both render wrapped at 79
columns with the continuation indent. The 4 KB ring and the 2 KB
advertised window absorb a 200-line burst without loss, and the send
path stays responsive while the receive path is saturated. Past the ten
minute mark the client had sent nothing but NICK, USER, JOIN and PONGs,
which is the keepalive gate proved by observation and not only by
reading the code.

**Libera**, `irc.libera.chat` 6667, nick `m65spike`, three minutes
forty-four. Resolved (three A records), registered on
`uranium.libera.chat`, took 001-004, 250/265/266, the whole MOTD and
376, saw its own MODE and JOIN echoed, and got 353/366 for `#mega65`
with the channel's real users in it. No server PING arrived inside four
minutes; the client's own `PING :alive` fired at the 150 s idle mark
and the server's PONG came back. Counters: up 3:44, rx 40 lines / 3 KB,
tx 3, pings 0, idle max exactly 150 s, reconnects 0 - every one of them
right, including `pings 0`, which is the count of *server* pings
answered. A host probe first confirmed plaintext 6667 is open from this
network and that the greeting's longest line is 98 bytes, well inside
the 512-byte line buffer; Libera sends NOTICE lines before registration
and the client renders them.

The session was ended by resetting the machine, so no QUIT was sent:
`m65lib.sh` has no documented escape for RUN/STOP, and a dropped link
is routine on IRC. If a clean QUIT is wanted from a script, that escape
is the thing to find.

### 5.5 What TLS in the MVP actually costs (2026-09-22)

Section 2 settles on carrying a root key or two and verifying the
chain. This is what that needs, measured against what Libera and OFTC
serve today rather than assumed.

**The chains.** Both networks serve the same shape: three
certificates, every key RSA, every signature `sha256WithRSAEncryption`.

| | leaf | intermediate | third |
|---|---|---|---|
| Libera | molybdenum.libera.chat, RSA-4096, 1768 B | Let's Encrypt YR1, RSA-2048, 1247 B | ISRG Root YR, RSA-4096, 1528 B |
| OFTC | weber.oftc.net, RSA-2048, 1319 B | Let's Encrypt YR1, RSA-2048, 1247 B | ISRG Root YR, RSA-4096, 1528 B |

The third is ISRG Root YR cross-signed by ISRG Root X1. The servers
send it; they do not send X1. Both chains are captured under
`tests/certs/` as DER, so the host suite can be built on what the
networks actually serve.

**Carry Root YR, and the work halves.** A verification costs what the
*issuer's* key costs, not the subject's. Anchored on Root YR's key the
client verifies the leaf under YR1's RSA-2048 and YR1 under Root YR's
RSA-4096, and stops: two exponentiations, X1 never touched. Anchored on
X1 it needs a third, under another RSA-4096. Confirmed twice, by
`openssl verify -partial_chain` and by checking each signature directly
against the parent's key. So: Root YR first, X1 kept as the fallback
for a server that chains straight to it.

That nearly went into this file backwards. The first attempt anchored
on Root YR *without* `-partial_chain` and reported "verification
failed", which is openssl refusing a non-self-signed anchor and says
nothing about the design. It is the same shape of mistake as reading
the map's hex size column as decimal in 5.3: a tool's confident output
is a measurement, and gets the same scepticism as any other.

**What is missing is PKCS#1 v1.5.** Certificate signatures use it.
TLS 1.3 needs only PSS for CertificateVerify, so PSS is all the family
has: `rsa.h` declares `rsa_pss_verify` and nothing else, and no
`pkcs1`, `v1_5` or `v15` appears anywhere under any project's `src/`.
The gap is narrow, though. `rsa_pss_verify` does its exponentiation
entirely through `mp.h` and differs from v1.5 only afterwards:
`mp_init`, `mp_from_be`, `mp_to_mont`, `mp_mont_exp`, `mp_from_mont`,
`mp_to_be`, then a padding check. A v1.5 verifier makes the same six
calls and checks `00 01 FF..FF 00` followed by the SHA-256 DigestInfo.
`MP_MAX_LIMBS` is 256, which is 4096 bits, so both key sizes are
already in range. It goes in a new file here, because the crypto copied
from `../ssh` and `../mega-gemini` stays identical to its origin.

**Storage fits, without much to spare.** The bank's store is
`CERT_CAP` of `$1700`, exactly the 5888 bytes of bank 5 above
mega-net's socket pool at `$5E900`.

| chain | bytes | left in the store |
|---|---|---|
| Libera | 4543 | 1345 |
| OFTC | 4094 | 1794 |

But the engine keeps only the leaf: `cert_stream` appends while
`cert_index == 0` and records `leaf_len` alone. Keeping a chain means
appending every certificate and holding a small table of offsets and
lengths beside it.

**New work beyond the arithmetic.** `x509.h` states its boundary
plainly: "No chain, no names, no dates." So the walker needs the
to-be-signed byte range, the signature algorithm and value, and the
issuer and subject names to link one certificate to the next; a clock
for validity, which mega-ntp already sets at boot; and hostname
matching against the leaf's subject alternative names.

**The cost to expect, and it is an estimate.** Gemini verifies
RSA-4096 in 5.5 s and RSA-2048 in roughly a quarter of that. A Libera
connect would then spend about 5.5 s on CertificateVerify under the
leaf's RSA-4096 key, and 1.4 s plus 5.5 s on the chain: some 12 s of
exponentiation before X25519 and everything else. Against gemini's 11
to 12 s handshakes, expect close to 20 s to join. IRC pays that once a
session, which is why it is affordable here and would not be on a page
fetch. It is borrowed from another client's numbers and is to be
measured, not quoted.

### 5.6 The missing primitive, written and proved on the host (2026-09-22)

5.5 found that nothing in the family verifies a PKCS#1 v1.5 signature,
which is what every certificate in both chains is signed with. It
exists now, and the host suite proves it against the certificates
Libera and OFTC actually served.

**What was written**, all of it new and all of it in this project; the
crypto and `x509.c` copied from `../mega-gemini` are untouched and
still byte-identical to their origin.

- `src/crypto/pkcs1.c`: `rsa_pkcs1_sha256_verify`. The same six `mp.h`
  calls `rsa_pss_verify` makes, then the v1.5 padding check in place of
  PSS's. It insists the hash ends exactly at the end of the block,
  which is the check a lax verifier leaves out and a forger looks for.
- `src/tls/chain.c`: the walking `x509.h` deliberately omits.
  `chain_parts` finds the TBSCertificate's range and the signature and
  refuses any algorithm but sha256WithRSAEncryption; `chain_tbs_hash`
  hashes through the read hook in 64-byte pieces, so nothing needs a
  whole certificate in memory at once; `chain_verify_link` verifies one
  link. Everything is read through `x509_read`, as x509.c does, so a
  certificate may sit in far memory on the machine and in a plain
  buffer on the host without this code knowing the difference.
- `tests/test_chain.c`, run by `python3 build.py test`: **32 checks, 0
  failed.**

**The DigestInfo was not recited from memory.** The nineteen bytes
`3031300d060960864801650304020105000420` were recovered from a real
Libera signature, by exponentiating it with the issuer's key and
reading the block back out. The whole block is `00 01`, then `FF`
padding, then `00`, then those nineteen, then the hash: 202 padding
bytes under a 2048-bit key and 458 under a 4096-bit one, each
reconciling exactly to the modulus length.

**What the suite checks.** Per network: the walked offsets against what
openssl reports, the signed range starting at 4 and running 1488 bytes
for Libera and 1039 for OFTC; that the leaf verifies under the
intermediate and the intermediate under Root YR, which is the
two-verification path a carried Root YR key buys; and that it fails
when it should, under the wrong issuer, under the certificate's own
key, with a bit flipped in the signature, with a bit flipped in the
signed body, and on a truncated certificate. Root YR verifies under
Root X1 as well, so the fallback anchor works. 32 is exactly the number
of checks written, so none were skipped, and the positive and negative
cases both pass, so the verifier discriminates rather than returning a
constant.

**Two problems for the machine, neither solved here.**

- `crypto_equal` and `crypto_yield` are defined in `curve25519.c`. So
  anything wanting RSA drags in curve25519 and, behind it, sha512. On
  the host that costs nothing. In a bank it will, and it is why the
  host suite links the whole crypto set rather than a hand-picked one:
  naming a minimal list only moves the link error around.
- `chain.c` holds the issuer's modulus and exponent and the child's
  signature in statics, 1032 bytes, because a 512-byte modulus is not
  going on the stack; `x509.c` does the same for its own RSA path. On
  top of that `mp_shared` and `mp_scratch` come to about 2.6 KB.
  Where all of it lives on the MEGA65 is the integration question, and
  gemini's bank has only a few hundred bytes free. This may have to run
  in the client rather than in a bank.

**The cost is still unmeasured.** 5.5's estimate of roughly 12 s of
exponentiation per connect is borrowed from gemini's numbers. Nothing
here has run on hardware.

### 5.7 Both screen modes, and a wedged port (2026-09-22)

Decision 3: follow whichever height the machine is already in, rather
than forcing one.

**The change.** `m65_screen_init` reads the display-rows register,
which the ROM's 80x50 leaves at 49, and keeps that height instead of
calling `setscreensize(80, 25)`. The layout is measured from the bottom
now, so the client's four rows sit at the end of whatever screen there
is; in 25 rows every row is exactly where it always was.

**The screen had to move, and that is the part worth knowing.** At 50
rows the screen is 4000 cells. From `$0800` that runs to `$179F`, over
mega-net's trampoline at `$1600`. So it goes to `$10000` in bank 1,
ending at `$10F9F`, just below the font at `$11000`. The SSH client
found this first and fixed its screen there for both heights (ssh
5.23). It costs nothing here, because conio reads the screen base out
of the VIC registers rather than assuming one: `setscreenaddr()` is
enough and `cputs`, `gotoxy` and `clrscr` all follow, with `clrscr`
clearing width times height, which is the right 4000 in 50-row mode.

**Read once, not on every init.** `m65_screen_init` runs again after
every disk write, which tears 80-column mode down. As first written it
re-read the display-rows register each time -- but the height is set by
the V400 bit, which this module writes, while the register it reads is
the ROM's. A disk write could therefore have dropped a 50-row session
back to 25 mid-run. The height is now remembered the way the colours
already are.

**A mistake worth recording.** The three platform copies' `.c` files
were byte-identical, so the `.h` files were assumed to be as well and
mega-irc's header was copied over the others. They were not identical:
gemini's declared `m65_screen_reverse`, which its own `ui.c` calls, and
its build broke. Restoring from git and inserting only the new
declarations fixed it. Check what is about to be overwritten, not the
file beside it.

**Where the family stands**, surveyed rather than assumed:

| copy | rows-aware | links it |
|---|---|---|
| mega-irc, mega-ntp, mega-gemini | yes, the `.c` byte-identical | irc and ntp do; gemini excludes it |
| mega-ftpc | no, and already divergent before this | yes |
| mega-radio | no | a dormant probe |
| ssh | yes, its own smaller register-level module | yes |

mega-gemini's own `src/m65/screen.c` is untouched: it puts the screen
at `$0800` deliberately and drives a pager built for 25 rows, and
reworking that would cost a great deal here for no gain. mega-ftpc and
mega-radio are left alone too. They keep the old behaviour, nothing is
broken, and neither was in scope.

**The port wedged mid-session and held this up for hours.** The JTAG
link stopped working, and for a while nothing could run in either
height. The
signature is precise, and it is worth knowing because it is not the
"held port" of the other failure mode: the device nodes exist, nothing
holds them, `m65.osx` reports `failed to set output baud rate using
IOSSIOSPEED`, `--autodiscover --verbose` detects the serial port but
reports `found 0 candidate USB devices`, and `mon.py` dies with
`termios.error: (22, 'Invalid argument')`. The decisive test: open the
node, `tcgetattr`, then `tcsetattr` handing back the port's *own
unmodified* attributes. That failed with EINVAL on both channels, which
means the port is wedged rather than busy or misconfigured. The likely
cause is ours: `m65.osx` was SIGKILLed several times by a timeout
wrapper. Only a replug or a power cycle clears it.

**Verified on hardware, in both heights (2026-09-22).** The user
restarted the machine and the port came back: `tcsetattr` succeeded on
both channels, which is the same test that condemned it. So a board
restart re-enumerates the adapter and clears the wedge. That is worth
knowing, because it is less disruptive than unplugging the cable and it
is the first thing to try.

**25 rows**, against the fixture: every row where 5.4 left it, and the
counters right --
`up 0:00:07  rx 15  tx 3  ping 1  idle 1s max 5  rc 0`. The screen now
lives at `$10000` and the client draws through it correctly, which is
the relocation proved rather than reasoned.

**50 rows**: the screen dump carries 52 rows where 25-row mode gives
27, and the layout lands exactly where the arithmetic puts it.

| element | 25 rows | 50 rows |
|---|---|---|
| title | 0 | 0 |
| chat ends | 20 | 45 |
| counts | 22 | 47 |
| status and input | 23 | 48 |
| keys | 24 | 49 |

The fixture's log agrees with both screens: two connects, one
disconnect which was the reset between the runs, no errors, and a PONG
answered in 0.01 s.

**Two things about testing 50-row mode, for whoever does it next.**
`boot_prg` cannot be used: it resets first, which undoes the mode.
Reset, poke, then mount and run by hand. And poking the display-rows
register alone does not make the machine genuinely 80x50 -- set the
V400 bit as well, `poke 53297,peek(53297)or8`, or conio reads a height
that disagrees with the register the client detects from. Both pokes do
survive as far as the `run`, which was the open question.

### 5.8 The chain verified end to end, and the name that would have failed (2026-09-22)

5.6 proved one link. This is the whole thing: a chain walked up to a key
the client carries, the leaf checked against the host actually dialled,
and the dates checked.

**A common-name check would have rejected both networks.** Libera's leaf
is `CN=molybdenum.libera.chat` and OFTC's is `CN=weber.oftc.net`. Nobody
dials either. The names people use, `irc.libera.chat` and
`irc.oftc.net`, live in the subject alternative names: twelve of them
for Libera, four for OFTC. So the client walks the SAN extension, OID
`55 1d 11`, and does not consult the common name at all. Both CNs happen
to appear in their own SAN as well, which changes nothing: matching the
*dialled* host against the CN would still have failed. This is the sort
of thing that looks like a detail until it rejects every server.

**The anchor is carried as a hash, not a modulus.** Both networks send
the cross-signed ISRG Root YR, so the client verifies every signature up
the chain and then checks the top certificate's SubjectPublicKeyInfo
hash against the ones it carries. 32 bytes an anchor instead of 512, and
no weaker for it: nobody can produce a certificate whose SPKI hashes to
a carried value, and the link below it still has to verify under that
key. `tools/gen_roots.py` emits `src/tls/roots.c` from the captured
fixtures, 64 bytes for ISRG Root YR and ISRG Root X1.

**Dates are digit strings.** Both use UTCTime, tag `0x17`,
`YYMMDDHHMMSSZ`, so validity is a twelve-digit comparison and needs no
calendar. GeneralizedTime, which a certificate reaching past 2049 would
carry, is reduced to the same twelve digits by dropping the century.

**What the suite proves**, 65 checks with none failing, on both real
chains: the walked offsets; every link; the full chain reaching a
carried anchor; a two-certificate chain refused as sound but untrusted;
the wrong host refused before any arithmetic runs; an expired chain
refused; the leaf alone refused; and failure under a flipped signature
bit, a flipped body bit and truncation. 65 is exactly the number of
checks written, so none were skipped, and positive and negative cases
both pass, so the code discriminates rather than returning a constant.

**A test that could not fail, caught before it was committed.** A first
draft of the name case ended in `|| 1`, which made it vacuously true.
What it was trying to record, whether the CN also appears in the SAN, is
the issuing authority's choice and not a property to assert, so it is
printed as a note instead. A check that cannot fail inflates the count
and proves nothing: the same fault as a benchmark whose result nothing
reads (mega-radio 5.4).

**Settled, and built (2026-09-22).** Date checking needs a clock and
this machine may not have one set. The user chose: **say so, and let
them set it by hand or with mega-ntp.** Refusing every certificate when
the clock is unset would be wrong, and skipping the check silently
would be worse than saying so on screen.

`src/clock.c` makes the judgement. The threshold for "plainly unset" is
the day this program was built, because a clock reading earlier than
its own software cannot be right, and `__DATE__` supplies that without
a constant in a header that goes stale. `clock_read` returns whether
the clock is worth believing, so the caller either hands the time to
`chain_verify` or hands it null and puts the warning on the screen. It
reads the month as 1 to 12 and the year from 1900, which is what the
library actually passes whatever its header says about 0 to 11;
mega-ntp's `rtc_read` found that first and this depends on it.

24 checks cover a clock that is set, a chip that has never been set,
the year before the build, every field out of range, a leap second, a
date absurdly far ahead, and the century wrap where `min_yy + 20` would
have overflowed a two-digit year and quietly inverted the test rather
than widening it. The judging half takes its threshold as a parameter
so the suite means the same thing whenever it is compiled.

### 5.9 One screen module across the family (2026-09-22)

The user invited standardising the other clients while working here. The
survey found one real inconsistency, and corrected an assumption of mine.

**The inconsistency: the first press of the background key.** The user
asked for this earlier in the year: the machine boots blue, so the first
press should go to black rather than to the next colour up, which is
yellow. mega-ftpc, ssh, gopher and mega-gemini's own module all had it.
The shared platform module never did. So mega-ntp's key rotated to
yellow while mega-ftpc's went dark, for no reason a user could see, and
mega-irc carried the same gap with no key bound to it yet. The shared
copy now has the block word for word from mega-ftpc, so the copies
converge rather than merely agreeing.

**The correction: mega-ftpc had not drifted, it was ahead.** 5.7
recorded it as a divergent copy left alone. That was the wrong reading.
Its copy was the one carrying the black-first press; what it lacked was
the height support written here. It has now taken the shared module
whole, checked before moving rather than assumed: it writes no screen
memory directly, and its bank 1 holds the font at `$11000` and the
bookmarks at `$11800`, so a screen ending at `$10F9F` lands on nothing.
Its status and keys rows follow the screen's end. 31,755 bytes to
31,900.

**Where the family stands now.** All four copies of
`src/platform/m65_screen.c` are byte-identical: mega-irc, mega-ntp,
mega-gemini and mega-ftpc. The headers still differ and should: each
declares what its own client needs, and gemini's `m65_screen_reverse`
was lost once to a copy that assumed otherwise (5.7).

| client | the module it links | black-first | follows the height |
|---|---|---|---|
| mega-irc, mega-ntp, mega-ftpc | the shared platform copy | yes | yes |
| mega-gemini | its own `src/m65/screen.c` | yes | no |
| ssh | its own register-level module | yes | yes |
| gopher | its own `gopher_screen.c` | yes | no |

gopher was in the original request and was never missed; its module is
named `gopher_*` rather than `m65_*`, which is why a search for the
shared name finds nothing there. The two that do not follow the height
keep their own modules and their own pagers, built for 25 rows.
Reworking either would cost a great deal for a gain nobody asked for, so
neither was touched.

**Verified on hardware (2026-09-22).** mega-ftpc booted, connected to
its fixture, listed a directory and rendered correctly with its keys
row at 24, so the shared module's change breaks nothing. Then the
background key, read straight off `$D020` and `$D021` at `$FFD3020`:

| presses | border and background |
|---|---|
| none | `0606`, blue, as the ROM leaves it |
| one | `0000`, black |
| two | `0202`, red |

The first press goes to black, which is the behaviour. The second
landing on red rather than white is the rest of the rule working: white
is the text colour and the rotation skips it, so 0 goes to 2.

**mega-ntp agrees**, and it is the other client that binds this key.
Booted with the user's permission to sync the machine, it read `0606`,
`0000`, `0202` through the same three presses. Its rows sit where the
shared module puts them, keys at 24 and status at 23, and it synced
correctly: `2026-09-22 13:45:57` against the Mac's 13:45, having
reported the clock `3H59M59S FAST` beforehand. So both clients that
expose the key behave the same, and mega-ntp still works after the
change to the module it links.

**An earlier attempt at this measured nothing**, and is worth recording
because it read like a result. The FTP fixture had not started, so the
client sat at its Host prompt, the `b` went into a text field, and the
colour read `0606` three times over. A reading that does not move across
a state change is not a null result, it is a broken instrument. What
gave it away was having predicted `0000` beforehand and getting no
movement at all, which is the same discipline 5.8 needed.

**mega-ftpc also gained the deploy tool its siblings have** and that its
own notes had been asking the reader to improvise for a long time. A
redeploy no longer empties the bookmarks: the card's disk is kept under
`build/deploy` first and `FTPC.CFG` is carried across, as mega-ntp has
done since 5.3 there.

### 5.10 The engine talks to Libera, and Libera is not one key type (2026-09-22)

The first step of TLS in the MVP was a zero-code experiment: gemini's
own host client, unchanged, pointed at `irc.libera.chat:6697`. **It
completed the handshake.** TLS 1.3, the engine's one suite
`TLS_CHACHA20_POLY1305_SHA256` and one group X25519, all accepted, and
`openssl` confirms that constraint is negotiable there. An engine built
for Gemini talks to a real IRC server. That was the largest unknown in
the whole plan, and it is answered.

**But the signature came back UNCHECKED, scheme `0x0503`.** That is
ECDSA over P-384 with SHA-384, one of the two schemes the engine
advertises only so that such servers still answer, and cannot verify.
In the same minute `openssl` reported `rsa_pss_rsae_sha256`, and the
pin printed was not the RSA-4096 leaf captured in 5.5. So the
round-robin was surveyed one address at a time:

| address | leaf key | chain above it | signs the handshake with |
|---|---|---|---|
| 185.30.166.168, ruthenium | ECDSA P-384 | YE2, Root YE, X2 cross-signed by X1 | `ecdsa_secp384r1_sha384` |
| 193.57.167.220, gallium | RSA-4096 | YR1, Root YR | `rsa_pss_rsae_sha256` |
| 93.158.237.2, tantalum | RSA-4096 | YR1, Root YR | `rsa_pss_rsae_sha256` |

All three OFTC addresses are RSA-2048 over the same YR1 and Root YR.
And Libera's DNS had answered with three *other* addresses an hour
earlier, so its pool is larger than three and the EC share of it is
unknown. The EC chain is kept as `tests/certs/libera_ec_0..3.der`.

**What this means.** A client that verifies RSA only, which is
everything built so far, works on OFTC always and on Libera whenever
DNS lands it on an RSA server. On an EC server the handshake completes
but the signature is unverifiable and `chain_parts` refuses the chain
outright, since it accepts only `sha256WithRSAEncryption`. The
fixtures of 5.5 came from one address; the engine happened to reach
another, which is exactly why the host runs against real servers.

**P-384 would cost:** SHA-384, absent, though it is SHA-512 with other
initial values and a truncated result; a `p384.c` sibling of the
182-line `p256.c` over the same multi-precision layer, which handles
any even limb count; P-384 keys and `ecdsa-with-SHA384` in the X.509
and chain code; `0x0503` in the kit's verifier; and Root YE carried as
a third anchor. On the machine the 384-bit rows would run on the
generic multiplier rather than the tuned 256-bit path, and none of it
could ever be propagated to gemini, whose bank has a few hundred bytes
free. That is a scope decision with a memory budget attached, and it is
the user's to make.

**Until it is made**, the honest options on an EC server are to refuse
with a clear message, or to reconnect and let the round-robin try
again. Trust on first use is not honest here, for the reason 5.5 gives.
The client now says which it met: `chain_verify` returns
`CHAIN_UNSUPPORTED` for an ECDSA-signed chain, distinct from
`CHAIN_MALFORMED`, because "malformed" would be untrue of a chain that
walks perfectly well and is merely beyond this client.

**Changed in gemini's engine to get here**, and propagated, since the
files stay identical to their origin: `tls.c` hands the kit every
certificate of the chain where it kept only the leaf, and the kit gained
`tk_cert_read` so a verifier can walk the store. Gemini's behaviour is
unchanged, proven rather than assumed: its gate ran the same 258 checks
before and after, its client built 84 bytes smaller for the condition
that went, and its host client still fetches geminiprotocol.net with the
signature verified and the pin reported. `chain_split` finds each
certificate in the concatenated store from its own DER header, so no
table of offsets is kept anywhere.

### 5.11 IRC over TLS, end to end on the host (2026-09-22)

The whole stack against a real server, nothing near the MEGA65 yet:

    build/host/tls_irc_host irc.libera.chat --ip 193.57.167.220

- the handshake completes and the server's signature over it verifies,
  `rsa_pss_rsae_sha256` under an RSA-4096 leaf;
- three certificates, 4527 bytes, found in the kit's store from their
  own headers;
- the chain verifies to a carried anchor, ISRG Root YR: the leaf under
  YR1's RSA-2048, YR1 under Root YR's RSA-4096, the leaf's SAN covering
  `irc.libera.chat`, every certificate in date;
- NICK and USER go over it, and `001 Welcome to the Libera.Chat Internet
  Relay Chat Network m65tls` comes back;
- QUIT, and the server's `MODE m65tls :+Ziw`, the Z being the mark
  Libera sets on a TLS connection.

Exit 0. 160 host checks pass across the three suites, the store split
included. Everything decision 2 asked for is now real on the host.

**The EC server refuses cleanly.** Against 185.30.166.168 the handshake
completes with the signature UNCHECKED, four certificates arrive, and
the client does not register. It reports the chain as signed with a
scheme it cannot verify and stops there; it does not fall through to
trust on first use.

**A test bug worth recording.** The first draft of the store test held a
4543-byte chain in a 4096-byte array. The last 447 bytes ran past the
array into the length field, the splitter walked a corrupted store, and
undefined behaviour let seven checks pass and one fail. It is the same
class of fault as 5.3's counter overflow, an unbounded write past a
fixed buffer, and the test discipline caught it before it went anywhere.

### 5.12 OFTC refused the engine once, and the investigation ran into throttling (2026-09-22)

The engine's one attempt at `irc.oftc.net:6697` ended in alert 40,
handshake_failure, from the server. `tls.c:364` is where an incoming
fatal alert is recorded, so that was the server's decision, not a
timeout.

What followed is recorded as a false trail, because it read like a
result at every step. Constrained `openssl` probes, restricted to the
engine's one suite and one group, failed against OFTC and succeeded
against Libera, and a theory formed: OFTC rejects narrow ClientHellos
as scanner fingerprints. The theory then grew. Restricting to *any*
single suite failed, even the one OFTC picks unconstrained; restricting
to any single group failed too. A server that refuses its own preferred
suite is not following a policy, and that should have been the moment
to doubt the instrument. The next round settled it: a probe offering
two suites and two groups succeeded and **negotiated
`TLS_CHACHA20_POLY1305_SHA256`**, which refutes "OFTC lacks ChaCha20"
outright, and in the same round an *unconstrained* probe, which had
succeeded twice before, failed. OFTC's answers were varying with the
connection count, not with the ClientHello. That is per-source
throttling, which IRC networks do as a matter of course, and by then
this Mac had opened something like twenty connections to OFTC in ten
minutes.

**What is actually known.** Every OFTC address serves an RSA-2048 leaf
over YR1 and Root YR, signing the handshake with `rsa_pss_rsae_sha256`:
exactly the shape the chain code verifies. Its TLS 1.3 accepts
ChaCha20-Poly1305 with X25519, proven by the one constrained handshake
that got through. Nothing in that gives the engine a reason to fail.
The alert 40 it received is therefore unexplained, and may itself have
been the throttle, since the engine's attempt came seconds after
`openssl` had connected from the same address.

**The cold retry, ten minutes later: alert 40 again.** So the alert is
real, and the paragraph above was too generous to the throttle. Both
things are true at once: OFTC throttles a source that connects too
often, which is what made the probes contradict themselves, *and* it
refuses this engine's ClientHello on a cold connection. The narrower
statement that survives is this. OFTC accepts the engine's suite and
group when they are offered alongside others, since the two-suite
two-group probe negotiated ChaCha20 with X25519, and it refuses the
engine's exact hello. Something about the hello's shape, not its
cryptography.

The `openssl` probes could never have isolated that, because `openssl`
lists TLS 1.2 suites in its hello even when restricted, so none of them
sent what the engine sends. The engine offers one suite, one group, no
TLS 1.2 suites at all, and five extensions. The clean instrument is its
own bytes: capture the ClientHello, send exactly that and two
one-change variants, spaced apart. Three connections, not twenty.

**Resolved, with the engine's own bytes.** The ClientHello, 146 bytes,
was captured and sent to OFTC as the control, then three one-change
variants, one connection each, fifty seconds apart:

| hello | reply |
|---|---|
| the engine's own bytes | alert 40 |
| plus a second cipher suite, AES-256-GCM | alert 40 |
| plus `secp256r1` in supported_groups, the key share still x25519 | **a 56-byte handshake message** |
| plus a 32-byte session id, middlebox compatibility mode | alert 40 |

Fifty-six bytes is too short for a ServerHello carrying a key share,
and exactly the size of a HelloRetryRequest. Decoded on one more
connection: its random is the retry-request magic and its `key_share`
names group `0017`, secp256r1, with no share. **OFTC's TLS does not
accept X25519. It asks for a P-256 key share**, and this engine fails
on a retry by design, `TLS_E_HRR`. That explains every earlier result.
A hello whose only group is x25519 gets handshake_failure, which is
what RFC 8446 says a server sends when no group is shared; a hello that
lists P-256 gets a retry asking for it; `openssl` answers a retry
transparently and completes, which is why its two-group probe
succeeded. The suite and the session id were never the question.

**What P-256 ECDH would cost.** Less than P-384, because the curve is
here already: `p256.c` has the Jacobian doubling and addition its
signature verifier uses, and a scalar multiplication is the same loop
without Shamir's trick, some twenty lines exposed as a keygen and an
agreement. The shared secret is the point's 32-byte x coordinate, the
length the key schedule already takes. The engine then either offers
two key shares in every hello, which adds about 71 bytes and a P-256
keygen to every connect including Libera's, or answers the retry with
a P-256 share, which costs Libera nothing and adds a retry path to the
state machine. The kit needs a P-256 keygen and agreement beside the
x25519 ones. On the machine, gemini's P-256 verification takes about
12 s (gemini 5.6), so a keygen and an agreement together sit near 16 s
on top of the connect; once a session, as 5.5 argues. Gemini's shared
`tls.c` would take it behind a compile-time switch, so its own hello
and timing stay exactly as they are; Gemini servers all do x25519.

This is settled and built (5.14): the user chose to add P-256, so OFTC
is now reachable, and to decline P-384, reconnecting past Libera's EC
servers instead.

### 5.14 P-256 for OFTC, done; P-384 declined for a reconnect (2026-09-22)

The user's decision on 5.10 and 5.12: add P-256 key agreement so OFTC is
reachable, and rather than support P-384, reconnect past Libera's EC
servers, since its round-robin usually lands on RSA.

**P-256 ECDH.** `p256.c` already had the Jacobian doubling and addition
its signature verifier uses; a key generation and an agreement are a
scalar multiplication each, the verifier's loop without Shamir's trick,
about eighty lines. They are `p256_keygen` and `p256_ecdh`, proven on
the host against the cryptography package: two pairs and the secret they
agree, ten checks. All of it, and everything below, is behind a
`TLS_P256` compile switch, so gemini compiles none of it.

**The engine.** The ClientHello now offers x25519 and secp256r1. A
HelloRetryRequest, which is how a server that will not take x25519 asks
for P-256 (5.12), is answered with a second ClientHello carrying a
P-256 share, over the RFC 8446 4.4.1 synthetic-message-hash transcript.
`tk_keyshare_p256` and `tk_keys_handshake_p256` sit beside the x25519
pair in the kit. The shared secret is the point's 32-byte x coordinate,
the length the key schedule already takes, so the schedule is untouched.

**gemini is unchanged, proven three ways.** Its gate is the same 258
checks; its ClientHello is byte-identical outside the random and the
share; and the generated assembly of `tls.c` off the switch differs
from the committed original only in 38 `fail()` `__LINE__` constants,
shifted by the `#else` branches that hold the original `tls_start` and
`server_hello` verbatim, with no structural change. tls_host still
fetches geminiprotocol.net with the signature verified.

**End to end on the host, against the real servers:**

| server | key exchange | chain | result |
|---|---|---|---|
| OFTC, weber | retry request, then P-256 | RSA-2048 to Root YR | `001`, exit 0 |
| Libera, gallium | x25519 | RSA-4096 to Root YR | `001`, exit 0 |
| Libera, ruthenium | x25519 | ECDSA P-384 | refused, exit 2 |
| Libera, by name | round-robin to tantalum | RSA-4096 | `001`, exit 0 |

OFTC over TLS registers where it could not before. Libera's RSA path is
untouched. The EC server is refused as `CHAIN_UNSUPPORTED`, and the host
client, when no address is pinned, reconnects up to three times so the
round-robin can hand out an RSA server; the client on the machine will
do the same. 174 host checks pass, the P-256 suite among them.

**Still only on the host.** The P-256 ECDH has not been compiled for the
6502 yet: mega-irc builds no bank, so the engine and curve link only
into the host tools so far. On the machine, gemini's P-256 verification
takes about 12 s (gemini 5.6), so a keygen and an agreement together
will sit near 16 s on an OFTC connect, once a session. That is the
machine's to measure when the bank is built.

Two lessons, both already in this file in other forms. A result that
contradicts itself across runs is a broken instrument, not a subtle
server (5.9). And a theory that needs a bigger exception every round is
the instrument talking (5.3, 5.8).

### 5.13 The bank has room for the chain, where gemini keeps its renderer (2026-09-22)

5.6 left two problems for the machine: `crypto_equal` and
`crypto_yield` living in `curve25519.c`, so that any RSA seemed to drag
curve25519 and sha512 in behind it; and `chain.c`'s 1032 bytes of
statics on top of the multi-precision state, against a gemini bank with
a few hundred bytes free. Both are smaller than they looked, measured
from gemini's bank map rather than assumed.

**The first is no problem at all.** `curve25519.c` is already in the
bank, because X25519 is the handshake's one group; it was never RSA
that brought it. So is `sha512.c`, which curve25519 needs. The only
code the IRC client adds to a bank is `pkcs1.c`, `chain.c`, `roots.c`
and a `tk_cert_read` entry in the kit.

**The second has an answer in the map.** Gemini's bank is tight in its
main window, 331 bytes below the stack, but its top window at `$E000`
holds the gemtext renderer:

| object | bytes |
|---|---|
| `render.o` | 4209 |
| `gemtext.o` | 1475 |
| `.text.hi` in all | 5668 |
| `.bss.hi` | 351 |

An IRC client has no gemtext to render. That is about 5.7 KB of the top
window free for the chain code and its buffers, several times what they
need. Where the chain lives on the machine is therefore settled: the
top window, in the renderer's place.

An estimate until compiled, as ever. The machine measures it.

### 5.15 The bank on the machine: what fit, and how (2026-09-22)

5.13 said the top window had "several times what they need". It had
measured the chain on the host. Compiled for the 6502, `chain.c` was
**8,307 bytes** of code, about four times its host size, against a
window of 6,144 bytes less the 312 the zero-page swap takes. The rest
of this finding is what it took to fit, measured by linking rather than
by adding columns; the trial links live in the scratch directory and the
final numbers are in `build/bank/crypto.map`.

**Two trial links, both over.** gemini's bank without its renderer,
plus the chain, PKCS#1, the roots and the P-256 agreement, with the
`ram` region widened so the link would finish and the map would say
where it ended: everything in the main region under LTO overran `$BE00`
by 12,077 bytes (LTO inlines the chain into one 4,942-byte
`chain_verify`); the chain compiled apart into the window was 9,624
bytes against 5,832, with the main region still 4,692 over. And there
is no second bank to give it: PLATFORM.md's map has banks 2 and 3 as
the ROM and 4 and 5 as mega-net's.

**What bank 1 costs in this client's shape.** A third trial, of bank 1
alone: no renderer, no ECDSA verify, plus the P-256 pair and PKCS#1.
That links with **868 bytes free** in the main region and 5,480 in the
window. The ECDSA verifier goes because a leaf with an EC key sits in an
EC chain, which this client refuses (5.10, 5.14): the verifier could
only ever have run for a connection about to be dropped, and it and its
helpers were about 3 KB. So the whole of the chain had to fit the
window, with 868 bytes of slack beside it.

**A rewrite for size bought 9 %.** The theory was that passing a
`(read, ctx, len)` triple through every call and seven arguments into
the element walker cost the bytes, each argument being a soft-stack push
at every site. Selecting one certificate at a time and walking it with
a cursor struct took the file from 8,307 to 7,573. Wrong theory; the
compiler's code is simply about 35 bytes a line. What the per-function
sizes showed instead was a seam:

| function | bytes | needs |
|---|---|---|
| `chain_host_matches` | 1,552 | the DER, a host name |
| `chain_valid_at`, `time12`, `tbs_field` | 1,183 | the DER, a clock |
| `chain_verify_link`, `chain_parts`, `chain_verify`, `chain_tbs_hash` | 3,282 | the DER, SHA-256, RSA |

The name and the dates are policy: no key, no arithmetic, and the clock
they compare against is the client's. The signatures and the anchor are
arithmetic and belong beside the keys. So the check is now two halves
in two images over one DER walker: `der.c` (1,511 bytes; both images),
`chain.c` (3,115; the bank's window), `policy.c` (2,302; the client).
The host suite runs both halves in sequence, as the machine does, and
its 76 checks pass as before; the host client does the same. The window
then held `der.o`, `chain.o`, `roots.o` and the entry glue in 4,483
bytes, later 4,537 with `chain_split` moved into `der.c` because the
client's half starts with it too.

**The first real link was 1,658 bytes over, and the map said why.** A
symbol-by-symbol diff against the trial: `x509_key_of` had gone from
1,308 bytes to 2,484 and its helpers `element`, `integer` and `oid_is`
had grown 457 between them. In the trial every call passed the bank's
one reader as a constant and LTO kept one copy specialised for it; the
window's call went through the `chain_ref`'s pointer, a runtime value,
so the link kept a second, generic copy. The cure is two wrappers in
`api.c`, `ck_key_of` and `ck_spki_hash`, which pass the constant, and
which `chain.c` calls under `CHAIN_BANK` (`chain_bank.h`): 1,243 bytes
back. The remaining 415 over was exactly the region's `.noinit`, which
is llvm-mos's *static stack* (`.noinit..Lstatic_stack`, 416 bytes: the
locals of functions that never recurse), plain data reached by absolute
address, so it now lives in the window as the zero-page swap already
does. For margin, two cuts: the pin entry is a stub, since the
trust-on-first-use fallback that needs it is not in the MVP and gemini
keeps the working one; and the zeros the kit promises past the store's
end are written by the client's kit rather than the bank's DMA.

**The result.** CRYPTO 34,461 bytes with **332 free** below the bank's
stack (gemini's has 331); CHAIN 4,537 with about 800 free in the
window. The chain's 1 KB of copies costs nothing: the issuer's modulus
sits in the lower half of `mp_scratch` and the child's signature in the
upper, which is safe because `pkcs1.c` copies the modulus out with
`mp_init` before it reads the signature into the lower half, and writes
the upper only after that; the exponent is in `small` and the issuer's
key at `stage + 130`, where verify keeps its own. The client is 35,608
bytes with 4,232 bytes of headroom above its data, so the MVP's windows
and editor will be finding their bytes.

**The vectors under the colour RAM, in 80x50.** The bank's interrupt
vectors are at CPU `$FFFA` while it is mapped, which is physical
`$1FFFA`: the colour-RAM mirror, cells 2042 to 2047. With 25 rows the
display stops at cell 2000 and the vectors survive, which is how ssh
5.7 placed them. With 50 rows those cells are row 25, columns 42 to 47,
and every colour write there, which the chat's scrolling does, wipes the
vectors. So the prologue writes them on every entry and keeps the six
colour bytes to put back on exit, and they are six bytes of `$0F`: all
three vectors point at `$0F0F` in bank 0, the ROM's old screen page,
where INIT keeps an RTI; as a colour, `$0F` is a quiet light grey. That
is thirty-eight bytes of prologue for a case gemini never had.

**A trade made knowingly.** Decrypted application data reaches the
client as it is decrypted, before the record's tag has been checked. A
forged record ends the connection when its tag fails, so the most a
forger gets is one record's lines on the screen before the link drops;
holding every record back would cost a buffer the size of the largest
record, 16 KB, which this client has nowhere to put. `conn.c` says so.

### 5.16 IRC over TLS on the MEGA65 (2026-09-22)

The client, the bank and the chain, all on the hardware, the same
afternoon the bank first linked. The disk carries `irc`, `meganet`,
`crypto` and `chain`; the client loads the two bank images, samples
entropy for a second, checks the clock, and then connects. The screen
was dumped every five seconds during a connect, and each dump stalls
the CPU (gemini 5.11), so the seconds below are at most a little high.

| run | server | rows | verified in | to the join |
|---|---|---|---|---|
| Libera | cadmium | 25 | (scrolled off) | 20 s |
| Libera | tungsten | 25 | **19 s** | 20 s |
| Libera | (80x50) | 50 | **20 s** | 20 s |
| OFTC | via P-256 retry | 25 | **26 s** | 25 s |
| the fixture, plain | 192.168.1.232:6667 | 25 | none | 5 s |

"Verified in" is the bank's own count of frames spent on the key
agreement, the RSA-PSS signature and the two chain links; it is shown
on the title row, where the MOTD cannot scroll it away. "To the join"
counts from the last prompt and includes DNS, the connection, the
handshake's round trips and the registration. In each case the chain
was three certificates to ISRG Root YR.

**OFTC's P-256 costs six or seven seconds, not sixteen.** The engine
offers X25519 first, so both the X25519 share and, after the
HelloRetryRequest, the P-256 pair are computed; the difference between
26 s and Libera's 19 to 20 s is the P-256 key generation and agreement
together. 5.14's sixteen seconds was borrowed from gemini's P-256
*verify*, which is two scalar multiplications and an inversion; the
agreement is cheaper than that.

**80x50 held with the bank in use.** Fifty-three rows in the dump, the
counts on row 47, a line sent and echoed, some forty bank calls while
the chat scrolled colour through row 25, where the vectors live: the
handling of 5.15 works, or at least nothing pressed RESTORE to prove it
fully. The plaintext path is unchanged.

**One connect in six failed and its reason was lost.** The run after
the first success came back at the "any key to leave" prompt; the
driver script typed a line before it looked, the keystroke took the
client to BASIC, and the chat rows with the reason went with it. Not
reproduced in the next five connects, on either network. The likely
cause is 5.10's: Libera's resolver answer put an ECDSA server first, and
mega-net's resolver hands back one address, so "try another server"
may have tried the same one three times. Open until the next failure
is read; the script now looks before it types.

**#mega65 on Libera is a real channel with people in it.** The NAMES
reply listed two. Two test lines went there, harmless ones, and that
is two more than a test run should send: the machine recipe now uses a
channel of its own.

### 5.17 A window under the KERNAL, and the views on the machine (2026-09-22)

The MVP's windows, scrollback and slash commands (section 2, the
review's decisions) cost what the review said they would, about 7 KB,
against 4.2 KB of headroom: the first link was 3,383 bytes past `$D000`
with the 1 KB stack floor still to find. The small cuts were the ones
the family already prescribes and the map named: `__udivmodsi4` and
`__udivsi3`, 794 bytes of 32-bit division from the counts row and from
the decimal writer (the uptime is now kept as digits and counted, and
`ui_cat_num` subtracts powers of ten, gemini 5.8); and 1.9 KB of
buffers moved to the low RAM the family map leaves to clients
(`lowram.h`: the receive and send buffers, the line, the typed line,
the folded line, the clock, the row being logged). Not enough.

**The lever was 8 KB nobody had claimed.** Once `m65_own_vectors` has
mapped the KERNAL out, which is the whole run, bank 0's `$E000-$FEFF` is
plain RAM below mega-net's vector stub at `$FF00`, and PLATFORM.md's map
had no row for it. A PRG cannot load there, the KERNAL being in the way
at LOAD time, so it is a third disk image, HIGH, loaded by `cbmdos_load`
through DMA in `bank_boot` like CRYPTO and CHAIN, and zeroed for its
`.bss` since crt0 knows only the region it was told about. The client
links with a script of its own, `src/m65/irc.ld`: the target's
`link.ld` with the region added and the objects named, compiled apart
without LTO so they can be, and `OUTPUT_FORMAT { SHORT FULL(ram)
TRIM(hi) }` so one link makes both files. The first choice of objects,
`der`, `policy`, `view` and `log`, overflowed the window by 1,210 bytes:
`view.o` is 3,337 bytes compiled apart, and it calls the screen and the
UI at every turn, which is what LTO is for. `der`, `policy` and `log`
went, 5,801 bytes with 1.9 KB spare, and `view` stayed. The result:
**`irc.prg` 38,365 bytes with 3,162 bytes of headroom, HIGH 5,801, and
the disk now carries five files.**

**The ordering trap, found the first time it could be.** `log_init` was
called before `bank_boot`, which loads HIGH: the call landed in unloaded
memory and the client hung with a blank status row, four boots in a
row. Nothing in HIGH may run before `bank_boot` returns; `main` says so
where it calls the first of it. A crash with no message is the shape of
this mistake (gemini 5.6 found the same shape from a different cause).

**The views, on the fixture.** The bar on row 0 with the showing view in
brackets and unread views starred; the join with its names line; a
40-line flood, each line wrapped at 79 with its continuation indented,
as the spike drew them; three cursor-ups to `back 3` on the counts row
and HOME to live again; `/join #second` onto F5, a line into it, `/part`
back to the status view with the first channel starred by the flood
that arrived meanwhile; `/quit` to BASIC. The function keys and
MEGA+cursor have no escape in the driver, so they ran only through the
same `view_show` and `view_scroll` the commands use; a press by hand is
owed. The slash parser refuses what it does not know rather than
sending it as text (section 2).

### 5.18 Bookmarks, and the last 27 bytes (2026-09-22)

The review's fourth item, in the family's shape: `IRC.CFG` on the boot
disk, a magic line `IRC1` and five lines an entry, host, port, TLS,
nick and the channels to join, comma-separated; `src/m65/marks.c` is
mega-ftpc's `marks.c` with the fifth field, the text staged at bank 1
`$11800`, which nothing else of this client's uses. At start, if the
file has entries, they are listed and a number connects, RETURN alone
goes on to the prompts, `dN` deletes; after the prompts, "Save as a
bookmark (y/n)". No passwords (section 2). And `tools/deploy.py` in the
same commit, mega-ftpc's with the file name changed, because a plain
`put_d81` would now wipe what the user saved: from here on a redeploy
of this client goes through it and nothing else.

**On the machine, three boots.** The tool's first run found an
`IRC.D81` on the card without an `IRC.CFG` and said so; the prompts,
`y` to the save, the join. The second boot showed the picker,
`1  192.168.1.232 6667 plain  mega65  #mega65-test`, and `1` joined.
The tool's second run reported `keeping irc.cfg`; the third boot listed
the entry again; `d1` emptied the list and the prompts followed.

**The bytes, again.** The bookmarks cost 2.4 KB of module and, in the
main region, the CBM DOS write path nothing had referenced before
(`cbmdos_delete`, `cbmdos_close`, `f011_write_block`: 1.5 KB). The
HIGH window took `marks.o` and overflowed by 393 bytes; the main region
overflowed by 1,049 and then needed its 1 KB floor. Found, in order:
`$0800-$0FFF`, the ROM's old screen page, which this client vacated
when its screen went to `$10000` (5.7) and which only the bank's one
RTI byte at `$0F0F` uses, took the TLS engine's state, the line being
composed, the view table, the failure text and the row scratch (1.7 KB
out of `.bss`; `lowram.h` has the table, and nothing crosses `$0F0F`);
`marks.c`'s and `policy.c`'s work buffers borrow the stream's line
buffer, idle whenever they run; `log_draw` was 949 bytes of 32-bit
arithmetic and `redraw` 1,233, and 16-bit counts, which an 8192-row
ring fits, made them 40 % smaller; the longest strings were shortened.
That left the main region **27 bytes** under the floor. The channel
list, which has no initialiser, moved to the page's last 64 bytes, and
the CTCP VERSION reply went. `irc.prg` 41,584 bytes with 1,658 of
headroom; HIGH 7,699 with 237 spare; the bank untouched.

**What the MVP's remaining features will meet.** The main region is
full to within a page and the HIGH window to within 237 bytes. NickServ
is a `/msg`, which exists; an editor with a cursor and history is code,
and it will have to pay for itself: candidates are the diagnostic
counts row (700 bytes), `m65_fold_utf8`'s tables, and conio's
`escapeCode` (765 bytes) if the shared screen module's `cputs` can be
replaced by the DMA it already uses for rows.

### 5.19 The page that was not blank (2026-09-22)

The user's photograph, at the machine: the "Channels:" prompt with its
cursor at the right-hand edge. The channel list had moved to `$0F80`
in the ROM's old screen page (5.18), and a screen page arrives full of
screen-code spaces, `$20`, not zeros; the prompt, which keeps whatever
default it finds in its buffer, found sixty-three blanks. Every other
buffer in that page is written before it is read, and the TLS engine
sets each field it depends on in `tls_start` (gemini overlays that
state on a reused buffer for the same reason), so this was the one.
The page is now cleared whole at the top of `main`, before the bank's
INIT writes its RTI into it, so nothing placed there later inherits
the ROM's spaces. Found by a person looking at a screen, not by the
driver, whose dumps had read the prompt row as fine because spaces
dump as spaces.

### 5.20 Two bugs the user found, and what my instruments could not (2026-09-22)

**The colour key changed one row.** MEGA-F cycled the text colour and
only the counts row followed. The cause is in the shared screen module:
`apply_colours()` calls conio's `textcolor()`, which decides the colour
of what is written *next*, and conio writes colour as it writes
characters. The counts row is rewritten every second, so it changed; the
chat rows, whose colour RAM was written when they were drawn, did not.
The fix is a repaint: `m65_screen_cycle_text_colour()` now fills the
whole screen's colour RAM with the new colour, which is what "all text
colours the same, at all times" means. It is in the family's shared
`m65_screen.c` and propagated byte-identical to mega-ntp and mega-ftpc
(5.9); both still build. In 80x50 the fill writes through cells
2000-2047, which are also the bank's interrupt vectors at `$1FFFA`
(5.15) -- safe, because the bank's prologue rewrites them on every entry
and a keypress is never inside a bank call.

**"The shortcut keys stopped responding after I joined three
channels."** Three causes, none of them about three channels:

1. The minute between failed reconnects was `wait_frames()`, which
   polled the network and **read no key at all** -- RUN/STOP included.
   On Libera, where a reconnect meets the ECDSA servers (5.10), that
   cycle repeats: a minute deaf, three failed connects, a minute deaf.
   It is now `wait_secs()`, which reads the keyboard: RUN/STOP leaves,
   any other key retries at once, and the message says so.
2. The whole reconnection was told in the status view while the user
   was watching a channel, so nothing appeared to happen. A dead link
   now switches to the status view, where its story is.
3. A function key for a view that does not exist was swallowed in
   silence. It now says "no view there yet".

**The experiment that proved nothing.** To reproduce this I joined a
channel and scrolled with the cursor keys, which the driver can send:
no "back N" appeared, and for a moment that looked like the bug. It was
not. The channel held five rows and the chat area is twenty, so
`view_scroll` correctly clamped to zero. A test whose subject cannot
move cannot fail, and the family has met that shape before (a check
that cannot fail, 5.8). The rerun needs a view with more rows than the
screen.

**And what the driver cannot do at all.** `m65 -t` has escapes for
RETURN, the cursor keys, HOME and RUN/STOP, and none for F1-F8 or for
MEGA+letter. The function keys and the colour keys cannot be pressed
from here: they were exercised through the same `view_show` and
`m65_screen_cycle_text_colour` that `/join` and the tests reach, and
the keypress itself is the user's to confirm. Their screen is an
instrument this one does not have -- the cursor at the right-hand edge
in 5.19 was invisible to a text dump, where spaces dump as spaces.

### 5.21 Retrying a name that is many servers (2026-09-22)

`irc.libera.chat` is a rotation: a different address each time, and
about half of them serve the ECDSA chain this client cannot verify
(5.10), so it drops the connection and dials again. Three attempts in
a row was both too few to get past a run of them and fast enough to
look like abuse -- the user's screen showed two ECDSA refusals and then
"no answer from the server", which is Libera's throttle, not a third
EC server.

So: **five attempts, three seconds apart**, and a failure is retried
when it is this *server's* and not this *name's*. `conn.c` classifies
that (`conn_retry`): a refusal, a silence or a hang-up during the
handshake is worth another address; a TLS alert is the server's
considered no; an unverifiable chain is worth another server, which is
what it was already. The pause is what the throttle wants and is not
dead time -- `wait_secs` reads the keyboard through it.

**The hardware found the hole in that.** RUN/STOP during a pause set
"cancelled", abandoned *that attempt*, and returned to the caller,
which then announced a minute's wait and waited -- indistinguishable
from being ignored, which is the very complaint of 5.20. A cancel now
propagates: `conn_cancelled` from the handshake, `quitting` from the
pause, and the reconnect loop leaves rather than waiting. Verified on
the machine: RUN/STOP inside a retry pause returns to BASIC.

What this does not fix is a network whose only server is ECDSA: no
number of retries reaches one. That is 5.22.

### 5.22 P-384 proven on the host, and what it would cost the machine (2026-09-22)

The user asked for ECDSA support after the retry tuning. The host half
is done and passes; the machine half is a design problem with a number
attached, and the number is the point of this finding.

**What the chain actually is.** Every certificate Libera's EC servers
send is P-384 signed `ecdsa-with-SHA384`: the leaf under Let's
Encrypt's YE2, YE2 under ISRG's Root YE, Root YE under **ISRG Root X2**
(which X1 cross-signs with RSA, so that last link is the kind this
client already verifies). Anchoring at X2 is therefore three elliptic
verifications, and the handshake's own CertificateVerify -- also P-384,
the leaf's key being P-384 -- is a fourth. The ClientHello has
advertised `ecdsa_secp384r1_sha384` all along so that such servers
answer at all, so nothing in the handshake needs changing.

**What was written.** `src/crypto/sha384.c`, a new file because
`sha512.c` is the Gemini client's and stays byte-identical: SHA-384 is
its compression with another initial value and a truncation.
`src/crypto/p384.c`, `p256.c`'s arithmetic on 24-limb numbers -- the
same Jacobian doubling and addition, the same Shamir loop.
`tools/gen_p384.py` derives the five curve constants and refuses to
print them unless G is on the curve and nG is the point at infinity,
which pins all five. `tools/gen_ec384_vectors.py` turns the real chain
into test vectors and verifies each with the cryptography package
before printing it, so a failure in the C is the C's.
`tests/test_ec384.c`: **15 checks, 0 failed** -- SHA-384 against FIPS
180-4's own examples and across a block boundary, all three links
verified, and a flipped bit in the digest, in r, in s, an off-curve
key, r = 0, s = 0 and a real signature under the wrong key all refused.

**What it costs, measured rather than guessed.**

| | bytes |
|---|---|
| `p384.c` code | 4,427 |
| `p384.c` constants | 240 |
| `p384.c` scratch (`.bss`) | 1,488 |
| `sha384.c` code and constants | 476 |
| **wanted** | **~6.6 KB** |
| free in the bank's main region | 332 |
| free in the bank's top window | ~237 |
| free in the client | ~1,400 |

SHA-512's `compress` is already in the bank (2,749 bytes, for
curve25519), so SHA-384 rides on it nearly free; the rest does not fit
anywhere, and the scratch is its own problem -- P-384 wants 744 limbs
and `mp_scratch` holds 512.

**And what it costs in time.** On the host, with real signatures on
both curves, a P-384 verification is **3.50x** a P-256 one. That
matches the arithmetic: about 1.5 times as many field multiplications
(384 doublings against 256), each 2.25 times costlier (24 limbs
against 16). This machine did a P-256 verification in about 10 s
(gemini 5.6) **on a tuned 16-limb Montgomery row** (`mp256_m65.S`);
P-384 would fall back to the general 32-bit row, so 35 s is the floor
and the truth is above it. Three links and a CertificateVerify is
**two and a half minutes at best**. The client sends its Finished
before verifying, so that runs against the server's registration timer
rather than its handshake timer (gemini 5.6) -- but registration timers
are about a minute, and this would not make it.

**So the honest position:** the arithmetic is done and proven; putting
it on the machine needs (a) the EC code in a swappable window image in
place of CHAIN, which is possible because the chain walk can collect
its three (hash, key, r, s) triples -- 720 bytes -- before any
verifying starts, so the two are never needed at once; (b) a trimmed
scratch laid over `mp_scratch`, which is idle during an EC
verification because no RSA is running; (c) most likely a tuned 24-limb
row in assembly, as `mp256_m65.S` is for P-256, to buy back the
general row's penalty and bring the connect under a minute; and (d)
the P-384 key and signature parsing, which must go in this client's own
files since `x509.c` is the Gemini client's. That is a 5.15-scale piece
of work whose success is not certain, and the alternative -- 5.21's
retry, which reaches Libera reliably and leaves an EC-only network
unreachable -- is already in.

### 5.23 The function keys were dead, and the record had said why all along (2026-09-22)

The user: F1, F3, F5 and F7 do nothing, so the last channel joined is
the only one reachable.

**The cause was a fact I asserted twice without measuring.** I took the
C64's PETSCII table -- `$85` to `$8C`, interleaved F1 F3 F5 F7 F2 F4 F6
F8 -- and wrote it into the code, into section 2's decision row and
into 5.17, calling it "the order the keyboard delivers them". It is
not. That table is what the KERNAL's GETIN hands back; this client owns
the vectors and reads the hardware queue at `$D610`, which gives
**`$F1` to `$FE`, F1 to F14, in label order**. `PLATFORM.md` item 15
has said exactly that since before this client existed, and
`ssh/src/ui.h` carries `#define KEY_F1 0xf1 /* F1..F14 are $F1..$FE */`.
The cursor and HOME codes I had were right, which is why scrolling
worked and hid the shape of the mistake.

So the order the user asked for -- F1 F3 F5 F7 first, so that four
views need no SHIFT -- is a mapping this client makes, not one the
keyboard offers. The codes being sequential, the unshifted keys are the
even offsets: view = n/2 for even n, 4 + n/2 for odd.

**The driver can press them after all.** `m65 -t $'\xf3'` puts `$F3`
into the queue and the client reads it as F3: the function keys were
tested here rather than handed to the user to try. What the driver
still cannot do is set the modifier bits in `$D611`, so MEGA+letter
remains the user's to press -- the correction to 5.20's rule is that
*key codes* are reachable and *modifiers* are not.

**RUN/STOP now has two levels**, as the user asked: in a session it
ends that session and returns to the first screen, where another
bookmark or another server can be chosen; at the first screen it quits
to BASIC, which is the only way out. `main` keeps the one-time work --
the network, the bank, the entropy, the lease -- and calls `session()`
in a loop; `session()` resets the counters, the input, the views and
the logs, so a second connection starts clean. A cancel inside the
connect path (5.21) leaves the session rather than the client.

**It cost 794 bytes and the build refused it**, the 1 KB stack floor
being what it is (gemini 5.6). Found: 345 by retiring the spike's
telemetry -- the counts row kept the view, the uptime, lines in and
out and how far back it is scrolled, and lost the byte count, the PING
count and the reconnect count, which existed for the soak of 5.2 --
and 146 more from over-long strings. **1,044 bytes of headroom.** The
next lever, unused, is `m65_fold_utf8`: 894 bytes to fold accented
characters to their base letters, which a two-line loop mapping them
to `?` would replace.

**Verified on the machine**, all of it: F3 moved the bar from `#two` to
`#one`; RUN/STOP returned to the first screen; a second session
connected, registered and joined with a clean view table; and RUN/STOP
there quit to BASIC.

### 5.24 The rest of the MVP, and a byte war with one wrong prediction in it (2026-09-22)

The MVP's last two items, to the user's choices: history on the plain
cursor keys with the scrollback moved to MEGA and the cursor keys, and
a NickServ password typed each session rather than stored.

**The editor.** The input line had been append-and-backspace since the
spike. It now has a cursor: left and right move it, a character is
inserted where it sits and backspace takes the one before it, and the
cell under it is drawn in reverse -- bit 7 of the screen code -- so the
character stays legible, which an underscore over it would not. The
lines already sent are kept in the attic past the views' eight
megabytes, eight of them, walked with the cursor keys; the program pays
two bytes of state for that and nothing else. And the input line has
the bottom row to itself at last: it shared a row with the status
messages from the spike onwards, so every message wiped what was being
typed, which NOTES.md had called the MVP's job to fix. The keys are
said once into the status view instead of holding a row of their own.

**NickServ.** A masked prompt at the first screen, asked whichever way
the server was chosen, since a bookmark carries no password and never
will (section 2); blank skips it. It goes out on `001` and is never
echoed to a view. `/identify [password]` does it by hand later.

**The byte war, which is the substance of this finding.** The session
loop of 5.23 cost 794 bytes and these features about 600 more, against
a program region that was already full. What was tried, in order:

| lever | bytes | outcome |
|---|---|---|
| the spike's telemetry retired (5.23) | 345 | kept |
| over-long strings (5.23) | 146 | kept |
| conio's `cputs` path replaced by DMA | 199 text | kept, and it made the siblings smaller |
| `escapeCode`, conio's 765-byte buffer | 0 | **the prediction was wrong** |
| `m65_fold_utf8` replaced locally | 894 | kept |

The fourth row is the one worth the space here. `escapeCode` is 765
bytes of `.bss` in every client, and the reasoning was that it exists
to serve `cputs`, so writing the rows by DMA would let the linker drop
it. `cputs` did go; **`escapeCode` did not** -- it is reached
indirectly, so `clrscr` or `conioinit` keeps it alive, and no
disassembly search finds it by absolute address. `.bss` went from 1,810
bytes to 1,841. A hypothesis about the linker, checked only by the
link, and the link said no.

The rewrite was kept regardless, because it stands on its own: every
caller in the family passes an explicit x and y, so the cursor-relative
path was dead weight; a length rather than a terminator retires the `@`
special case, screen code `$00` having made `cputs` stop early; and the
guard against a zero-length DMA, which the hardware reads as 64 KB,
survives. It made **mega-ntp 347 bytes smaller and mega-ftpc 532**, and
the file stays byte-identical across the three.

What finally paid was `m65_fold_utf8`, 894 bytes of tables that fold
accented Latin letters to their base ones. This client now shows `?`
where that one showed `e`. It is the first thing to put back if room
ever appears. **`irc.prg` is 42,150 bytes with 1,066 of headroom.**

**None of this has run on the machine.** The MEGA65's USB adapter
vanished from the Mac before the verification run -- no
`/dev/cu.usbserial-*`, nothing on the USB bus, no process holding
anything, which is 5.1's shape and the user's to clear. The editor, the
history, the masked prompt and the IDENTIFY are proven only by the
compiler and the link map.

**Three instruments measured nothing this session and twice I nearly
read the emptiness as an answer**: a memory probe whose `mon.py`
stderr I had sent to `/dev/null`, a disassembly scan whose `objdump`
stderr I had done the same to, and an `awk` using `strtonum`, which BSD
awk does not have, reporting a 894-byte function as 0 bytes. The rule
this family already has -- an instrument that measures nothing is
broken, not a subtle result -- now has three more cases, and the
remedy in each was to show stderr and check the instrument produced
output before believing its silence.

### 5.25 The history was written off the end of the machine (2026-09-23)

The editor's history stored nothing, and the machine said so plainly:
one cursor-up filled the input line with seventy-six dots, which is
what `m65_ascii_to_screencode` returns for a byte it cannot render.
Floating reads, rendered.

`HIST_BASE` was `$8800000`, chosen as "the attic past the views' eight
megabytes". The attic **is** eight megabytes, `$8000000` to `$87FFFFF`,
and the eight views at a megabyte each fill it exactly, so that address
is one byte past the end of the RAM: the writes went nowhere and the
reads returned float. The room was inside the views' own slots all
along -- an 8192-row ring is 640 KB of each megabyte, leaving 384 KB
spare -- so the history now sits at `$80A0000`, above view 0's ring.

Verified on the machine afterwards, with the fixture: one cursor-up
gives the line sent last, a second gives the one before it, and
cursor-down comes forward again. The rest of 5.24 was verified in the
same run -- the NickServ prompt shows `*******` for what is typed and
the fixture's log has `PRIVMSG NickServ :IDENTIFY hunter2` at 08:42:33;
`helo` with two cursor-lefts and an `l` gives `hello`; and a status
message leaves a real line in the input row untouched, which is the
collision NOTES.md wanted gone.

**The machine can now be power-cycled without the user.** They put it
on a Shelly Plug US G4 (`192.168.1.167`, `auth_en` false, gen-4 RPC:
`Switch.Set?id=0&on=false|true`, `Switch.GetStatus?id=0`). A cycle was
proved on 2026-09-23: 4.5 W to 0.0 W, the `/dev/cu.usbserial-*` nodes
gone with the board and back about a second after power returned, and
BASIC on the screen. That is the unattended cure for the wedged-FTDI
state that cost a day in 5.7, and for the adapter vanishing as it did
in 5.24. **Never cut power during a card write** -- `tools/deploy.py`,
`put_d81`, any `mega65_ftp` transfer -- or the card's filesystem pays
for it; idle at BASIC, or hung in a program, is safe.

**A postscript on RUN/STOP, which was never broken.** Two runs of the
verification ended with `wait_for READY` timing out after the client
was told to quit, and the screen then showed a live session with lines
reading "no channel here", as though the key had been taken for a
RETURN. Watched one press at a time it is exactly right: one RUN/STOP
in a session goes to the first screen, where the picker offers the
saved bookmark, and a second there quits to BASIC. The fault was the
test script's. `wait_secs` returns on *any* key, so a RUN/STOP pressed
while the session was still closing is eaten by the two-second wait
that sends the QUIT, and a fixed `sleep 3` races that window; the
stray presses then landed in the next screen. Drive this client one
key at a time with a dump between, or the transcript will accuse the
program of the harness's timing.

### 5.26 MEGA with the cursor keys cannot scroll on this keyboard (2026-09-23)

The user found MEGA-CRSR dead, and it was, for a reason no document on
hand gives: with MEGA held, the MEGA65's keyboard sends `$11` -- cursor
down -- for **both** directions, marking SHIFT in `$D611` (bit 0)
instead of turning the code into `$91`. The handler took every
MEGA-cursor for "down", scrolled forward from the newest line, and
nothing moved. There is no cursor-up code to fall back on: the
dedicated UP key is SHIFT with down in the matrix (the spare codes 72
to 75 produce nothing), so it collapses the same way. A client could
tell them apart by reading bit 0; this one no longer has the bytes to.

**How that was measured, since the driver could not.** `m65 -t` maps
what it types through a table that knows F1/F3/F5/F7, the cursor keys
and a handful of escapes, and nothing else: no F9-F14, no modifier held
with a key. So the first fix, scrollback on F9 and F11, "verified"
nothing -- `$'\xf9'` fell through the table silently and the counts row
never showed `back`; only the control in the same run, history on plain
cursor-up, which did work, said the keys had not arrived rather than
the code being wrong. But the driver injects by writing up to three
matrix codes to `$D615-$D617`, and `mon.py` can write those registers
itself: any key, and any chord, MEGA (61) held with cursor down (7).
With the CPU halted so nothing pops the queue, `$D610` and `$D611`
read back what the controller produced:

| held | `$D610` | `$D611` |
|---|---|---|
| F1 (4) | `$F1` | 0 |
| F9 (68), F11 (69), F13 (70) | `$F9`, `$FB`, `$FD` | 0 |
| HELP (67) | `$1F` | 0 |
| cursor down (7) | `$11` | 0 |
| SHIFT + down | `$91` | 1 |
| MEGA + down | `$11` | 8 |
| MEGA + SHIFT + down | `$11` | 9 |
| MEGA + A, B, C, Z | `$C1 $C2 $C3 $DA` | 8 |

That is `mega-net/tools/tap.py` now, with `--probe` for the table. Two
instruments had to be fixed on the way: the probe's first run printed
blank rows *including the control*, the signature of a broken
instrument and not of a silent keyboard -- `mon.py` had opened the
wrong FTDI channel, as the memory file already warned, and then the
parse had matched the echoed command instead of the data line. The
control row is what said so each time; put one in every probe.

**The fix.** The scrollback is on F9 (a page back) and F11 (forward),
`$F9`/`$FB`: keys nothing else used, physical on this keyboard, and
pressable from the driver. HOME goes to the newest line -- the help
line said "HOME live", which was jargon, and now says "HOME newest".
Verified on a 73-line view: `back 18`, `back 36`, `back 18`, and gone
after HOME; plain cursor-up still recalled the line last sent. The
colours were measured in the same run rather than looked at: MEGA-F
took the colour RAM across a row from `01` to `02`, so the 5.22 fill of
the whole screen holds, and MEGA-B took `$D020/$D021` from `06 06` to
`00 00`.

**An artifact of the injection, not the client.** Three codes landing
in one instant sometimes let the letter register before MEGA, so MEGA+F
arrived once as `$66` with the MEGA bit and MEGA+B twice as a bare `b`,
which the editor took as text while the border stayed put. A hand puts
MEGA down first and never sees it, and the `$C1-$DA` check stands;
`tap.py` now stages a chord the same way, modifier first, and with that
MEGA+B took `$D020/$D021` `06 06` to `00 00` to `01 01` on two presses
with nothing typed. **One thing is not explained.** Between the first
probe and the staged one the client was found dead: the counts row's
clock stopped at `0:00:13`, every key ignored, the CPU on a `BRK` at
`$FFF3` with the stack at `$01D0`. That was the run of the first,
unstaged `tap.py`, whose bare `os.write` could have sent a short line
and left a virtual key held; the registers read clear afterwards, but
only after the driver had already released everything, so that proves
nothing. Seven chords and a scroll since, staged, and it has not come
back. It is on the list, not off it.

**Bytes.** Twice this went below the floor. A helper that said "nothing
older here" when a scroll could not move cost 347 bytes, more than
twice the estimate; passing it a direction flag instead of a row count,
to "save the arithmetic", cost 69 more, because the compiler had
inlined the helper into `main` and there was no call to save, only a
branch to add. The message went: the counts row's `back N` already
stops growing at the oldest line kept. Headroom is 1,083 bytes.

### 5.33 DCC SEND receive: written, 800 bytes short (2026-09-27)

The user asked for file transfers after the commands, "nice to have".
Written on the `dcc-wip` branch: `src/m65/dcc.c` keeps a SEND offer
(name, the address as one decimal number, port, size), dials the
sender on mega-net's socket 1, writes the file to the boot disk as SEQ
(PRG when the name says .prg) and answers each chunk with the running
total as four big-endian bytes; the IRC link is pumped between chunks.
`netutil.c`'s helpers took a socket index for it. It links with 233
bytes between the data and $D000, where the floor is 1,024: the module
costs about 3.2 KB of ram once the shared disk and socket helpers come
out of line (cbmdos_delete 556, net_connect_s 401, dcc_get 1,266,
dcc_offer 853, number 229, in the map). Reshuffling the HIGH window
was tried and was worse: the disk layer and the bookmarks there with
the commands back under LTO overflowed HIGH by 58 and ram by 524, since
what the commands cost outside LTO (6,063 against about 4,100 under
it) is the biggest penalty of any module and is best paid where the
room is. Not on main. `/dcc` is not a command there; an offer is shown
in the person's view with a note that it cannot be taken.

### 5.32 The commands, the private views, the ignore list and CTCP (2026-09-27)

`src/m65/cmd.c`, in the HIGH window in the room 5.31 made, compiled
apart (an `ircc.h` names what it shares with ircc.c: the text helpers,
the nick, the send buffer, the status macro). A table of words and a
flag byte per command say the shape of its line -- a channel first, a
word, the rest as text, sent as "VERB channel word :text" -- so most
commands are one path; a switch of twenty-four cases with a `sendv`
call each was 2,366 bytes, the table form 2,043. Commands: /join /part
/query /close /msg /me /notice /nick /away /back /whois /names /topic
/invite /kick /mode /op /deop /ignore /clear /identify /raw /quit, and
/help lists them. A private message goes to the person's own view --
/query opens one, the first message from a person opens one too, and
the bar stars it -- else it is shown "(private)" where you are; what
you type in a person's view goes to them and shows as your own "<nick>"
line. /ignore keeps eight nicks in the attic above the sent-line
history (zeroed at start: the attic held noise the first time, and the
list said "full"). CTCP VERSION and PING are answered, ACTION shown,
a DCC offer said in the person's view. WHOIS numerics (301 307 311-319
330 338 671), INVITE, 331, 305/306 go to the view you are in.

Two bugs from the first run: the ignore entry was fetched into `shown`,
which say() composes into, so the name copied itself to the end of the
row; the list uses `tmp` now. And Libera drops private messages from a
fresh unidentified connection without a word, so the probe client that
sends a message, a VERSION, a DCC offer, a notice and an action runs
against `tools/irc_test_server.py`, which relays them. Verified on the
machine against Libera over TLS (whois, away/back, nick, join, topic,
names, mode, deop, kick and invite refused as expected, part, a /msg to
self opening a view on F3, /close, ignore add/list/remove, /clear) and
against the local server with the probe (the view opening starred, the
VERSION reply seen by the probe, /me and text and /notice arriving, the
ignored probe's five lines shown nowhere). HIGH 7,404 of 7,936; 3,458
bytes of headroom.

### 5.31 The chain check moves into the bank whole; P-256 and OFTC go (2026-09-27)

The commands did not fit: 1,149 bytes of headroom, the HIGH window
full, a 2.9 KB overflow on the first attempt. The user chose to drop
OFTC (the one network that needed the P-256 HelloRetryRequest, 5.14)
rather than keep it: OFTC still answers in the clear on 6667. Without
`-DTLS_P256` the bank's ram region has 4.8 KB free and the two P-256
entries stay in the jump table as stubs that answer 0.

That room took `policy.c` (the name and the dates), so `ck_api_chain`
now runs the whole check: {host28, now28, flags u8}, the host copied
into a 64-byte static and the clock into 13, then chain_split, the
policy, then chain_verify -- the free half first, the seconds after, as
before. The client's `chain_check` is one call (`tk_chain(host, now)`),
`store_read`, `tk_cert_read` and `tk_cert_len` are gone, and `der.c`
and `policy.c` left the HIGH window: 4.4 KB free there, 3,135 of
headroom. Proven on the machine: Libera over TLS, the certificates
verified, the MOTD in. Host suite unchanged, 5 for 5.

### 5.30 IRCCRYPTO: one disk for the family (2026-09-25)

The TLS bank is loaded as `IRCCRYPTO` (0.1.3) so this client can share
`Original/net-tools.d81` with the SSH client, whose bank became
`SSHCRYPTO`; see mega-ssh 5.34. Nothing else changed. Booted from the
combined disk on the machine, its bank and window loading first.

### 5.29 The keys close up when a view closes (2026-09-24)

The user, from a screenshot: five views on F1 F3 F5 F7 F2, `/part` in
the F3 one, and the bar read F1 F5 F7 F2 -- a hole where F3 had been,
and every later key one step further from the hand than it needed to
be. The views were slots, the keys were the slots' numbers, and a
closed slot stayed a closed slot.

The fix separates the two. A view's slot never moves, because its
scrollback ring in the attic is addressed by the slot (log.c); what
moves is an order, eight bytes in view.c, mapping a key position to a
slot. Opening a view appends its slot to the order; closing one takes
its position out and the positions after it step down; the bar and the
function keys go through `view_at(pos)`. Everything that routes a line
by name or by slot is untouched, and no ring is copied. 91 bytes;
headroom 1,155.

Verified on the machine against the fixture: three joins gave F3 #one,
F5 #two, F7 #three; a line into #one, then `/part` there, gave F3 #two
and F5 #three with F7 empty; F3 showed #two, F5 #three, F7 said "no
view there yet"; #two's log did not contain #one's line, which is the
check that the ring stayed with its slot; and `/join #four` took F7.
Released as 0.1.2.

### 5.28 A status message is a notice, not a fixture (2026-09-24)

The user, from a screenshot: "joined #c64" sat on the status row until
the next message replaced it, and found it intrusive. Fair -- a notice
that has been read has no business staying.

Two behaviours, both cheap: every status message arms a five-second
countdown that the once-a-second tick runs down and then clears the
row; and an F-key view switch clears it at once, since a notice about
the old view is stale in the new one. The row is cleared by writing it
with two null pieces, which `ui_line` pads to spaces. Nothing waits on
the countdown: the places that show an error and block on "any key" do
so in a blocking wait where the tick does not run, so those messages
stay until answered.

The countdown is armed by a macro in ircc.c that shadows `ui_status`
at all 23 call sites and calls the real function through its
parenthesised name -- one definition, no site touched, and `ui.c`,
which the NTP client shares, unchanged. 171 bytes; headroom 1,246.

Verified on the machine against the fixture: "joined #one" on the row
two seconds after the join, blank at seven; F7 into an empty slot put
"no view there yet" on the row and F1 took it off in under a second.
Released as 0.1.1.

A note for the driver: the chat area is rows 1-20, **row 21 is a
spacer**, the counts row is 22, the status row 23, the input row 24. A
check that reads row 21 for the newest chat line reads the spacer and
learns nothing, which is what this finding's own script did.

### 5.27 Step 3: nick colours and SASL, and where their bytes came from (2026-09-23)

The step's windows came with 5.17; this is the other two, both on the
machine, and the freeze 5.26 left open, closed.

**The freeze first.** Twelve rounds of MEGA-F, MEGA-B, F9, F11, a
letter and RETURN -- 48 chords, 12 lines sent -- with the counts row's
clock read after each: it never stopped. The one freeze seen was the
run of the first, unstaged injector, whose bare `os.write` could leave
a key held; it is that, and off the list.

**Nick colours.** Every line someone speaks -- `<nick>`, `* nick`,
`-nick-` -- paints the name in one of eight colours chosen by a hash of
it (`h = h * 5 + c`, low three bits): cyan, green, yellow, orange,
pink, light green, light blue, light grey; none dark, none the usual
text colours. The words around it keep the text colour, so MEGA-F
still recolours every word at once and the names stay theirs. The
colour has to survive a scroll and a redraw, and the rows live in the
attic, so the span rides with the row: the log's slot grew from 80
bytes to 96, the 80 screen codes then `at`, `len` and `colour`, one
`lcopy` for both. (A separate ring for the spans, tried first, cost a
second address computation and six single-byte far calls: 360 bytes of
the HIGH window, which has 119 to give.) `log_draw` paints the span
over the text colour after copying the row, stepping past the palette
entry that matches the background, so MEGA-B can hide no name; the live
row is drawn through it too now, rather than copied from LOW_ROW, so
one routine knows the colour. The ring is 768 KB of each view's
megabyte and the editor's history moved above it, to `$80C0000`.

Measured, not looked at: with the fixture's flood bot in `#one`, the
colour RAM under `<mega65>` read `01 0D 0D 0D 0D 0D 0D 01`, the row's
own nick in 13 as the hash predicts, and under `<flood>` `01 03 03 03
03 03 01`; after a MEGA-F the text went `02` and the nicks stayed `03`;
a page back with F9, the same.

**SASL PLAIN, with NickServ behind it.** When a password was typed --
the one prompt now reads "Password, SASL or NickServ" -- the client
sends `CAP LS` before `NICK`. A server that lists `sasl` gets `CAP REQ
:sasl`, on its ACK `AUTHENTICATE PLAIN`, on its `+` the account, the
account and the password NUL-separated as base64 in one line, and
`CAP END` after its 903; a server that lists nothing, says NAK, or
answers 904-907 gets `CAP END` and the NickServ IDENTIFY on 001 as
before. No password, no CAP at all. The fixture gained `--sasl
user:pass` and the CAP and AUTHENTICATE handling, decoding with
Python's own base64. On the machine: `hunter2` produced
`AUTHENTICATE bWVnYTY1AG1lZ2E2NQBodW50ZXIy`, which is
`mega65\0mega65\0hunter2`, 903, CAP END, 001, and no IDENTIFY; `wrong`
produced 904, the status line "SASL failed: SASL authentication
failed; NickServ instead", CAP END, 001 and `PRIVMSG NickServ
:IDENTIFY wrong`. Against the fixture run without `--sasl`, whose
`CAP LS` lists nothing: CAP END at once, 001, IDENTIFY. With no
password: no CAP line at all, registered as before.

**Base64 on a 6502.** The textbook encoder -- three bytes to four
characters with shifts by two, four and six -- came to 291 bytes, and a
rewrite of it with single-byte temporaries and no conditional
expressions to 293: the shifts are the cost, a bit per instruction. A
bit-serial encoder, one bit at a time into a six-bit accumulator with
the padding decided by the count of characters out, is 163. The lesson
generalises: on this CPU, count the shifts, not the lines.

**Where the bytes came from.** Nick colours cost 353 in the client and
118 in the window; SASL 1,435 before trimming and about 1,050 after
(the CAP dispatch on a first letter rather than three `same_ci`, one
`cap_end`, the plain `CAP LS`). Against 1,083 of headroom that is two
floors broken, and two things paid for it, neither of them a feature:

1. **conio is gone from the shared screen module.** It had not printed
   through conio since 5.24, but `conioinit()` was still called, and
   linking it dragged in `escapeCode`, 765 bytes of escape-sequence
   table nothing here ever consulted, which 5.24 had predicted would
   go and found would not. What `conioinit` did for this module was
   six pokes and a key-queue flush; what `clrscr` did was two fills
   sized from conio's own idea of the screen. Both are inline now,
   named, and the module is byte-identical in mega-ntp and mega-ftpc
   again, which were rebuilt, deployed, and run: the IRC client in 25
   and 50 rows, mega-ntp syncing the clock, mega-ftpc drawing its host
   screen.

2. **The disk layer's two buffers left the program region.** The
   sector buffer (512) and the second BAM sector (256) were `.bss`, and
   the only writable data of any size the client has. This client
   leaves through the ROM's reset -- `jmp ($fffc)` with the ROM mapped
   back, and the screen after it is the cold-start banner -- so
   whatever BASIC 65 keeps in bank 0 `$1000-$13FF` is rebuilt on the
   way out and the pages are the client's during the run, which no
   runtime component claims (PLATFORM.md's map has `$0334`, `$1600`,
   `$1700` and `$1800`). They are at `$1100` and `$1300`, through
   `F011_BUF_AT` and `BAM2_AT` macros the siblings leave unset, so the
   four other copies of those files are byte-identical still. Proved by
   the boot itself (four images read through the buffer), a bookmark
   saved and one removed (the BAM written), and BASIC afterwards:
   `PRINT 6*7` and `DIR` both answer.

Headroom now **1,417 bytes**; the HIGH window 119.

**ECDSA, the honest state.** 5.22 measured P-384 at about 6.6 KB of
code and scratch and 3.5 P-256 verifies of time, some 2.5 minutes of
arithmetic against a 60-second registration timer. Step 3 has not
changed either number, and the bank and the window have 332 and 119
bytes. It is not a matter of finding bytes: it needs a fourth image
swapped into the window for the verification alone, a 24-limb
assembly row, and a scheme for doing the chain's three verifies inside
the server's patience -- a design, not a session's work. It stays where
5.22 left it, with this client refusing EC chains and dialling the next
address, which Libera makes work.
