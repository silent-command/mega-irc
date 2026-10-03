# mega-irc

An IRC client for the MEGA65 on mega-net: TLS 1.3 to Libera and OFTC
with the certificate chain verified to a carried root, or plaintext to
a server of your own; one channel, printed as it comes, pinged and
answered, counted on a status row. Native mode, llvm-mos, 80x25 or
80x50, `-Oz`, the arithmetic in a bank image as the Gemini client does
it. 0BSD.

The platform modules under `src/platform/` (`m65_*`) are the family's
copies (from `../mega-ntp`, which took them from `../mega-ftp` and
`../mega-ssh`); `src/netutil.c` is the Gemini client's (TCP), `src/ui.c`
the NTP client's; `src/crypto/` and `src/tls/x509.c`, `tls.c`, `keys.c`,
`tlskit.h`, `tlskit_host.c` are the Gemini client's, byte-identical;
`src/bank/trampoline.S`, `trampoline.ld`, `dma.c`, `dma.h` likewise. A
fix to one belongs in all of them. What is new here is in new files.

**Status (2026-09-22): TLS is on the machine, and so are the views.**
The spike is done (5.1 to 5.4); the MVP's TLS half is proven end to end
on the MEGA65 (5.15, 5.16): Libera joined over TLS with the chain
verified to ISRG Root YR in 19 to 20 s of bank arithmetic, in both
screen heights; OFTC via its P-256 retry in 26 s. The bank is gemini's
TLS bank without its renderer and its ECDSA verifier, with the P-256
agreement and, in the top window, the chain's signatures; the client
checks the name and the dates itself. The views are in (5.17): the
status window and up to seven channels on F1 F3 F5 F7 F2 F4 F6 F8,
each with its scrollback as screen rows in attic RAM, F9 and F11 to
scroll it a page at a time and HOME for the newest (5.26), the cursor
keys for the lines already sent (5.24), the slash commands (`/join
/part /quit /nick /msg /me /raw`), MEGA-B and MEGA-F for the colours,
no default channel (section 2); bookmarks in IRC.CFG on the boot disk,
a picker at start, a save prompt after the answers, and
`tools/deploy.py` to carry the file across a redeploy (5.18). Step 3
is in (5.27): every nick in a colour of its own by a hash of the name,
painted from the row's slot in the attic so it survives scrolling and
MEGA-F; and SASL PLAIN when a password is given, with NickServ behind
it for a server that has none (`tools/irc_test_server.py --sasl
user:pass` offers it). The
colour keys repaint the whole screen and no wait is deaf to the
keyboard (5.20); the retries are paced, five attempts three seconds
apart, past Libera's ECDSA servers and its throttle (5.21).

**P-384 is proven on the host, and cannot fit the machine yet (5.22).**
`p384.c` and `sha384.c` verify all three links of the elliptic-curve
chain Libera actually serves, against vectors the cryptography package
signed off first. They cost 4.7 KB of 6502 code and 1,488 bytes of
scratch, against 332 bytes free in the bank, 237 in its window and
1.4 KB in the client, and a verification is 3.5 times a P-256 one,
which was 10 s here. Read 5.22 before starting that work: it is a
5.15-scale exercise plus a timing question, not an afternoon.

The function keys work on `$F1-$FE`, the codes the machine actually
sends, and RUN/STOP has two levels: out of a session to the first
screen, out of that to BASIC (5.23).

**Step 2 is done and has run on the machine (5.25, 5.26).** The editor
has a cursor, insertion and eight lines of history on the plain cursor
keys; the scrollback is a page at a time on F9 and F11 with HOME for
the newest -- it was MEGA with the cursor keys, which this keyboard
cannot deliver (5.26); the input line has the bottom row to itself, so
a status message no longer wipes what is being typed; and NickServ is
a masked prompt asked each session, sent on `001`, never stored and
never echoed.

The room came from `m65_fold_utf8`: this client shows `?` where the
family's fold shows `e`, and that is the first thing to put back if
room appears. The client has **1,417 bytes** of headroom, its HIGH
window 119, the bank 332. Step 3 was paid for by two removals, not
features: the shared screen module no longer links conio at all (765
bytes of `escapeCode` went with it), and the disk layer's sector
buffer and BAM copy live in bank 0 `$1100`/`$1300`, which is safe only
because this client exits through the ROM's reset (5.27, lowram.h).

## Picking this up

