/* mega-irc: one server, over TLS or in the clear, the status window and
 * up to seven channels on the function keys, scrollback in attic RAM,
 * the common slash commands. The spike's shape (NOTES.md section 5)
 * with the MVP's TLS half underneath it (REQUIREMENTS.md 5.15, 5.16)
 * and its windows on top (section 2). Bookmarks and NickServ are still
 * to come. */
#include "mega65/memory.h"
#include "meganet.h"
#include "m65_screen.h"
#include "m65_boot.h"
#include "m65_exit.h"
#include "netutil.h"
#include "ui.h"
#include "irc.h"
#include "clock.h"
#include "tls/chain.h"
#include "m65/lowram.h"
#include "m65/bank.h"
#include "m65/rnd.h"
#include "m65/conn.h"
#include "m65/log.h"
#include "m65/view.h"
#include "m65/marks.h"

#define IRCC_VERSION "0.1.2"

/* Measured from the bottom, so the same layout works in 25 rows and in
 * 50: row 0 the views, the chat between, then the counts row, the
 * input row (which the status messages share) and the keys row. */
/* The input line has the bottom row to itself and the status messages
 * the one above it: they shared a row from the spike onwards, so every
 * message wiped what was being typed (NOTES.md section 5, 5.24). The
 * keys are said once into the status view instead of holding a row. */
#define ROW_COUNTS ((unsigned char)(m65_screen_rows() - 3))
#define ROW_INPUT ((unsigned char)(m65_screen_rows() - 1))

#define LINE_MAX 512                 /* RFC 1459: 510 plus the CRLF */
#define FRAMES_PER_S 50
#define IDLE_PING_S 150              /* our own PING after this long without a byte */
#define DEAD_S 300                   /* and the link is dead after this long */

/* The function keys are $F1 to $FE, F1 to F14, in label order: the
 * MEGA65's own queue at $D610, not the C64 PETSCII $85-$8C its KERNAL
 * would hand back, which is the mistake that shipped them dead (5.23).
 * PLATFORM.md 15 and ssh's ui.h have said so all along. */
#define KEY_F1 0xf1
#define KEY_F8 0xf8
#define KEY_F9 0xf9                  /* F9 and F11: the scrollback, on keys that certainly send a code (5.26) */
#define KEY_F11 0xfb
#define KEY_UP 0x91
#define KEY_DOWN 0x11
#define KEY_LEFT 0x9d
#define KEY_RIGHT 0x1d
#define KEY_HOME 0x13

static char server[64] = "irc.libera.chat";
static char port_text[6] = "6697";
static char tls_text[2] = "y";
static char nick[17] = "mega65";
#define channel LOW_CHANNEL          /* no default, and none needed: the user may name some, comma-separated, or a bookmark may (section 2); 64 bytes in low RAM */
static unsigned char use_tls;

#define line LOW_LINE                /* the line being assembled from the stream */
static unsigned int line_len;
#define shown LOW_SHOWN              /* a line composed for the screen */
#define input LOW_INPUT              /* what is being typed */
static unsigned char input_len, input_pos;       /* the length, and where the cursor sits in it */
#define nspass LOW_NSPASS            /* the NickServ password, this session's only (section 2) */
/* The lines sent, for the cursor keys to walk back through: in the
 * attic, so they cost the program two bytes of state and nothing else
 * (5.24). Eight is a power of two and divides 256, so the counter may
 * wrap without disturbing the slots.
 *
 * NOT past the views' eight megabytes, which was the first attempt and
 * is off the end of the machine: the attic is 8 MB, $8000000 to
 * $87FFFFF, and the eight views at a megabyte each fill it exactly. The
 * room is inside view 0's slot instead, above its 8192-row ring: 640 KB
 * of 80-byte rows at first (5.25), 768 KB since the nick's span rides
 * in a 96-byte slot (step 3), so the history sits at $80C0000 and the
 * slot still has 256 KB spare above it. */
#define HIST_BASE 0x80C0000UL
#define HIST_MAX 8
#define HIST_SLOT 208
static unsigned char hist_count, hist_at;
#define tmp LOW_TMP                  /* a line being composed to send, 513 bytes */
static const char *now;              /* the clock as twelve digits (LOW_NOW), or null when it cannot be trusted (5.8) */

/* The spike counted bytes, PINGs and reconnects for the soak (5.2); the
 * client shows what a user reads -- lines in and out, and how far back
 * the view is scrolled. The rest went for the bytes the session loop
 * needed (5.23). */
static unsigned long rx_lines, tx_lines;
static unsigned long idle_frames;
static unsigned char up_h, up_m, up_s, frame_in_s;   /* the clock kept as digits: no 32-bit division here (gemini 5.8) */
static unsigned char last_frame, registered, pinged_idle, quitting;

/* A status message is a notice, not a fixture: "joined #c64" sat on its
 * row until the next message replaced it, which the user found
 * intrusive (5.28). Every message arms a countdown, the second tick
 * runs it down and clears the row, and a view switch clears it at
 * once. The macro arms it at all 23 call sites without touching one;
 * the parenthesised name calls the real function, unexpanded. */
#define STATUS_SECS 5
static unsigned char status_ttl;
#define ui_status(a, b) (status_ttl = STATUS_SECS, (ui_status)(a, b))
#define status_clear() (status_ttl = 0, (ui_status)(0, 0))

