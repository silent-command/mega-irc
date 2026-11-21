# mega-irc

An IRC client for the MEGA65. Connect to an IRC network over TLS 1.3
or plain text, join channels, and chat, with a view for the server and
up to seven channels or private conversations, each with its own
scrollback.

## Starting

Mount `IRC.D81` and type `RUN "IRC"`, or choose IRC from the
[Net-Tools](https://github.com/silent-command/mega-menu) menu. It needs a
network cable and the 8 MB attic RAM. Set the clock first (with
[mega-ntp](https://github.com/silent-command/mega-ntp)) so it can check
the dates on a server's certificate.

On the first screen, type a bookmark's number and press RETURN, or press
RETURN alone to type a server, port, nick and channels. D and a number
deletes a bookmark; RUN/STOP quits to BASIC.

## Keys

| Key | Does |
|---|---|
| F1 | the server view |
| F3 / F5 / F7 | channel or conversation views 1 to 3 |
| F2 / F4 / F6 / F8 | views 4 to 7 |
| F9 / F11 | scroll back / forward a page |
| CLR/HOME | back to the newest line |
| CRSR up / down | lines you sent before |
| CRSR left / right | move in the line you are typing |
| RETURN | send the line, or run a command |
| MEGA+F / MEGA+B | change the text / background colour |
| RUN/STOP | leave the server, back to the first screen |

## Commands

| Command | Does |
|---|---|
| `/join #channel` | join a channel |
| `/part` | leave this channel |
| `/msg nick text` | send a private message |
| `/query nick` | open a conversation with someone |
| `/close` | close this conversation |
| `/me text` | an action |
| `/nick name` | change your nick |
| `/whois nick` | about someone |
| `/topic text` | show or set the topic |
| `/away reason`, `/back` | mark yourself away, and back |
| `/ignore nick` | hide someone's messages (`/ignore -nick` undoes it) |
| `/identify password` | log in to NickServ |
| `/quit message` | leave the server |
| `/help` | the full list |

## Credits

Created by Justin Cameron. Apache License 2.0; see [LICENSE](LICENSE)
and [NOTICE](NOTICE).
