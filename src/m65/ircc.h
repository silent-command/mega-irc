/* What ircc.c shares with the command module (cmd.c), which lives in
 * the HIGH window under the KERNAL and is compiled apart, so these are
 * ordinary calls across the boundary rather than LTO's inlining (5.32).
 * The buffers are the low-RAM ones (lowram.h); the status macro arms
 * the fade at every call site (5.28). */
#ifndef IRCC_H
#define IRCC_H

#include "ui.h"
#include "irc.h"
#include "m65/lowram.h"

#define IRCC_VERSION "0.2.2"
#define LINE_MAX 512                 /* RFC 1459: 510 plus the CRLF */

#define shown LOW_SHOWN              /* a line composed for the screen */
#define SHOWN_END (shown + LOW_SHOWN_CAP - 1)
#define tmp LOW_TMP                  /* a line being composed to send, 513 bytes */
#define nspass LOW_NSPASS            /* the NickServ password, this session's only (section 2) */

#define STATUS_SECS 5
extern unsigned char status_ttl;
#define ui_status(a, b) (status_ttl = STATUS_SECS, (ui_status)(a, b))
#define status_clear() (status_ttl = 0, (ui_status)(0, 0))

extern char nick[17];
extern unsigned char quitting;

/* ircc.c */
char *compose(const char *a, const char *b, const char *c);   /* two or three pieces into `shown`; where the next goes */
void say(unsigned char v, const char *a, const char *b, const char *c);   /* those pieces as one line of view v */
char *fold(char *p, char *end, const char *s);               /* UTF-8 reduced to a byte a character */
void say_text(unsigned char v, const char *a, const char *b, const char *c, const char *text);   /* b is a nick, coloured */
unsigned char send_all(const char *s);
unsigned char send3(const char *a, const char *b, const char *c);   /* the pieces and a CRLF, sent */
unsigned char is_channel(const char *s);
unsigned char view_of(const char *target);                   /* the view a message about `target` belongs in */
unsigned char view_for(const char *name);                    /* the view for a name, opened if need be; VIEW_NONE if all are taken */
void privmsg(const char *target, const char *text, unsigned char action);
void join(char *chan);

/* cmd.c */
void cmd_init(void);                                         /* once, after the HIGH window is in place */
void cmd_line(char *s);                                      /* a typed line past its slash */
void cmd_said(irc_msg *m, const char *who);                  /* a PRIVMSG or NOTICE, routed and shown */
unsigned char cmd_routed(irc_msg *m, const char *who, const char *text);   /* the other lines cmd.c knows; 0 if not one of them */

#endif