/* ---- the screen ---------------------------------------------------------- */

#define SHOWN_END (shown + LOW_SHOWN_CAP - 1)

/* Two or three pieces into `shown`; where the next piece goes. */
static char *compose(const char *a, const char *b, const char *c)
{
  char *p = shown;
  p = ui_cat(p, SHOWN_END, a); p = ui_cat(p, SHOWN_END, b); p = ui_cat(p, SHOWN_END, c);
  *p = 0;
  return p;
}

/* Those pieces as one line of view v. */
static void say(unsigned char v, const char *a, const char *b, const char *c)
{
  compose(a, b, c);
  view_line(v, shown);
}

/* UTF-8 reduced to one byte a character: the lead byte of a multi-byte
 * character becomes '?' and its continuation bytes are dropped, so the
 * columns still line up.
 *
 * The family's m65_fold_utf8 does better -- it folds the accented Latin
 * letters to their base ones -- and costs 894 bytes of tables to do it.
 * This client spent them on the session loop and the editor instead, and
 * shows a '?' where that one would show an 'e' (5.24). It is the first
 * thing to put back if the room ever appears. */
static char *fold(char *p, char *end, const char *s)
{
  unsigned char c;
  while (*s && p < end) {
    c = (unsigned char)*s++;
    if (c < 0x80) { *p++ = (char)c; continue; }
    if ((c & 0xc0) == 0x80) continue;             /* a continuation byte: the character is already marked */
    *p++ = '?';
  }
  *p = 0;
  return p;
}

/* Those pieces, then `text` folded from UTF-8, as one line of view v.
 * Every line said this way is someone speaking, and `b` is their nick:
 * it is painted in a colour of its own when drawn, one of eight by a
 * hash of the name, so a person keeps one colour for the session and
 * two people in a channel are told apart at a glance (step 3). The
 * words around it keep the text colour, so MEGA-F still changes every
 * word at once and the names stay theirs. */
static void say_text(unsigned char v, const char *a, const char *b, const char *c, const char *text)
{
  char *p = ui_cat(shown, SHOWN_END, a);
  unsigned char h = 0, n = 0;
  log_nick_at = (unsigned char)(p - shown);
  if (b) while (b[n]) { h = (unsigned char)(h * 5 + (unsigned char)b[n]); n++; }
  log_nick_len = n; log_nick_col = (unsigned char)(h & 7);
  p = ui_cat(p, SHOWN_END, b); p = ui_cat(p, SHOWN_END, c);
  fold(p, SHOWN_END, text ? text : "");
  view_line(v, shown);
}

static void draw_counts(void)
{
  char *t = LOW_COUNTS, *p = t, *e = t + LOW_COUNTS_CAP - 1;
  p = ui_cat(p, e, view_name(view_active));
  p = ui_cat(p, e, "  up "); p = ui_cat_num(p, e, up_h); p = ui_cat(p, e, up_m < 10 ? ":0" : ":");
  p = ui_cat_num(p, e, up_m); p = ui_cat(p, e, up_s < 10 ? ":0" : ":");
  p = ui_cat_num(p, e, up_s);
  p = ui_cat(p, e, "  rx "); p = ui_cat_num(p, e, rx_lines);
  p = ui_cat(p, e, "  tx "); p = ui_cat_num(p, e, tx_lines);
  if (view_back()) { p = ui_cat(p, e, "  back "); p = ui_cat_num(p, e, view_back()); }
  *p = 0;
  ui_line(ROW_COUNTS, t, 0);
}


/* ---- what is typed: the line, and the lines before it ------------------- */

static void hist_add(const char *s)
{
  lcopy((long)(unsigned int)s, HIST_BASE + (unsigned long)(hist_count % HIST_MAX) * HIST_SLOT, LOW_INPUT_CAP);
  hist_count++;
}

/* `back` of 1 is the line sent last; 0 is an empty line again. */
static void hist_recall(unsigned char back)
{
  if (!back) { input[0] = 0; input_len = input_pos = 0; return; }
  lcopy(HIST_BASE + (unsigned long)((unsigned char)(hist_count - back) % HIST_MAX) * HIST_SLOT,
        (long)(unsigned int)input, LOW_INPUT_CAP);
  input[LOW_INPUT_CAP - 1] = 0;
  for (input_len = 0; input[input_len]; input_len++) ;
  input_pos = input_len;
}

static void draw_input(void)
{
  char *t = LOW_SCRATCH;
  unsigned char n = 0, col, c, from = input_pos > 76 ? (unsigned char)(input_pos - 76) : 0;
  const char *s = input + from;
  t[n++] = '>'; t[n++] = ' ';
  while (*s && n < 79) t[n++] = *s++;
  t[n] = 0;
  ui_line(ROW_INPUT, t, 0);
  /* the cell under the cursor in reverse, which is bit 7 of the screen
   * code: the character stays legible under it, as an underscore in its
   * place would not (5.24) */
  col = (unsigned char)(2 + input_pos - from);
  c = input[input_pos] ? (unsigned char)m65_ascii_to_screencode(input[input_pos]) : 0x20;
  if (col < 80) lpoke(M65_SCREEN_RAM + (unsigned long)ROW_INPUT * 80 + col, (unsigned char)(c | 0x80));
}

