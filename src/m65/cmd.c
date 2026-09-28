/* The slash commands, the private views, the ignore list and CTCP: the
 * part of the client that grew past the program's room in September
 * 2026 (5.32). It sits in the HIGH window under the KERNAL (irc.ld,
 * $E000), in the room the chain's client half left when the whole
 * check moved into the TLS bank (5.31), and is compiled apart from
 * ircc.c, so every call across is an ordinary one (ircc.h).
 *
 * A private message goes to the person's own view when one is open --
 * /query opens one, and the first message from a person opens one too
 * -- else it is shown marked (private) in the view you are looking at.
 * What you type in a person's view goes to them, as in a channel. */
#include "mega65/memory.h"
#include "m65/view.h"
#include "m65/log.h"
#include "m65/ircc.h"

/* /ignore: up to eight nicks in the attic, above the sent-line history
 * in view 0's slot (ircc.c, HIST_BASE), each a NUL-terminated name of
 * at most 16; an empty entry is free. */
#define HIST_BASE 0x80C0000UL
#define HIST_MAX 8
#define HIST_SLOT 208
#define IGNORE_BASE (HIST_BASE + HIST_MAX * HIST_SLOT)
#define IGNORE_MAX 8
#define IGNORE_SLOT 17

/* The list emptied: the attic holds whatever it held at power-on, which
 * read as eight names of noise the first time (5.32). Once, at start. */
void cmd_init(void) { lfill(IGNORE_BASE, 0, IGNORE_MAX * IGNORE_SLOT); }

/* Entry i of the ignore list into `tmp`, the send buffer, which is idle
 * between lines sent: not `shown`, which say() composes into, and a
 * name said from there copied itself to the end of the row (5.32). */
static void ignore_get(unsigned char i)
{
  lcopy(IGNORE_BASE + (unsigned long)i * IGNORE_SLOT, (long)(unsigned int)tmp, IGNORE_SLOT);
  tmp[IGNORE_SLOT - 1] = 0;
}

static unsigned char ignored(const char *who)
{
  unsigned char i;
  for (i = 0; i < IGNORE_MAX; i++) { ignore_get(i); if (tmp[0] && same_ci(tmp, who)) return 1; }
  return 0;
}

static void usage(const char *text) { say(view_active, "-- ", text, 0); }

/* CMD a b :text, the parts that are given, sent as one line. */
static void sendv(const char *cmd, const char *a, const char *b, const char *text)
{
  char *p = tmp, *e = tmp + LINE_MAX - 2;
  p = ui_cat(p, e, cmd);
  if (a && *a) { p = ui_cat(p, e, " "); p = ui_cat(p, e, a); }
  if (b && *b) { p = ui_cat(p, e, " "); p = ui_cat(p, e, b); }
  if (text && *text) { p = ui_cat(p, e, " :"); p = ui_cat(p, e, text); }
  p = ui_cat(p, tmp + LINE_MAX, "\r\n"); *p = 0;
  send_all(tmp);
}

/* A CTCP reply: NOTICE who :\001TAG text\001, the wrapped part built in
 * `shown` and sent as the notice's text. */
static void ctcp_reply(const char *who, const char *tag, const char *text)
{
  char *p = compose("\001", tag, text && *text ? " " : "");
  p = ui_cat(p, SHOWN_END, text); p = ui_cat(p, SHOWN_END, "\001"); *p = 0;
  sendv("NOTICE", who, 0, shown);
}

/* A DCC offer in a private message: said in the person's view. Taking
 * one is written (the dcc-wip branch) and 800 bytes short of the room (5.33). */
static void dcc_offer(unsigned char v, const char *who, const char *text)
{
  char *p = compose("-- ", who, " offers DCC: ");
  fold(p, SHOWN_END, text);
  view_line(v, shown);
  say(v, "-- (this client cannot take a DCC transfer)", 0, 0);
}

/* ---- lines from the server ---------------------------------------------- */

