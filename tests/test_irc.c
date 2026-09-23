/* The host suite for src/irc.c: the line shapes a real server sends,
 * and the malformed ones it must not fall over.
 *
 * The samples are the ones seen on the wire during 5.4's Libera session
 * and the fixture runs, not invented shapes. */
#include <stdio.h>
#include <string.h>
#include "irc.h"

static int checks, failed;
#define CHECK(cond, what) do { checks++; if (!(cond)) { failed++; printf("FAIL line %d: %s\n", __LINE__, what); } } while (0)

/* irc_parse splits in place, so every case gets its own copy. */
static char buf[IRC_LINE_MAX + 1];
static uint8_t parse(irc_msg *m, const char *text)
{
  size_t n = strlen(text);
  if (n > IRC_LINE_MAX) n = IRC_LINE_MAX;
  memcpy(buf, text, n);
  buf[n] = 0;
  return irc_parse(buf, m);
}

static int eq(const char *a, const char *b)
{
  if (!a || !b) return a == b;
  return strcmp(a, b) == 0;
}

int main(void)
{
  irc_msg m;
  char tag[16];

  /* ---- what the spike already handled, kept working ---- */

  CHECK(parse(&m, ":nick!user@host PRIVMSG #mega65 :hello there"), "a PRIVMSG parses");
  CHECK(eq(m.prefix, "nick"), "the prefix is cut to the nick");
  CHECK(eq(m.host, "user@host"), "and what followed the bang is kept");
  CHECK(irc_is(&m, "PRIVMSG"), "the command is PRIVMSG");
  CHECK(m.nparams == 1 && eq(m.params[0], "#mega65"), "the target is the one parameter");
  CHECK(eq(m.trailing, "hello there"), "the text is the trailing");
  CHECK(m.numeric == 0, "a word command has no numeric");

  CHECK(parse(&m, "PING :mega-irc.test"), "a PING with no prefix parses");
  CHECK(!m.prefix && irc_is(&m, "PING"), "no prefix, command PING");
  CHECK(eq(m.trailing, "mega-irc.test"), "the token is the trailing");

  CHECK(parse(&m, "PING mega-irc.test"), "a PING with the token as a parameter");
  CHECK(m.nparams == 1 && eq(m.params[0], "mega-irc.test") && !m.trailing,
        "no colon means a parameter, not a trailing");

  CHECK(parse(&m, ":server 001 mega65 :Welcome to the network"), "a 001 parses");
  CHECK(m.numeric == 1, "001 reads as the number 1");
  CHECK(eq(m.params[0], "mega65"), "the nick is the first parameter");

  CHECK(parse(&m, ":server 433 * mega65 :Nickname is already in use"), "a 433 parses");
  CHECK(m.numeric == 433 && m.nparams == 2 && eq(m.params[1], "mega65"), "433 keeps both parameters");

  /* 353 is why one parameter was never enough: the channel is the
   * second, behind the "=" that says the channel is public. */
  CHECK(parse(&m, ":server 353 mega65 = #mega65 :mega65 groepaz @op"), "a 353 parses");
  CHECK(m.numeric == 353, "353 reads as its number");
  CHECK(m.nparams == 3 && eq(m.params[1], "=") && eq(m.params[2], "#mega65"),
        "the channel is the third parameter, behind the =");
  CHECK(eq(m.trailing, "mega65 groepaz @op"), "the names are the trailing");

  CHECK(parse(&m, ":server 366 mega65 #mega65 :End of /NAMES list."), "a 366 parses");
  CHECK(m.numeric == 366, "366 reads as its number");

  CHECK(parse(&m, ":server 372 mega65 :- welcome to the MOTD"), "a 372 parses");
  CHECK(m.numeric == 372 && m.trailing[0] == '-', "the MOTD line keeps its dash");

  CHECK(parse(&m, ":nick!u@h JOIN :#mega65"), "a JOIN with the channel as trailing");
  CHECK(irc_is(&m, "JOIN") && eq(m.trailing, "#mega65"), "JOIN, channel in the trailing");
  CHECK(parse(&m, ":nick!u@h JOIN #mega65"), "a JOIN with the channel as a parameter");
  CHECK(irc_is(&m, "JOIN") && eq(m.params[0], "#mega65"), "both JOIN shapes work");

  CHECK(parse(&m, ":nick!u@h QUIT :Ping timeout: 240 seconds"), "a QUIT parses");
  CHECK(irc_is(&m, "QUIT") && eq(m.trailing, "Ping timeout: 240 seconds"),
        "a colon inside the trailing is left alone");

  /* ---- the three things the spike got wrong ---- */

  CHECK(parse(&m, ":nick!u@h PRIVMSGX #c :x"), "a command that merely starts with PRIVMSG");
  CHECK(!irc_is(&m, "PRIVMSG"), "PRIVMSGX is not PRIVMSG");
  CHECK(irc_is(&m, "PRIVMSGX"), "but it is itself");

  CHECK(parse(&m, "@time=2026-09-22T16:00:00Z :nick!u@h PRIVMSG #c :tagged"),
        "a line with IRCv3 tags parses");
  CHECK(eq(m.tags, "time=2026-09-22T16:00:00Z"), "the tags are kept aside");
  CHECK(irc_is(&m, "PRIVMSG"), "the command is found behind the tags, not the tag itself");
  CHECK(eq(m.prefix, "nick") && eq(m.trailing, "tagged"), "and the rest parses normally");

  CHECK(parse(&m, ":server 005 me A=1 B=2 C=3 D=4 E=5 F=6 G=7 H=8 I=9 :are supported"),
        "more parameters than are kept");
  CHECK(m.nparams == IRC_PARAMS_MAX, "the store fills to its limit");
  CHECK(eq(m.trailing, "are supported"), "and the trailing is still found beyond them");

  /* ---- shapes that must not break it ---- */

  CHECK(parse(&m, ":nick!u@h PRIVMSG   #c   :spaced"), "repeated spaces");
  CHECK(eq(m.params[0], "#c") && eq(m.trailing, "spaced"), "runs of spaces collapse");

  CHECK(parse(&m, "PRIVMSG #c :"), "an empty trailing");
  CHECK(m.trailing && m.trailing[0] == 0, "which is empty, not absent");

  CHECK(!parse(&m, ""), "an empty line yields nothing");
  CHECK(!parse(&m, ":prefix-only"), "a prefix with no command yields nothing");
  CHECK(!parse(&m, "@tags-only"), "tags with nothing after them yield nothing");
  CHECK(parse(&m, "PING"), "a bare command is still a command");
  CHECK(irc_is(&m, "PING") && m.nparams == 0 && !m.trailing, "with nothing else");

  {
    /* A full-length line: 510 bytes of content, as the RFC allows. */
    static char big[IRC_LINE_MAX + 1];
    size_t i, head;
    strcpy(big, ":n!u@h PRIVMSG #c :");
    head = strlen(big);
    for (i = head; i < 510; i++) big[i] = 'x';
    big[510] = 0;
    CHECK(parse(&m, big), "a 510-byte line parses");
    CHECK(m.trailing && strlen(m.trailing) == 510 - head, "its trailing is whole");
  }

  /* ---- CTCP ---- */

  CHECK(parse(&m, ":nick!u@h PRIVMSG mega65 :\001VERSION\001"), "a CTCP VERSION arrives as a PRIVMSG");
  CHECK(irc_ctcp(&m, tag, sizeof tag), "and is recognised as CTCP");
  CHECK(eq(tag, "VERSION"), "the tag is VERSION");
  CHECK(m.trailing && m.trailing[0] == 0, "with no argument");

  CHECK(parse(&m, ":nick!u@h PRIVMSG #c :\001ACTION waves slowly\001"), "a CTCP ACTION");
  CHECK(irc_ctcp(&m, tag, sizeof tag) && eq(tag, "ACTION"), "the tag is ACTION");
  CHECK(eq(m.trailing, "waves slowly"), "the argument survives without the delimiters");

  CHECK(parse(&m, ":nick!u@h PRIVMSG #c :ordinary text"), "ordinary text");
  CHECK(!irc_ctcp(&m, tag, sizeof tag), "is not CTCP");
  CHECK(eq(m.trailing, "ordinary text"), "and is left untouched by the attempt");

  CHECK(parse(&m, ":nick!u@h PRIVMSG #c :\001\001"), "an empty CTCP");
  CHECK(!irc_ctcp(&m, tag, sizeof tag), "is refused rather than read as a blank tag");

  printf("%d checks, %d failed\n", checks, failed);
  return failed != 0;
}