/* ui_read_line with the text shown as asterisks: a NickServ password is
 * typed each session and never written to the disk, which is also how
 * this client is handed about (section 2, 5.24). */
static unsigned char read_secret(unsigned char row, const char *prompt, char *out, unsigned char cap)
{
  char *t = LOW_SCRATCH;
  unsigned char len = 0, key, n, i;
  const char *p;
  out[0] = 0;
  for (;;) {
    n = 0;
    for (p = prompt; *p && n < 78; p++) t[n++] = *p;
    for (i = 0; i < len && n < 78; i++) t[n++] = '*';
    t[n++] = '_'; t[n] = 0;
    ui_line(row, t, 0);
    key = ui_wait_key();
    if (key == KEY_RETURN) return 1;
    if (key == KEY_STOP) return 0;
    if (key == KEY_DEL) { if (len) out[--len] = 0; continue; }
    if (key >= 0x20 && key < 0x7f && len < cap) { out[len++] = (char)key; out[len] = 0; }
  }
}

/* ---- the wire ------------------------------------------------------------ */

static unsigned char send_all(const char *s)
{
  unsigned int n = 0;
  while (s[n]) n++;
  if (!conn_send((const unsigned char *)s, n)) return 0;
  tx_lines++;
  return 1;
}

static unsigned char send3(const char *a, const char *b, const char *c)
{
  char *p = tmp, *e = tmp + LINE_MAX - 2;      /* room kept for the CRLF and the NUL */
  p = ui_cat(p, e, a); p = ui_cat(p, e, b); p = ui_cat(p, e, c);
  p = ui_cat(p, tmp + LINE_MAX, "\r\n"); *p = 0;
  return send_all(tmp);
}

static unsigned char is_channel(const char *s) { return (unsigned char)(s && (*s == '#' || *s == '&')); }

/* The view a message about `target` belongs in: its channel's, else the status view. */
static unsigned char view_of(const char *target)
{
  unsigned char v = is_channel(target) ? view_find(target) : VIEW_NONE;
  return v == VIEW_NONE ? VIEW_STATUS : v;
}

/* The channel view for a name, opened if need be; VIEW_NONE if all seven are taken. */
static unsigned char view_for(const char *name)
{
  unsigned char v = view_find(name);
  if (v == VIEW_NONE) v = view_open(name);
  return v;
}

/* SASL PLAIN, when a password was given (step 3). CAP LS goes out before
 * NICK; a server that lists sasl gets CAP REQ, AUTHENTICATE PLAIN, then
 * the account and password base64 in one line, and CAP END after its
 * 903. One that lists nothing, refuses, or fails the login gets CAP END
 * and the NickServ IDENTIFY on 001 as before, so the one password
 * serves both and the older networks still work. Libera prefers this
 * and some networks and all Tor access require it. */
static unsigned char sasl_ok;                    /* 903 seen this connection: NickServ not needed */

static const char b64chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* n bytes at s as base64 at p, terminated. A bit at a time into a
 * six-bit accumulator: the textbook three-bytes-to-four needs shifts
 * by two, four and six, which a 6502 does a bit per instruction, and
 * two drafts of it came to 291 and 293 bytes; this one shifts by one
 * only. The count of characters out decides the '=' padding. (step 3) */
static __attribute__((noinline)) void b64(char *p, const unsigned char *s, unsigned char n)
{
  unsigned char acc = 0, bits = 0, out = 0, byte, i;
  while (n--) {
    byte = *s++;
    for (i = 0; i < 8; i++) {
      acc = (unsigned char)((acc << 1) | (byte >> 7)); byte <<= 1;
      if (++bits == 6) { *p++ = b64chars[acc & 63]; bits = 0; out++; }
    }
  }
  if (bits) { *p++ = b64chars[(acc << (6 - bits)) & 63]; out++; }
  while (out & 3) { *p++ = '='; out++; }
  *p = 0;
}

static __attribute__((noinline)) void cap_end(void) { send3("CAP END", 0, 0); }

/* The server's "AUTHENTICATE +" answered: nick NUL nick NUL password,
 * at most 66 bytes, as 88 of base64. `shown` is free between lines
 * said: the raw bytes in its second half, the text in its first. */
static __attribute__((noinline)) void sasl_answer(void)
{
  unsigned char *raw = (unsigned char *)shown + 128, *q = raw;
  const char *s;
  unsigned char part;
  for (part = 0; part < 3; part++) {
    if (part) *q++ = 0;                             /* the NUL between the fields */
    for (s = part == 2 ? nspass : nick; *s; ) *q++ = (unsigned char)*s++;
  }
  b64(shown, raw, (unsigned char)(q - raw));
  send3("AUTHENTICATE ", shown, 0);
}

static __attribute__((noinline)) unsigned char has_sasl(const char *s)
{
  for (; *s; s++) if (s[0] == 's' && s[1] == 'a' && s[2] == 's' && s[3] == 'l') return 1;
  return 0;
}