void cmd_said(irc_msg *m, const char *who)
{
  char tag[12];
  unsigned char notice = irc_is(m, "NOTICE"), query, qv, v;
  if (m->host && ignored(who)) return;           /* from a person on the ignore list; servers have no host */
  query = (unsigned char)(!is_channel(m->params[0]));
  qv = query ? view_find(who) : VIEW_NONE;
  if (query && qv == VIEW_NONE && !notice && m->host) qv = view_open(who);   /* a person's first message opens their view */
  v = qv != VIEW_NONE ? qv : view_of(m->params[0]);
  if (irc_ctcp(m, tag, sizeof tag)) {
    if (same_ci(tag, "ACTION")) say_text(v, "* ", who, " ", m->trailing);
    else if (notice) { char *p = compose("-- ", who, " CTCP reply: "); fold(p, SHOWN_END, m->trailing); view_line(v, shown); }
    else if (same_ci(tag, "VERSION")) ctcp_reply(who, "VERSION", "MEGA65 IRC client " IRCC_VERSION);
    else if (same_ci(tag, "PING")) ctcp_reply(who, "PING", m->trailing);
    else if (same_ci(tag, "DCC")) dcc_offer(v, who, m->trailing);
    return;                                        /* other requests go unanswered */
  }
  if (notice) say_text(v, "-", who, "- ", m->trailing);
  else say_text(v, (query && qv == VIEW_NONE) ? "(private) <" : "<", who, "> ", m->trailing);
  if (qv != VIEW_NONE && qv != view_active) ui_status("message from ", who);
}

/* INVITE, the topic and away notices, and WHOIS: 311 user, 312 server,
 * 313 operator, 317 idle, 319 channels, 301 away, 307 registered, 330
 * account, 338 actual host, 671 secure, 318 the end; into the view you
 * are in. */
unsigned char cmd_routed(irc_msg *m, const char *who, const char *text)
{
  unsigned int n = m->numeric;
  if (irc_is(m, "INVITE")) { char *p = compose("-- ", who, " invites you to "); ui_cat(p, SHOWN_END, text); view_line(view_active, shown); return 1; }
  if (n == 331) { say(view_of(m->params[1]), "-- no topic is set", 0, 0); return 1; }
  if (n == 305 || n == 306) { say(view_active, n == 306 ? "-- you are away" : "-- you are back", 0, 0); return 1; }
  if (n == 318) return 1;
  if (n == 301 || n == 307 || (n >= 311 && n <= 319 && n != 315 && n != 316) || n == 330 || n == 338 || n == 671) {
    char *p = shown, *e = SHOWN_END; unsigned char i;
    p = ui_cat(p, e, "-- ");
    for (i = 1; i < m->nparams; i++) { p = ui_cat(p, e, m->params[i]); p = ui_cat(p, e, " "); }
    if (m->trailing) fold(p, e, m->trailing); else *p = 0;
    view_line(view_active, shown);
    return 1;
  }
  return 0;
}

/* ---- what is typed --------------------------------------------------------- */

/* The word at the start of `arg` cut off and returned; `arg` moves past
 * it and the spaces after. */
static char *first_word(char **arg)
{
  char *w = *arg, *p = w;
  while (*p && *p != ' ') p++;
  if (*p) *p++ = 0;
  while (*p == ' ') p++;
  *arg = p;
  return w;
}

/* A channel command's channel: the argument if it names one, else the
 * view you are in; 0, with a note, when neither is a channel. */
static const char *chan_arg(char **arg)
{
  const char *chan;
  if (is_channel(*arg)) chan = first_word(arg);
  else chan = view_name(view_active);
  if (is_channel(chan)) return chan;
  usage("not in a channel here");
  return 0;
}

