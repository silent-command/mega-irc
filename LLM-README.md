# mega-irc

An IRC client for the [MEGA65](https://mega65.org), written in C for
llvm-mos, running in native MEGA65 mode on [mega-net](../mega-net).

**Status: working.** TLS 1.3 to Libera and OFTC with the certificate
chain verified to a carried root, plaintext to a server of your own,
eight views on the function keys with scrollback in the 8 MB expansion,
a line editor with history, SASL and NickServ, bookmarks -- all
verified on the machine. See `REQUIREMENTS.md` for the decisions, the
plan and what was seen on hardware, finding by finding.

## Using it

`bin/IRC.D81` holds the client, its TLS bank and mega-net. Mount it and
`RUN "IRC"`. The first screen lists the saved bookmarks, if any: a
number connects, RETURN asks instead, `dN` deletes one. The prompts are
the server, the port, TLS (y or n), a nick, the channels to join
(optional, comma-separated; none is fine, the status view shows the
MOTD and you `/join` from there), whether to save a bookmark, and a
password. The defaults are `irc.libera.chat`, 6697, TLS on, nick
`mega65`.

The password is asked every session and never written to disk. Where
the server offers SASL (Libera does) the client logs in with it before
registering; where it does not, the same password goes to NickServ on
connect. RETURN alone means none.

A TLS connection takes about 20 s: the handshake and the chain are
verified on the MEGA65 itself, RSA and P-256. Servers that present an
ECDSA chain are not supported; Libera serves one from about half its
addresses, and the client dials the next.

The client needs the 8 MB expansion RAM, which holds the scrollback.

| Key | Does |
|---|---|
| F1, F3, F5, F7, F2, F4, F6, F8 | the status view and up to seven channels, in that order (the first four need no SHIFT); an unread view is starred on the top row |
| F9, F11 | scroll the view a page back, forward |
| HOME | back to the newest line |
| cursor up, down | the lines already sent, to send again or edit |
| cursor left, right, INST/DEL | edit the line being typed |
| MEGA+F | next text color, for the whole screen |
| MEGA+B | next background and border color (the first press: black) |
| RUN/STOP | leave the server, back to the first screen; from there, to BASIC |

Each nick is shown in a color of its own, chosen from the name, so two
people are told apart at a glance.

| Command | Does |
|---|---|
| `/join #channel` | join, in the next free view |
| `/part` | leave the showing channel |
| `/msg nick text` | a private message |
| `/me does something` | an action |
| `/nick name` | change nick |
| `/identify password` | NickServ, by hand |
| `/raw LINE` | send a line as typed |
| `/quit` | leave the server |

Anything else that starts with `/` is refused rather than sent as text.

## Building

Needs [llvm-mos](https://llvm-mos.org), `c1541` from VICE, Python 3,
and two sibling checkouts: `../mega-net` and
`../mega65-libc` (github.com/MEGA65/mega65-libc).

```
python3 build.py           the disk image, bin/IRC.D81
python3 build.py test      the host suite: the IRC parser, the TLS engine, the chain check
python3 tools/deploy.py    onto the MEGA65's SD card, keeping the bookmarks (IRC.CFG) already there
```

A plain copy of the disk image onto the card replaces `IRC.CFG` with
an empty one; the deploy tool carries it across.

`tools/irc_test_server.py` is a small IRC server for testing on the
development machine: it registers, joins, chats, pings, floods on
request, offers SASL with `--sasl user:pass`, and logs every line.

## Bookmarks

Saved in `IRC.CFG` on the disk the client booted from, as plain text:
host, port, TLS, nick and channels. No passwords are stored.

## License

0BSD, see `LICENSE`. mega-net is a separate project under the same
license; mega65-libc is under its own. The platform modules under
`src/platform/` and the TLS engine and its arithmetic under `src/tls/`,
`src/crypto/` and `src/bank/` are shared with the MEGA65 Gemini, FTP,
NTP and SSH clients, under the same license.