/* One line from the server, parsed and routed to its view. */
static void handle_line(void)
{
  irc_msg m;
  char *who, *text;
  unsigned char v;
  if (!irc_parse(line, &m)) return;
  who = m.prefix ? m.prefix : "";
  text = m.trailing ? m.trailing : "";
  if (irc_is(&m, "PING")) {
    send3("PONG :", m.trailing ? m.trailing : m.params[0], 0); tx_lines--;   /* not a line of ours to count */
    return;
  }
  if (irc_is(&m, "CAP")) {
    /* "CAP * LS :caps", "CAP * ACK :sasl", "CAP * NAK :sasl": the
     * subcommand's first letter tells them apart */
    unsigned char sub = m.nparams > 1 ? (unsigned char)(m.params[1][0] | 0x20) : 0;
    if (sub == 'l') { if (has_sasl(text)) send3("CAP REQ :sasl", 0, 0); else cap_end(); }
    else if (sub == 'a') send3("AUTHENTICATE PLAIN", 0, 0);
    else cap_end();
    return;
  }
  if (irc_is(&m, "AUTHENTICATE")) { sasl_answer(); return; }
  if (m.numeric == 903) { sasl_ok = 1; say(VIEW_STATUS, "-- SASL: logged in as ", nick, 0); cap_end(); return; }
  if (m.numeric >= 904 && m.numeric <= 907) { say(VIEW_STATUS, "-- SASL failed: ", text, "; NickServ instead"); cap_end(); return; }
  if (m.numeric == 1) {
    registered = 1;
    say(VIEW_STATUS, "-- registered as ", nick, 0);
    ui_status(channel[0] ? "registered" : "registered: /join #channel to talk", 0);
    /* the password goes straight out and is never echoed anywhere (5.24) */
    if (nspass[0] && !sasl_ok) { send3("PRIVMSG NickServ :IDENTIFY ", nspass, 0); say(VIEW_STATUS, "-- identifying to NickServ", 0, 0); }
  }
  if (m.numeric == 433) {                          /* nick in use: a digit on the end, again */
    unsigned char n = 0; while (nick[n]) n++;
    if (n < 15) { nick[n] = '2'; nick[n + 1] = 0; } else nick[n - 1]++;
    send3("NICK ", nick, 0);
  }
  if (irc_is(&m, "PRIVMSG") || irc_is(&m, "NOTICE")) {
    char tag[12];
    unsigned char notice = irc_is(&m, "NOTICE"), query;
    v = view_of(m.params[0]);
    query = (unsigned char)(!notice && !is_channel(m.params[0]));   /* to us, not a channel: no view of its own yet */
    if (irc_ctcp(&m, tag, sizeof tag)) {
      if (same_ci(tag, "ACTION")) say_text(v, "* ", who, " ", m.trailing);   /* other CTCP requests go unanswered: the bytes (5.18) */
      return;
    }
    if (notice) say_text(v, "-", who, "- ", m.trailing);
    else say_text(v, query ? "(private) <" : "<", who, "> ", m.trailing);
    return;
  }
  if (irc_is(&m, "JOIN")) {
    const char *chan = m.params[0] ? m.params[0] : text;
    if (same_ci(who, nick)) { v = view_for(chan); if (v != VIEW_NONE) { view_show(v); ui_status("joined ", chan); } say(v, "-- you have joined ", chan, 0); }
    else say(view_of(chan), "-- ", who, " has joined");
    return;
  }
  if (irc_is(&m, "PART") || irc_is(&m, "KICK")) {
    const char *chan = m.params[0];
    unsigned char kick = irc_is(&m, "KICK");
    const char *victim = kick ? m.params[1] : who;
    v = view_of(chan);
    say_text(v, "-- ", victim, kick ? " was kicked: " : " has left: ", m.trailing);
    if (same_ci(victim, nick)) view_close(v);
    return;
  }
  if (irc_is(&m, "QUIT")) { say(VIEW_STATUS, "-- ", who, " has quit"); return; }
  if (irc_is(&m, "NICK")) {
    const char *to = m.params[0] ? m.params[0] : text;
    if (same_ci(who, nick)) { unsigned char i; for (i = 0; to[i] && i < 16; i++) nick[i] = to[i]; nick[i] = 0; }
    say(VIEW_STATUS, who, " is now known as ", to);
    return;
  }
  if (irc_is(&m, "TOPIC") || m.numeric == 332) { say(view_of(m.numeric ? m.params[1] : m.params[0]), "-- topic: ", text, 0); return; }
  if (m.numeric == 353) { say(view_of(m.params[2]), "-- here: ", text, 0); return; }
  if (m.numeric == 366) return;
  if (irc_is(&m, "MODE")) { say(view_of(m.params[0]), "-- mode ", m.params[1], text); return; }
  /* everything else to the status view: the command or numeric, the parameters, the text */
  { char *p = shown, *e = shown + LOW_SHOWN_CAP - 1; unsigned char i;
    p = ui_cat(p, e, m.cmd);
    for (i = m.numeric ? 1 : 0; i < m.nparams; i++) { p = ui_cat(p, e, " "); p = ui_cat(p, e, m.params[i]); }
    if (m.trailing) { p = ui_cat(p, e, " "); fold(p, e, m.trailing); } else *p = 0;
    view_line(VIEW_STATUS, shown); }
}