static void ignore_cmd(char *arg)
{
  /* /ignore nick adds, /ignore -nick removes, /ignore alone lists */
  unsigned char i, n = 0, free_slot = IGNORE_MAX, remove = (unsigned char)(arg[0] == '-');
  if (remove) arg++;
  for (i = 0; i < IGNORE_MAX; i++) {
    ignore_get(i);
    if (!tmp[0]) { if (free_slot == IGNORE_MAX) free_slot = i; continue; }
    if (!*arg) { say(view_active, "-- ignoring ", tmp, 0); n++; continue; }
    if (same_ci(tmp, arg)) {
      if (remove) { lfill(IGNORE_BASE + (unsigned long)i * IGNORE_SLOT, 0, IGNORE_SLOT); say(view_active, "-- no longer ignoring ", arg, 0); }
      else say(view_active, "-- already ignoring ", arg, 0);
      return;
    }
  }
  if (!*arg) { if (!n) usage("ignoring nobody; /ignore nick adds, /ignore -nick removes"); return; }
  if (remove) { say(view_active, "-- not ignoring ", arg, 0); return; }
  if (free_slot == IGNORE_MAX) { usage("the ignore list is full"); return; }
  for (n = 0; arg[n] && n < IGNORE_SLOT - 1; n++) tmp[n] = arg[n];
  tmp[n] = 0;
  lcopy((long)(unsigned int)tmp, IGNORE_BASE + (unsigned long)free_slot * IGNORE_SLOT, IGNORE_SLOT);
  say(view_active, "-- ignoring ", arg, 0);
}

/* The slash commands (section 2); anything else refused, never sent as
 * text. `s` is the line past its slash: the command word, then `arg`,
 * the rest of the line. The words are one table walked once, and the
 * shape of each command's line is a flag byte in a second, so the
 * common case -- gather a channel, a word and the rest, send a verb --
 * is one path: a chain of compares and a case per command was inlined
 * at every branch and cost kilobytes (5.32). */
enum { C_JOIN = 1, C_PART, C_QUIT, C_NICK, C_MSG, C_ME, C_IDENTIFY, C_RAW, C_QUERY, C_CLOSE, C_WHOIS, C_NAMES,
       C_TOPIC, C_AWAY, C_BACK, C_NOTICE, C_KICK, C_MODE, C_OP, C_DEOP, C_INVITE, C_IGNORE, C_CLEAR, C_HELP };
static const char cmd_words[] =
  "join\0j\0part\0p\0quit\0q\0nick\0msg\0m\0me\0identify\0id\0raw\0quote\0query\0close\0whois\0w\0names\0"
  "topic\0t\0away\0back\0notice\0n\0kick\0k\0mode\0op\0deop\0invite\0i\0ignore\0clear\0help\0?\0";
static const unsigned char cmd_ids[] = {
  C_JOIN, C_JOIN, C_PART, C_PART, C_QUIT, C_QUIT, C_NICK, C_MSG, C_MSG, C_ME, C_IDENTIFY, C_IDENTIFY, C_RAW, C_RAW,
  C_QUERY, C_CLOSE, C_WHOIS, C_WHOIS, C_NAMES, C_TOPIC, C_TOPIC, C_AWAY, C_BACK, C_NOTICE, C_NOTICE, C_KICK, C_KICK,
  C_MODE, C_OP, C_DEOP, C_INVITE, C_INVITE, C_IGNORE, C_CLEAR, C_HELP, C_HELP };

/* The shape of each command's line, by C_* */
#define F_CHAN 1     /* a channel first: the argument if it names one, else the view you are in */
#define F_WORD 2     /* then a word, which must be there */
#define F_TEXT 4     /* the rest of the line as the trailing text */
#define F_NEED 8     /* ...and it must be there */
#define F_REST 16    /* the rest of the line as a middle parameter (MODE) */
#define F_SWAP 32    /* the word before the channel (INVITE) */
#define F_SEND 64    /* sent as gathered: "VERB channel word :text" */
static const unsigned char cmd_flags[] = {
  0, 0, F_CHAN, F_TEXT, F_WORD | F_SEND, F_WORD | F_TEXT | F_NEED, F_TEXT | F_NEED, 0, 0, 0, 0,
  F_WORD | F_SEND, F_CHAN | F_SEND, F_CHAN | F_TEXT | F_SEND, F_TEXT | F_SEND, F_SEND, F_WORD | F_TEXT | F_NEED | F_SEND,
  F_CHAN | F_WORD | F_TEXT | F_SEND, F_CHAN | F_REST | F_SEND, F_CHAN | F_WORD, F_CHAN | F_WORD, F_WORD | F_CHAN | F_SWAP | F_SEND, 0, 0, 0 };
