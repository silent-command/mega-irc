/* The IRC line protocol, RFC 1459 with the modern additions this client
 * has to survive. Portable C99: the host suite proves it, the client
 * uses it, and nothing in here touches the screen or the network.
 *
 * The spike parsed lines inline in ircc.c, which worked but could never
 * be tested and carried three things worth not copying: it matched
 * PRIVMSG on four characters, so PRIVMSGX matched too; it had no notion
 * of IRCv3 message tags, so a line beginning "@tag " would have had the
 * tag read as its command; and it kept only one parameter, which is not
 * enough for 353, whose parameters are "= #channel" before the names.
 *
 * A line is split IN PLACE: the fields point into the caller's buffer
 * and the separators become terminators. That is what suits a 6502, and
 * it means the caller owns the memory and may keep or discard it. Feed
 * a copy if the original is still needed.
 */
#ifndef IRC_H
#define IRC_H

#include <stdint.h>

#define IRC_LINE_MAX 512         /* RFC 1459: 510 and the CRLF */
#define IRC_PARAMS_MAX 8         /* middle parameters kept; more are walked but not stored */

typedef struct {
  char *tags;        /* the "@..." segment with its @ gone, or 0. Not parsed further. */
  char *prefix;      /* the prefix with its ':' gone, cut at '!' so it is the nick alone, or 0 */
  char *host;        /* what followed the '!' in the prefix, or 0 */
  char *cmd;         /* always set when the parse succeeds */
  char *params[IRC_PARAMS_MAX];
  char *trailing;    /* what followed " :", or 0 */
  uint16_t numeric;  /* 001, 353 and so on; 0 when the command is not three digits */
  uint8_t nparams;
} irc_msg;

/* Splits `line` in place. 1 if there is a command, 0 if the line is
 * empty, all prefix, or otherwise has nothing to act on. */
uint8_t irc_parse(char *line, irc_msg *m);

/* Is the command exactly `name`? A plain comparison, so PRIVMSGX does
 * not match PRIVMSG. */
uint8_t irc_is(const irc_msg *m, const char *name);

/* A CTCP request or reply, which is a trailing wrapped in \001:
 * "\001VERSION\001" or "\001ACTION waves\001". Copies the tag into
 * `tag` and leaves m->trailing pointing at the argument with the
 * delimiters removed. 0 if the trailing is not CTCP, in which case
 * nothing is changed. */
uint8_t irc_ctcp(irc_msg *m, char *tag, uint8_t tagcap);

#endif
