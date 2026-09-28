# MEGA65 IRC Client

An IRC client for the MEGA65 in native C: one server at a time, over
TLS or in the clear, the status window and up to seven channels or
private conversations on the function keys, scrollback in attic RAM,
bookmarks on the disk. Version 0.2.0.

## What is on the disk

`IRC.D81` carries everything the client needs:

| File        | What it is                                                   |
|-------------|--------------------------------------------------------------|
| `irc`       | the client; `RUN "IRC"`                                      |
| `meganet`   | the mega-net TCP/IP stack, loaded into upper memory at start |
| `irccrypto` | the TLS bank: keys, ciphers and the certificate check        |
| `chain`     | the top window of that bank                                  |
| `high`      | the client's own second image, under the KERNAL              |
| `irc.cfg`   | your bookmarks, written by the client when you save one      |

The client needs the 8 MB attic RAM expansion for its scrollback, and a
network cable: mega-net takes a DHCP lease at start.

## The first screen

If bookmarks are saved, they are listed with a number each. Type a
number and RETURN to connect to that one, RETURN alone to be asked
instead, or `d` and a number to delete one.

The questions, when asked: the server, the port (6697 is TLS on most
networks, 6667 plaintext), whether to use TLS, your nick, and the
channels to join, comma separated; a name without its `#` gets one.
RETURN accepts what is shown. You can then save the answers as a
bookmark. Last comes the password, for SASL or NickServ, typed as
asterisks and never written to the disk; RETURN skips it.

RUN/STOP at the first screen returns to BASIC. RUN/STOP in a session
ends it and comes back to the first screen.

## The screen

Row 0 is the bar of views on their keys. The chat fills the middle.
Below it, the counts row shows the view's name, the session's uptime,
lines received and sent, and how far back the view is scrolled; then
the status row for short notices, which fade after five seconds; then
the line you are typing. Both 25-row and 50-row modes work: start the
client in whichever mode the machine is in.

| Key           | What it does                                              |
|---------------|-----------------------------------------------------------|
| F1            | the status view                                           |
| F3 F5 F7      | the first, second and third channel or private view       |
| F2 F4 F6 F8   | the fourth to seventh                                     |
| F9 / F11      | scroll the view back / forward a page                     |
| HOME          | back to the newest line                                   |
| CRSR up/down  | the lines you sent before, most recent first              |
| CRSR left/right | move within the line; typing inserts, DEL erases before |
| MEGA-B / MEGA-F | cycle the background / text color                       |
| RUN/STOP      | leave the session                                         |

Views open in the order you join or are messaged, and close up when
one closes, so the keys never skip. A view with new lines you have not
looked at is starred on the bar. Each nick is drawn in one of eight
colors chosen from its name, so two people are told apart at a glance.

## Talking

A line without a slash goes to the channel or person whose view you
are in. Everything else is a slash command; an unknown one is refused
rather than sent as text.

| Command                    | What it does                                            |
|----------------------------|---------------------------------------------------------|
| `/join #channel` (`/j`)    | join, and open its view                                 |
| `/part [#channel]` (`/p`)  | leave the channel, this view's if none is named         |
| `/query nick [text]`       | open a view for a person; what you type there goes to them |
| `/close`                   | close a person's view                                   |
| `/msg nick text` (`/m`)    | one private message                                     |
| `/me does something`       | an action, in the channel or private view you are in    |
| `/notice nick text` (`/n`) | a notice                                                |
| `/nick name`               | change your nick                                        |
| `/away [reason]`, `/back`  | mark yourself away, and back                            |
| `/whois nick` (`/w`)       | who they are; the answer lands in the view you are in   |
| `/names [#channel]`        | who is in the channel                                   |
| `/topic [#channel] [text]` (`/t`) | show the topic, or set it when text is given     |
| `/invite nick [#channel]` (`/i`) | invite someone                                   |
| `/kick nick [reason]` (`/k`) | remove someone, if you are an operator                |
| `/mode [#channel] modes`   | change channel modes                                    |
| `/op nick`, `/deop nick`   | give or take operator status                            |
| `/ignore nick`             | hide everything from a nick; `/ignore -nick` shows them again; `/ignore` alone lists |
| `/clear`                   | empty this view's scrollback                            |
| `/identify [password]`     | identify to NickServ, with the password given or the one typed at start |
| `/raw line` (`/quote`)     | send a line to the server as typed                      |
| `/quit [message]` (`/q`)   | leave the server                                        |
| `/help` (`/?`)             | list the commands                                       |

A private message from someone opens a view for them, starred on the
bar; if no view can be opened (all seven are taken) the message is
shown marked `(private)` in the view you are looking at. Messages from
an ignored nick are dropped before they reach the screen, their CTCP
requests too.

CTCP: a VERSION request is answered with the client's name and
version, a PING is echoed, an ACTION is shown as `* nick does
something`. A DCC file offer is shown in the person's view, but this
client cannot take one.

## TLS and certificates

With TLS on, the client checks the server's certificate chain: that
the leaf names the host you dialed, that every certificate is in date,
and that each is signed by the next up to a root the client carries
(the ISRG roots that Let's Encrypt issues under, which Libera uses).
The check takes about twenty seconds of arithmetic on the MEGA65; the
status view then says how many certificates were verified and how
long it took.

The dates are checked against the machine's clock, so set it first, by
hand or with the mega-ntp client. If the clock is unset the client
says so and skips the date check.

Only RSA-signed chains can be checked. Some of Libera's servers serve
an ECDSA chain instead; the client notices, drops that connection and
dials again, up to five times a few seconds apart, until it lands on a
server it can check. Networks whose servers insist on the P-256 key
agreement (OFTC) cannot be reached over TLS since version 0.2.0; use
port 6667 in the clear for those.

Passwords: with a password typed at start, the client offers SASL
PLAIN during registration and falls back to a NickServ IDENTIFY on
networks without it.

## Building it

Needs llvm-mos (`mos-mega65-clang`), the mega65-libc and mega-net
checkouts beside this one, VICE's `c1541`, and Python 3.

    python3 build.py           the bank, the client and bin/IRC.D81
    python3 build.py test      the host test suites
    python3 tools/deploy.py    the disk onto the card's net-tools folder,
                               keeping the irc.cfg already there

`build.py` prints how much room is left between the program and its
stack, and stops the build if it is under 1 KB. `tools/irc_test_server.py`
is a small IRC server for testing on the local network without
bothering a real one.

## License

Apache 2.0.