static const char *const cmd_verb[] = {
  0, 0, "PART", "QUIT", "NICK", 0, 0, 0, 0, 0, 0, "WHOIS", "NAMES", "TOPIC", "AWAY", "AWAY", "NOTICE", "KICK", "MODE",
  "MODE", "MODE", "INVITE", 0, 0, 0 };

static unsigned char cmd_lookup(const char *s)
{
  const char *w = cmd_words;
  unsigned char i;
  for (i = 0; i < sizeof cmd_ids; i++) {
    if (same_ci(w, s)) return cmd_ids[i];
    while (*w++) ;
  }
  return 0;
}

void cmd_line(char *s)
{
  char *arg = s, *word = 0;
  const char *chan = 0, *text = 0;
  unsigned char id, f, v;
  first_word(&arg);                                /* s is the word; arg the rest */
  id = cmd_lookup(s);
  if (!id) { say(view_active, "-- unknown command: /", s, "; /help lists them"); return; }
  f = cmd_flags[id];
  if ((f & F_CHAN) && !(f & F_SWAP) && !(chan = chan_arg(&arg))) return;
  if (f & F_WORD) { word = first_word(&arg); if (!*word) goto need; }
  if ((f & F_SWAP) && !(chan = chan_arg(&arg))) return;
  if ((f & F_NEED) && !*arg) goto need;
  if (f & F_TEXT) text = arg;
  switch (id) {
  case C_JOIN: join(arg); break;
  case C_PART:
    v = view_find(chan);
    sendv(cmd_verb[id], chan, 0, 0);
    if (v != VIEW_NONE) view_close(v);              /* at once, not on the echo: a server that sends none would leave it open */
    break;
  case C_QUIT:
    sendv(cmd_verb[id], 0, 0, *arg ? arg : "the MEGA65 says goodbye");
    quitting = 1;
    break;
  case C_MSG: privmsg(word, arg, 0); break;
  case C_ME:
    if (view_is_channel(view_active)) privmsg(view_name(view_active), arg, 1);
    else usage("/me needs a channel or a person");
    break;
  case C_IDENTIFY: {
    unsigned char i;
    if (*arg) { for (i = 0; arg[i] && i < LOW_NSPASS_CAP - 1; i++) nspass[i] = arg[i]; nspass[i] = 0; }
    if (nspass[0]) { send3("PRIVMSG NickServ :IDENTIFY ", nspass, 0); usage("identifying to NickServ"); }
    else goto need;
    break; }
  case C_RAW:
    if (*arg) { send3(arg, 0, 0); say(view_active, "-> ", arg, 0); }
    break;
  case C_QUERY:
    /* a view for a person; what you type there goes to them */
    if (!*arg || is_channel(arg)) goto need;
    v = view_for(first_word(&arg));
    if (v == VIEW_NONE) { usage("no free view: /close or /part one first"); break; }
    view_show(v);
    if (*arg) privmsg(view_name(v), arg, 0);
    break;
  case C_CLOSE:
    if (view_is_channel(view_active) && !is_channel(view_name(view_active))) view_close(view_active);
    else usage("/close ends a private view; /part leaves a channel");
    break;
  case C_AWAY: if (!*arg) text = "away"; break;
  case C_NOTICE: say_text(view_active, "-> -", word, "- ", arg); break;
  case C_OP: case C_DEOP: sendv(cmd_verb[id], chan, id == C_OP ? "+o" : "-o", word); break;
  case C_IGNORE: ignore_cmd(arg); break;
  case C_CLEAR: log_clear(view_active); view_show(view_active); break;
  case C_HELP:
    usage("/join /part /query /close /msg /me /notice /nick /away /back /whois /names");
    usage("/topic /invite /kick /mode /op /deop /ignore /clear /identify /raw /quit");
    break;
  }
  if (f & F_SEND) {
    if (f & F_SWAP) sendv(cmd_verb[id], word, chan, text);
    else sendv(cmd_verb[id], chan, (f & F_REST) ? arg : word, text);
  }
  return;
need:
  say(view_active, "-- /", s, " needs more; /help lists the commands");
}