/* Bytes from the connection, plaintext whether or not it is TLS. */
static void on_bytes(const unsigned char *p, unsigned int n)
{
  idle_frames = 0; pinged_idle = 0;
  while (n--) {
    unsigned char c = *p++;
    if (c == '\n') { if (line_len && line[line_len - 1] == '\r') line_len--; line[line_len] = 0; rx_lines++; handle_line(); line_len = 0; }
    else if (line_len < LINE_MAX) line[line_len++] = (char)c;
  }
}

/* ---- what is typed --------------------------------------------------------- */

/* A message or an action to a channel or a nick, sent and shown: in the
 * channel's view as the others' are, or as "-> target: text" in the
 * showing view when the target has no view here. */
static void privmsg(const char *target, const char *text, unsigned char action)
{
  unsigned char v = is_channel(target) ? view_find(target) : VIEW_NONE;
  char *p = tmp, *e = tmp + LINE_MAX - 2;
  p = ui_cat(p, e, "PRIVMSG "); p = ui_cat(p, e, target); p = ui_cat(p, e, action ? " :\001ACTION " : " :");
  p = ui_cat(p, e, text); if (action) p = ui_cat(p, e, "\001");
  p = ui_cat(p, tmp + LINE_MAX, "\r\n"); *p = 0;
  send_all(tmp);
  if (action) say_text(v == VIEW_NONE ? view_active : v, "* ", nick, " ", text);
  else if (v != VIEW_NONE) say_text(v, "<", nick, "> ", text);
  else say_text(view_active, "-> ", target, ": ", text);
}

/* Into a channel's view, opened if need be, and JOIN sent. `chan` may
 * lack its #, which is put on in place. */
static void join(char *chan)
{
  unsigned char v;
  if (!*chan) { say(view_active, "-- /join #channel", 0, 0); return; }
  if (!is_channel(chan)) { unsigned char n = 0; while (chan[n]) n++; if (n < 32) { chan[n + 1] = 0; while (n) { chan[n] = chan[n - 1]; n--; } chan[0] = '#'; } }
  v = view_for(chan);
  if (v == VIEW_NONE) { say(view_active, "-- no free view: /part one first", 0, 0); return; }
  view_show(v);
  send3("JOIN ", chan, 0);
}

/* /join, /part, /quit, /nick, /msg, /me, /raw; anything else refused,
 * never sent as text (section 2). `s` is the line past its slash: the
 * command word, then `arg`, the rest of the line. */
static void command(char *s)
{
  char *arg = s;
  while (*arg && *arg != ' ') arg++;
  if (*arg) *arg++ = 0;
  while (*arg == ' ') arg++;
  if (same_ci(s, "join") || same_ci(s, "j")) join(arg);
  else if (same_ci(s, "part") || same_ci(s, "p")) {
    const char *chan = is_channel(arg) ? arg : view_name(view_active);
    unsigned char v = view_find(chan);
    if (!is_channel(chan)) { say(view_active, "-- /part #channel", 0, 0); return; }
    send3("PART ", chan, 0);
    if (v != VIEW_NONE) view_close(v);              /* at once, not on the echo: a server that sends none would leave it open */
  } else if (same_ci(s, "quit") || same_ci(s, "q")) {
    send3("QUIT :", *arg ? arg : "the MEGA65 says goodbye", 0);
    quitting = 1;
  } else if (same_ci(s, "nick")) {
    if (*arg) send3("NICK ", arg, 0); else say(view_active, "-- /nick name", 0, 0);
  } else if (same_ci(s, "msg") || same_ci(s, "m")) {
    char *text = arg;
    while (*text && *text != ' ') text++;
    if (*text) *text++ = 0;
    while (*text == ' ') text++;
    if (*arg && *text) privmsg(arg, text, 0); else say(view_active, "-- /msg nick text", 0, 0);
  } else if (same_ci(s, "me")) {
    if (*arg && view_is_channel(view_active)) privmsg(view_name(view_active), arg, 1);
    else say(view_active, "-- /me needs a channel, and something to do", 0, 0);
  } else if (same_ci(s, "identify") || same_ci(s, "id")) {
    unsigned char i;
    if (*arg) { for (i = 0; arg[i] && i < LOW_NSPASS_CAP - 1; i++) nspass[i] = arg[i]; nspass[i] = 0; }
    if (nspass[0]) { send3("PRIVMSG NickServ :IDENTIFY ", nspass, 0); say(view_active, "-- identifying to NickServ", 0, 0); }
    else say(view_active, "-- /identify password", 0, 0);
  } else if (same_ci(s, "raw") || same_ci(s, "quote")) {
    if (*arg) { send3(arg, 0, 0); say(view_active, "-> ", arg, 0); }
  } else say(view_active, "-- unknown command: /", s, 0);
}

/* Each channel of a comma-separated list joined, in order. */
static void join_all(const char *list)
{
  char one[33];
  unsigned char n;
  while (*list) {
    for (n = 0; *list && *list != ',' && n < 31; list++) if (*list != ' ') one[n++] = *list;
    one[n] = 0;
    if (n) join(one);
    while (*list && *list != ',') list++;
    if (*list == ',') list++;
  }
}

static void typed_line(void)
{
  if (input[0] == '/') { command(input + 1); return; }
  if (view_is_channel(view_active)) privmsg(view_name(view_active), input, 0);
  else say(view_active, "-- no channel here: /join one", 0, 0);
}

/* ---- the connection ---------------------------------------------------------- */

