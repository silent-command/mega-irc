# mega-irc: notes before a line is written

An assessment of a native MEGA65 IRC client on mega-net, made on
2026-09-15 after the gemini client's step 8, before any code. The
family's conventions apply when this becomes a project: a `LLM.md`,
a `REQUIREMENTS.md` with decisions, plan and numbered findings, the
`m65_*` platform copies, host-first testing, mega-net as a sibling.

## 1. What IRC needs

Plain TCP, line-based text ending `\r\n`, one connection, a few
commands out (`NICK`, `USER`, `JOIN`, `PRIVMSG`, `PART`, `QUIT`), the
server's `PING` answered within a few minutes, and the numerics parsed
on the way in: `001` welcome, `353`/`366` the NAMES list, `372`/`376`
the MOTD, `433` nick in use, `PRIVMSG`/`NOTICE`/`JOIN`/`PART`/`QUIT`
from others. Lines are at most 512 bytes (IRCv3 message tags can add
up to 8 KB; cap or ignore them). Optional layers: `CAP LS/REQ/END` and
SASL PLAIN (base64 of `\0user\0pass`) for account login; NickServ
`IDENTIFY` by `PRIVMSG` otherwise; a CTCP `VERSION` reply. The parser
is smaller than the Gemini status and meta layer.

## 2. Feasibility: yes, and most of the hard parts exist

| Need | Already in the family |
|---|---|
| TCP, DNS, DHCP, the polling discipline | mega-net, unchanged |
| Screen, font, the low-RAM map, disk I/O, boot and exit | the `m65_*` platform copies (mega-ftpc, ssh) |
| A full-screen session reading keys and the network without blocking | the SSH client's session loop (`sshc.c`): exactly IRC's shape |
| Scrollback in attic RAM, a pager over fixed-width rows | gemini's document store (`doc.c`) and pager; gopher's text store |
| UTF-8 folded to the MEGA65 charset | gemini's fold (`render.c`, in its bank) |
| Config on disk: servers, channels, nick | the GEMMARKS / GEMHOSTS pattern (`marks.c`, `pins.c`) |
| Timestamps | mega-ntp's clock set at boot |
| TLS 1.3 for port 6697 | mega-gemini's engine (`src/tls`) and CRYPTO bank, as is |

Genuinely new: the IRC parser; a per-channel scrollback model; a
non-blocking line editor (the clients' `ui_read_line` blocks; the SSH
loop shows the interleaving); the connect, register and keepalive
state machine. The surface is smaller than the Gemini client's: no
URL resolver, no link table, no gemtext.

## 3. The risks

1. **Hours-long TCP sessions.** Nothing in the family has held a
   connection for hours. mega-net's TCP has an RTO and retransmission
   and a stop-and-wait sender; idle behaviour over an evening, the 4 KB
   receive ring under a channel flood or a large NAMES list, and the
   Ethernet controller's quirks (mega-net 5.16) are untested at that
   duration. Test this first, with a throwaway spike: connect, answer
   `PING`, log for three hours, before any UI exists.
2. **TLS and certificate rotation.** Libera and OFTC use Let's
   Encrypt, which rotates keys every 60 to 90 days. Gemini's trust on
   first use by key would say "key CHANGED" at every renewal: right for
   Gemini's self-signed world, wrong here. Either a "trust the new
   key?" prompt or TLS as transport privacy without pinning, the
   signature still verified. The handshake costs 5 to 12 s, once per
   connect (gemini 5.12).
3. **Memory, with TLS in.** The gemini client sits about 1.2 KB from
   its ceiling with the TLS engine inside it. An IRC client's UI is
   lighter, so it should fit better; the same discipline applies (the
   1 KB stack floor in the build, the low-RAM table, the bank's
   window). Plaintext-only would be roomy.
4. **Drawing while typing.** Incoming lines must land without
   disturbing the input row: a layout question on 80x25 or 80x50 (the
   SSH client supports 50 rows), not a technical one.

## 4. How common TLS is for IRC (2026)

The norm on every major network, not yet universally required.

- **Libera.Chat**: 6697 is the documented default; plaintext 6667 still
  works. SASL is accepted over TLS only, and some channels admit only
  TLS-connected users. NickServ `IDENTIFY` by `PRIVMSG` still works
  over plaintext, the password in the clear.
- **OFTC**: the same picture.
- **EFnet, Undernet, QuakeNet, DALnet**: plaintext widely offered, TLS
  on most servers.
- **Self-hosted networks** (ergo, InspIRCd, UnrealIRCd): increasingly
  TLS-only by default; many do not listen on 6667.
- **Bouncers** (ZNC, soju): almost always TLS.

The trend is one way: plaintext listeners are dropped, not added. A
plaintext client is usable today (`#mega65` on Libera over 6667) and
right for the spike and the MVP; TLS is a requirement of the finished
thing, and the cheapest part to add, since the engine and bank exist.

## 5. The order

1. **Spike**, a day or less: plaintext 6667, register, join one
   channel, print lines, answer `PING`, run for hours. Answers risk 1
   before anything is built on it.
2. **MVP**: a server window and one channel window, scrollback in
   attic RAM, a non-blocking editor, NickServ login, config on disk.
3. **Then**: several channel windows, nick colouring, SASL, and TLS by
   dropping in gemini's bank with the certificate policy of risk 2
   decided.

## 6. Where it stands

The spike of section 5 step 1 is built and its server proven; it has
not yet run on the machine (REQUIREMENTS.md 5.1 has the state and the
exact next commands). `LLM.md` has the pickup notes.

## 7. Decisions to settle before the MVP

**All five were settled by the user on 2026-09-22.** REQUIREMENTS.md
section 2 is the authority; this is the summary.

- TLS: **in from the MVP**, not after. This overrode the recommendation
  to defer it, and it removes the plaintext NickServ problem with it.
- Certificate policy: **carry one or two root public keys and verify
  the chain to them**, with trust on first use kept only as the
  fallback when no carried root matches. Not key pinning, which would
  cry wolf at every Let's Encrypt renewal (risk 2).
- Rows: **support both**, detecting at launch whichever mode the
  machine is already in rather than forcing one.
- Scrollback: **lines packed end to end in attic RAM with a
  per-channel index of line offsets.**
- Config: **the family's SEQ-text pattern**, as GEMMARKS and GEMHOSTS.