- `python3 build.py test` is the gate: four host suites, 174 checks,
  before anything reaches the machine. `python3 build.py` builds the
  bank and the client and refuses on the ssh 5.6 miscompile shape, an
  empty top window, and less than 1 KB of stack headroom.
- The fixture server may still be listening on 6667 from the last
  session (`lsof -nP -iTCP:6667`); if not, start it as 5.1 says. Its
  log is the record of the run: keep it under `build/`.
- The machine is one resource: never two tool calls on the serial
  port at once, and never a screen dump more than once a minute during
  a long run (gemini 5.11); during a connect, every five seconds at
  most, since a dump stalls the CPU and the bank is counting frames.
  If `m65` cannot open the port, look for stale `m65` or `mega65_ftp`
  processes before anything else, then for the device itself
  (`ls /dev/cu.usb*`).
- **That failure is silent, so recognise it by shape.** On 2026-09-22
  an orphaned `m65.osx` from inside `M65Connect.app`, parented to
  launchd with the GUI long closed, held the port for two days.
  `put_d81` then hangs for as long as you let it, while `row` returns
  an empty line and no error at all, because `raw()` discards stderr.
  Two lessons: bound every serial call in a script rather than letting
  one run for ten minutes, and run `m65.osx -S0` directly with stderr
  shown when the driver goes quiet. `lsof` on the device does not
  necessarily name the holder; `ps -Ao pid,etime,command | grep m65`
  does. Beware that `pgrep -f` also matches the wrapper shell running
  your own command. A SIGKILLed `m65.osx` can wedge the FTDI port; send
  TERM first, and a board restart clears the wedge (5.7).
- **Look before you type.** A test script that types into the client
  after a failed connect presses "any key to leave", and the reason on
  the chat rows goes to BASIC with it (5.16). Dump the screen first.