/* Waits `secs`, polling the network, and reads the keyboard while it
 * waits: RUN/STOP returns 2 and any other key returns 1, so nothing here
 * leaves the client deaf. The minute between failed reconnects used to
 * read no key at all, RUN/STOP included, which is what "the shortcut
 * keys stopped responding" was (5.20). */
static unsigned char wait_secs(unsigned int secs)
{
  unsigned long w = 0, frames = (unsigned long)secs * FRAMES_PER_S;
  unsigned char lf = PEEK(0xd7fa), k;
  while (w < frames) {
    net_poll();
    k = ui_key();
    if (k == KEY_STOP) return 2;
    if (k) return 1;
    if (PEEK(0xd7fa) != lf) { lf = PEEK(0xd7fa); w++; }
  }
  return 0;
}

/* The clock, for the certificates' dates: trusted only if it is at least
 * the day this program was built, else the user is told to set it, by
 * hand or with mega-ntp, and the dates go unchecked (5.8). */
static void read_clock(void)
{
  clock_date d;
  now = 0;
  if (clock_read(&d) && clock_plausible(&d, CLOCK_BUILT_YY)) { clock_format(&d, LOW_NOW); now = LOW_NOW; return; }
  say(VIEW_STATUS, "-- the clock is unset: set it or run mega-ntp; certificate dates go unchecked", 0, 0);
}

static unsigned char connect_and_register(void)
{
  const char *err;
  unsigned char ip[4], tries;
  unsigned int port = 0;
  const char *s = port_text;
  while (*s >= '0' && *s <= '9') port = port * 10 + (unsigned int)(*s++ - '0');
  registered = 0; line_len = 0;
  /* One name, many servers: Libera hands out a different address each
   * time and serves ECDSA from about half of them (5.10), which this
   * client cannot check, so it dials again. Three attempts in a row was
   * both too few to get past a run of them and fast enough to look like
   * abuse, which is what the third attempt's silence was; five, spaced,
   * is the pair of changes (5.21). */
  for (tries = 0; ; tries++) {
    ui_status("looking up ", server);
    if (!net_resolve(server, ip, &err)) { ui_status("network: ", err); return 0; }
    ui_status("connecting...", 0);
    if (conn_open(server, ip, port, use_tls, now, &err)) break;
    /* RUN/STOP anywhere in here means leave the client, not skip this
     * server: cancelling one attempt and then waiting a minute for the
     * next is indistinguishable from being ignored (5.21) */
    if (conn_cancelled) { quitting = 1; return 0; }
    if (conn_retry && tries < 4) {
      say(VIEW_STATUS, "-- ", err, "; trying another server");
      ui_status("pausing before the next try", 0);
      if (wait_secs(3) == 2) { quitting = 1; return 0; }   /* a pause the server's throttle wants, and RUN/STOP is heard through it */
      continue;
    }
    ui_status("failed: ", err);
    say(VIEW_STATUS, "-- could not connect: ", err, 0);
    return 0;
  }
  if (use_tls) {
    char *p = tmp, *e = tmp + 79;
    p = ui_cat(p, e, "-- TLS: ");
    p = ui_cat_num(p, e, conn_certs);
    p = ui_cat(p, e, " certificates verified in "); p = ui_cat_num(p, e, conn_secs); p = ui_cat(p, e, " s");
    if (!now) p = ui_cat(p, e, ", dates unchecked");
    *p = 0; view_line(VIEW_STATUS, tmp);
  }
  ui_status("registering as ", nick);
  sasl_ok = 0;
  if (nspass[0] && !send3("CAP LS", 0, 0)) return 0;   /* the server holds registration until CAP END (step 3); the plain LS is one line */
  if (!send3("NICK ", nick, 0) || !send3("USER ", nick, " 0 * :MEGA65 IRC client")) return 0;
  return 1;
}

/* The bookmarks on the screen and one chosen: its fields into the
 * connection's, 1. RETURN alone (or none saved) is 0: the prompts
 * follow. "dN" removes entry N. */
static unsigned char pick_bookmark(void)
{
  char t[80], ans[4];
  unsigned char i, n;
  marks_load(boot_drive);
  for (;;) {
    ui_clear_rows(2, (unsigned char)(MARKS_MAX + 3));
    if (!marks_count) return 0;
    for (i = 0; i < marks_count; i++) {
      char *p = t, *e = t + 79;
      if (!marks_get(i, server, port_text, tls_text, nick, channel)) break;
      p = ui_cat_num(p, e, (unsigned long)(i + 1)); p = ui_cat(p, e, "  "); p = ui_cat(p, e, server);
      p = ui_cat(p, e, " "); p = ui_cat(p, e, port_text); p = ui_cat(p, e, (tls_text[0] | 0x20) == 'y' ? " TLS  " : " plain  ");
      p = ui_cat(p, e, nick); p = ui_cat(p, e, "  "); p = ui_cat(p, e, channel);
      *p = 0;
      ui_line((unsigned char)(3 + i), t, 0);
    }
    ans[0] = 0;
    ui_status("number connects; RETURN asks; dN deletes", 0);
    if (!ui_read_line(2, "Bookmark: ", ans, 3)) m65_exit_to_basic();
    if (!ans[0]) { ui_clear_rows(2, 2); return 0; }
    n = (unsigned char)((ans[0] == 'd' || ans[0] == 'D' ? ans[1] : ans[0]) - '1');
    if (n >= marks_count) continue;
    if (ans[0] == 'd' || ans[0] == 'D') { marks_remove(n); continue; }
    marks_get(n, server, port_text, tls_text, nick, channel);
    ui_clear_rows(2, (unsigned char)(MARKS_MAX + 3));
    return 1;
  }
}

static void session(void);

int main(void)
{
  const char *err;

  mega65_io_enable();
  /* the ROM's old screen page, $0800-$0FFF, holds this client's buffers
   * (lowram.h) and arrives full of screen-code spaces, not zeros: the
   * channel prompt once offered 63 blanks as its default (5.19). Cleared
   * whole, before the bank's INIT puts its RTI at $0F0F */
  lfill(0x0800UL, 0, 0x0800);
  m65_own_vectors();
  m65_screen_init();
  ui_line(0, "MEGA65 IRC client - version " IRCC_VERSION, 0);
  ui_status("loading mega-net...", 0);
  if (!net_load(&err)) { ui_status("network: ", err); for (;;) ; }
  ui_status("loading the TLS bank...", 0);
  if (!bank_boot(&err)) { ui_status("TLS: ", err); for (;;) ; }
  /* only now: log_init lives in the HIGH window bank_boot has just loaded (5.17) */
  if (!log_init()) { ui_status("no attic RAM: this client needs the 8 MB expansion", 0); for (;;) ; }
  ui_status("gathering randomness...", 0);
  rnd_init();
  ui_status("waiting for a lease...", 0);
  if (!net_dhcp(&err)) { ui_status("network: ", err); for (;;) ; }
  conn_on_data = on_bytes;
  for (;;) session();                             /* a session, then the first screen again (5.23) */
  return 0;
}

/* One visit to the first screen and the session it begins. RUN/STOP in
 * a session comes back here, where another server or another bookmark
 * can be chosen; RUN/STOP at the first screen itself is the only way
 * out of the client, which is the user's model of it (5.23). */