- On the model for this work, asked by the user and answered on
  2026-09-15: the bulk (parsing, screen, editor, config, the fixture)
  is steady engineering with an exact hardware feedback loop and suits
  the smaller model; the larger one earns its cost when a fault is
  layered and intermittent (the Gemini client's 5.13 is the archetype)
  and when the TLS certificate policy is designed. Whichever runs it:
  measure before theorising, bisect by file when the compiler
  surprises, record every finding here.

## Read first

1. This file.
2. `NOTES.md`: the assessment this began from, the risks, the order.
3. `../mega-net/docs/PLATFORM.md`: the machine, the memory map, the
   traps, the tools, the test driver.
4. `REQUIREMENTS.md`: decisions, the plan, the numbered findings; 5.15
   for how the bank fits and 5.16 for what the machine measured.
5. `../mega-gemini/LLM.md` and its REQUIREMENTS 5.4 to 5.8, 5.12:
   how a TLS bank is built, loaded, called and made to fit.

## Commands

```
python3 build.py test         the host suites: the IRC line protocol, the clock, P-256 key agreement, the certificate chain on the real fixtures in tests/certs; the gate for any change
python3 build.py              the bank (build/bank/crypto.bin, chain.bin), the client and bin/IRC.D81
python3 build.py bank         the bank alone, with its map in build/bank/crypto.map (check .text.hi and .bss.hi there after touching crypto.ld: a pattern that matches nothing is silent, gemini 5.8)
python3 build.py hostclient   build/host/tls_irc_host HOST[:PORT] [--ip A.B.C.D] [--nick N]: IRC over TLS on the host against a real server (5.11, 5.14); reaches OFTC via a P-256 retry, reconnects past Libera's EC servers; --ip pins one, since Libera's differ (5.10)
python3 tools/gen_roots.py > src/tls/roots.c     regenerate the carried trust anchors from tests/certs (committed; needs the cryptography package)
python3 tools/gen_p384.py                        the P-384 constants in src/crypto/p384.c, re-derived and checked (G on the curve, nG infinity); pure Python
python3 tools/gen_ec384_vectors.py > tests/ec384vectors.h   the EC chain's links as test vectors, each verified before it is printed (committed; needs the cryptography package)
python3 tools/irc_test_server.py [--port 6667] [--ping 90] [--chat 60] [--names N] [--flood N] [--flood-every S] [--sasl USER:PASS] [--log FILE]
                              the plaintext fixture, on the Mac; it logs every line with the time
```

On the machine, with the driver (the Mac is 192.168.1.232):

```
source ../mega-net/tools/m65lib.sh
python3 tools/deploy.py                            IRC.D81 onto the card, carrying IRC.CFG (the bookmarks) over; the only way to redeploy (5.18)
boot_prg irc.d81 irc 'erver:'                     the prompts, when no bookmark is saved; RETURN accepts each default (irc.libera.chat, 6697, TLS y, mega65); the channels have none and need none
boot_prg irc.d81 irc 'ookmark:'                   the picker, when one is: a number connects, RETURN alone asks, dN deletes
type_line ''; type_line ''; type_line ''; type_line ''; type_line '##mega65-test'; type_line 'n'; type_line ''
                                                  a channel of your own (#mega65 on Libera has people in it, 5.16) or none, then "Save as a bookmark", then the
                                                  password: RETURN for none, else SASL where the server offers it and NickServ where it does not (5.27)
wait_for 'registered' 120                         a TLS connect is about 20 s of arithmetic plus the network; 'joined' when a channel was named
row 0                                             the bar: "[F3 #channel]" showing, others starred when unread; "-- TLS: ... verified in N s" is in the status view (F1)
row 22                                            the showing view, up h:mm:ss, rx and tx lines, "back N" when scrolled (row 47 in 80x50); the byte, PING and reconnect counters went for the bytes (5.23)
row 23; row 24                                    the status row, then the input line, which has the bottom row to itself now (5.24)
type_line 'hello from the MEGA65'                 a PRIVMSG to the showing channel; /join /part /quit /nick /msg /me /raw are commands
type_keys '~U~U'                                  cursor up walks the lines already sent (5.24)
python3 ../mega-net/tools/tap.py 44 44 45         F9 F9 F11: the scrollback, a page at a time, "back N" on row 22; type_keys '~H' is HOME, back to the newest (5.26)
$M65 -t $'\x9d'                                   a cursor-left, to test editing mid-line; $'\x91' is up, $'\xf3' is F3 (5.23)
$M65 -t $'\xf3'                                   F3 as a raw key code: the function keys have no ~ escape (5.23)
type_keys '~C'                                    RUN/STOP: QUIT, and back to the first screen; RUN/STOP there quits to BASIC
```

The driver's `-t` types F1/F3/F5/F7, the cursor keys, HOME, RETURN,
DEL and RUN/STOP, and nothing else: no F9-F14, no MEGA or CTRL held
with a key. `mega-net/tools/tap.py` presses any key by matrix code,
alone or as a chord, through the same `$D615` registers the driver
uses (`tap.py 44` is F9, `tap.py '3d 15'` is MEGA+F), and `tap.py
--probe` reads back what the keyboard queued for it; that is how the
scrollback and the colours were verified (5.26).

For the plaintext fixture answer `192.168.1.232`, `6667`, `n`. For
80x50, `boot_prg` cannot be used: `stage_d81 irc.d81` first (the disks
live in `net-tools` and nothing mounts from a subdirectory, 2026-09-23),
then reset, `poke53371,49`, `poke53297,peek(53297)or8`, then `mount` and
`run` by hand (5.7), and `unstage_d81 irc.d81` when the run is done.

## What is mine in memory

Bank 0: `$0800-$0FFF` the ROM's old screen page, this client's data
(the TLS state, the line being sent, the view table, the scratch rows,
the channel list) around the bank's RTI at `$0F0F` (5.15, 5.18);
`$1400-$15FF` and `$1A00-$1FAF` more of its buffers; both in one table,
`src/m65/lowram.h` (addresses, not arrays: sizeof is 2);
`$1700` the bank's trampoline; the client from `$2001` to below `$D000`,
the soft stack growing down from there; `$E000-$FEFF` the HIGH window,
`der`, `policy`, `log` and `marks` compiled apart and loaded by
`bank_boot` (`src/m65/irc.ld`, 5.17), nothing in it callable before
that. Bank 1: the screen at `$10000` (both heights, 5.7), the font at
`$11000`, the bookmark text at `$11800` (5.18), the
TLS bank at `$12000-$1BDFF` with its stack to `$1BFFF` (CRYPTO), the
chain check, the zero-page swap and the static stack at `$1E000-$1F7FF`
(CHAIN, 5.15). Bank 5 `$5E900`: the server's certificate chain, up to
5,888 bytes. Attic `$8000000`: the views' scrollback, 1 MB a view, 80
screen codes a row, 8192 rows kept (`log.c`). `$1FFFA-$1FFFF` is both
the bank's vectors and, in 80x50, row 25's colour: see 5.15 before
touching either.

## Rules

- Every wait bounded on `$D7FA`; mega-net polled from every loop,
  including inside the crypto (the bank yields).
- No `int64_t`; `uint32_t` only in accumulators.
- The host proves the arithmetic; the machine measures it. Nothing goes
  in the bank that has not passed a host suite.
- Files copied from `../mega-gemini` stay identical to their origin;
  what is new here goes in new files. `api.c`, `jumptable.S`,
  `crypto.ld` and the `src/m65/` files are this client's own forks.
- The bank DMAs only into named `.bss` buffers, never into a local
  (gemini 5.5); results larger than A and X go back in the parameter
  block (the frames); nothing returns in Z (trap 2).
- The chain's buffers in the bank are names for the multiplier's
  scratch, `small` and the stage (`chain_bank.h`): idle while a chain
  is checked, and only then. A new entry that runs during one must not
  use them.
- The bank's window objects are compiled apart; a call from them into
  `x509_key_of` goes through `ck_key_of`, with the reader as a constant,
  or the link keeps a second copy (5.15).
- Sizes come from a link map, not from the host build: the 6502 code
  is about four times the host's (5.15).
- No 32-bit division in the client: `__udivmodsi4` is 800 bytes
  (gemini 5.8; 5.17). Keep clocks as digits; write decimals with
  `ui_cat_num`, which subtracts powers of ten.
- Nothing in the HIGH window runs before `bank_boot` has loaded it: a
  call into it earlier hangs with a blank status row (5.17). A module
  goes there only if it is self-contained; one that calls the screen
  and the UI at every turn belongs under LTO in ram.
- Screen patterns lowercase; deploy on `build.py`'s exit status, and
  only through `tools/deploy.py`: a plain `put_d81` wipes IRC.CFG (5.18).
- **Every wait reads the keyboard.** A loop that only polls the network
  is deaf to RUN/STOP, and a minute of that is indistinguishable from a
  crash (5.20). Use `wait_secs`, and let a cancel propagate out to the
  main loop rather than abandoning one attempt (5.21).
- **The driver can send any key code, escape or not**: `m65 -t $'\xf3'`
  puts `$F3` (F3) straight into `$D610`, which is how the function keys
  were finally tested rather than guessed at (5.23). What it cannot do
  is set the modifier bits in `$D611`, so MEGA+letter is still the
  user's to press. Their screen sees what a text dump cannot (5.19).
- **Key codes come from `PLATFORM.md` item 15 and `ssh/src/ui.h`, not
  from the C64.** This client owns the vectors and reads `$D610`: the
  function keys are `$F1-$FE` in label order, not PETSCII's `$85-$8C`.
  Asserting that table from memory shipped the keys dead twice (5.23).
- A buffer borrowed by time (`marks.c` and `policy.c` use the stream's
  line buffer) is safe only while its owner is idle: say when, next to
  the borrowing.
- **Take that exit status without a pipeline.** This shell is zsh, where
  `PIPESTATUS` is spelled `pipestatus` and is indexed from 1, so the
  familiar `rc=${PIPESTATUS[0]}` after `cmd | grep ...` yields an *empty
  string*, and `[ "$rc" -ne 0 ]` then guards nothing at all. It fails
  open and silently, which is the worst way to fail. Write
  `cmd > log 2>&1; rc=$?` and grep the log afterwards. `timeout` is not
  a macOS command either: `perl -e 'alarm N; exec @ARGV' cmd` is.
- **Never read an instrument's silence as an answer.** Three measured
  nothing in one session and twice the emptiness was nearly taken for a
  result: `mon.py` and `llvm-objdump` with their stderr sent to
  `/dev/null`, and an `awk` using `strtonum`, which BSD awk lacks,
  reporting an 894-byte function as 0 (5.24). Show stderr, and check
  the tool produced output before believing what it did not say.
- A hypothesis about what the linker will drop is settled by the link
  and nothing else: `cputs` went and `escapeCode` stayed (5.24).
- Record every hardware finding in `REQUIREMENTS.md`, numbered.
- Commits are local until the user asks for a push.
- What the user sees is in American English: "colors", not "colours"
  (the user's rule for every app, 2026-09-22). Identifiers in the shared
  platform modules keep their spelling; strings do not.