static void session(void)
{
  unsigned char k, f, joined = 0;

  rx_lines = tx_lines = 0;
  idle_frames = 0; up_h = up_m = up_s = frame_in_s = 0;
  registered = pinged_idle = quitting = 0;
  line_len = 0; input_len = input_pos = 0; input[0] = 0;
  hist_count = hist_at = 0;                       /* the lines sent belong to the session that sent them */

  ui_clear_rows(0, (unsigned char)(m65_screen_rows() - 1));
  ui_line(0, "MEGA65 IRC client - version " IRCC_VERSION, 0);
  ui_line(UI_ROW_KEYS, "RUN/STOP quits to BASIC", 0);
  if (!pick_bookmark()) {                         /* the prompts, when no bookmark was chosen */
    ui_status("RETURN to accept entry", 0);
    if (!ui_read_line(3, "Server: ", server, 63)) m65_exit_to_basic();
    if (!ui_read_line(4, "Port: ", port_text, 5)) m65_exit_to_basic();
    if (!ui_read_line(5, "TLS (y/n): ", tls_text, 1)) m65_exit_to_basic();
    if (!ui_read_line(6, "Nick: ", nick, 16)) m65_exit_to_basic();
    /* the channels are optional: none, and the connection sits in the
     * status view with the MOTD, /join to hand (section 2); a bare name
     * gets its # */
    if (!ui_read_line(7, "Channels: ", channel, 63)) m65_exit_to_basic();
    if (channel[0] && !is_channel(channel)) {
      unsigned char n = 0; while (channel[n]) n++;
      if (n < 63) { channel[n + 1] = 0; while (n) { channel[n] = channel[n - 1]; n--; } channel[0] = '#'; }
    }
    { char yn[2] = "n";
      if (marks_count < MARKS_MAX && ui_read_line(9, "Save as a bookmark (y/n): ", yn, 1) && (yn[0] | 0x20) == 'y')
        ui_status(marks_add(server, port_text, tls_text, nick, channel) ? "saved to IRC.CFG" : "could not write IRC.CFG", 0); }
  }
  /* asked whichever way the server was chosen, since a bookmark carries
   * no password and never will (section 2); RETURN alone skips it */
  if (!read_secret(11, "Password, SASL or NickServ (RETURN for none): ", nspass, LOW_NSPASS_CAP - 1)) m65_exit_to_basic();
  use_tls = (unsigned char)((tls_text[0] | 0x20) == 'y');
  ui_clear_rows(1, ROW_COUNTS);
  view_init();
  say(VIEW_STATUS, "-- F1-F8 views  F9/F11 scroll back/forward  HOME newest  CRSR history  MEGA-B/F colors  RUN/STOP leaves", 0, 0);
  if (use_tls) read_clock();
  if (!connect_and_register()) { ui_line(UI_ROW_KEYS, "any key", 0); ui_wait_key(); return; }
  draw_input();
  last_frame = PEEK(0xd7fa);

  while (!quitting) {
    conn_pump();
    f = PEEK(0xd7fa);
    if (f != last_frame) {                          /* once a frame: the clocks, the counts once a second */
      last_frame = f; idle_frames++;
      if (++frame_in_s == FRAMES_PER_S) {
        frame_in_s = 0;
        if (++up_s == 60) { up_s = 0; if (++up_m == 60) { up_m = 0; up_h++; } }
        draw_counts();
        if (status_ttl && !--status_ttl) status_clear();   /* the notice has been read; the row goes back to blank */
      }
    }
    if (registered && !joined) { joined = 1; join_all(channel); }
    if (idle_frames >= (unsigned long)IDLE_PING_S * FRAMES_PER_S && !pinged_idle) { send3("PING :alive", 0, 0); tx_lines--; pinged_idle = 1; }
    if (idle_frames >= (unsigned long)DEAD_S * FRAMES_PER_S || !conn_alive()) {
      /* to the status view, where the reconnection is told: a user
       * watching a channel saw nothing happen at all, which is part of
       * what "the shortcut keys stopped responding" looked like (5.20) */
      view_show(VIEW_STATUS);
      say(VIEW_STATUS, "-- the link is dead: reconnecting", 0, 0);
      conn_abort(); idle_frames = 0; pinged_idle = 0; joined = 0;
      draw_counts();                                /* the counts row names the view, and the view just changed */
      if (!connect_and_register()) {
        if (quitting) break;                        /* RUN/STOP during the attempt, not a failure to wait out */
        say(VIEW_STATUS, "-- no reconnect; any key to retry, RUN/STOP to leave", 0, 0);
        if (wait_secs(60) == 2) break;
      }
      continue;
    }
    k = ui_key();
    if (!k) continue;
    rnd_stir(k);
    if (k >= 0xc1 && k <= 0xda && (ui_last_mods & MOD_MEGA)) {   /* MEGA+letter: the capital with bit 7 set (ssh 5.29), as mega-ftp binds them */
      k = (unsigned char)(k & 0x7f);
      if (k == 'B') m65_screen_cycle_background();
      else if (k == 'F') { m65_screen_cycle_text_colour(); view_show(view_active); }   /* the fill took the nicks' colours too; the redraw paints them back (step 3) */
      continue;
    }
    if (k >= KEY_F1 && k <= KEY_F8) {
      /* F1 F3 F5 F7 F2 F4 F6 F8, so the first four views need no SHIFT
       * (section 2). The codes run in label order, so the unshifted keys
       * are the even offsets and the shifted ones the odd: F1 F3 F5 F7
       * are positions 0 to 3, F2 F4 F6 F8 are 4 to 7 (5.23), and a
       * position is a view only while one sits there. An empty one
       * says so rather than swallowing the key (5.20). */
      unsigned char n = (unsigned char)(k - KEY_F1), v;
      v = view_at((unsigned char)((n & 1) ? 4 + (n >> 1) : (n >> 1)));   /* the key is a position; the view module says which view sits there (5.29) */
      if (v != VIEW_NONE) { view_show(v); draw_counts(); status_clear(); }   /* a notice about the old view is stale in the new one */
      else ui_status("no view there yet", 0);
      continue;
    }
    /* the scrollback, a page at a time: F9 back, F11 forward. It was
     * MEGA and the cursor keys, which the user found dead and which the
     * driver cannot send at all -- $D611's modifier bits are out of its
     * reach, only key codes are -- so it moved to two keys nothing else
     * uses and this side can press (5.26). The "back N" on the counts
     * row is the feedback: it stops growing at the oldest line kept. */
    if (k == KEY_F9 || k == KEY_F11) {
      view_scroll(k == KEY_F9 ? (int16_t)(view_rows() - 2) : (int16_t)-(view_rows() - 2));
      draw_counts(); continue;
    }
    if (k == KEY_UP || k == KEY_DOWN) {             /* the lines already sent, as every other IRC client does (5.24) */
      unsigned char have = hist_count < HIST_MAX ? hist_count : HIST_MAX;
      if (k == KEY_UP) { if (hist_at < have) hist_recall(++hist_at); }
      else if (hist_at) hist_recall(--hist_at);
      draw_input();
      continue;
    }
    if (k == KEY_LEFT) { if (input_pos) input_pos--; draw_input(); continue; }
    if (k == KEY_RIGHT) { if (input_pos < input_len) input_pos++; draw_input(); continue; }
    if (k == KEY_HOME) { view_scroll(0); draw_counts(); continue; }
    if (k == KEY_STOP) { send3("QUIT :the MEGA65 says goodbye", 0, 0); break; }
    if (k == KEY_RETURN) {
      if (input_len) { view_scroll(0); hist_add(input); hist_at = 0; typed_line(); input_len = input_pos = 0; input[0] = 0; }
      draw_input();
    } else if (k == KEY_DEL) {                      /* backspace, at the cursor rather than the end */
      if (input_pos) { unsigned char i; input_pos--; for (i = input_pos; i < input_len; i++) input[i] = input[i + 1]; input_len--; }
      draw_input();
    }
    else if (k >= 0x20 && k < 0x7f && input_len < LOW_INPUT_CAP - 1) {
      unsigned char i;
      for (i = input_len; i > input_pos; i--) input[i] = input[i - 1];   /* insert, so a correction mid-line is possible */
      input[input_pos++] = (char)k; input_len++; input[input_len] = 0;
      draw_input();
    }
  }
  conn_close();
  wait_secs(2);                                   /* the QUIT and the close_notify on their way */
  conn_abort();                                   /* and the socket freed before the next session */
}
