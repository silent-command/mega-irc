/* The views: the status window and up to seven channels, one on the
 * screen at a time, each with its scrollback in the log (log.h). The
 * chat area is rows 1 to the counts row; row 0 is the bar of views on
 * their function keys, F1 F3 F5 F7 F2 F4 F6 F8 -- a mapping this client
 * makes, not the keyboard's (5.23), and one that closes up when a view
 * closes, so the keys never skip (5.29). */
#ifndef VIEW_H
#define VIEW_H
#include <stdint.h>

#define VIEW_MAX 8
#define VIEW_STATUS 0
#define VIEW_NONE 0xff
#define VIEW_NAME_CAP 33
#define VIEW_ROW_FIRST 1
/* the chat rows: from row 1 to the row above the counts row */
uint8_t view_rows(void);

extern uint8_t view_active;

void view_init(void);                          /* the status view, shown */
uint8_t view_open(const char *name);           /* the first free slot, or VIEW_NONE */
void view_close(uint8_t v);                    /* back to the status view if it was showing */
uint8_t view_find(const char *name);           /* case-insensitively; VIEW_NONE if no such view */
uint8_t same_ci(const char *a, const char *b); /* the compare it uses: ASCII case folded; 0 if either is null */
const char *view_name(uint8_t v);
uint8_t view_at(uint8_t pos);                  /* the view on key position pos (0 = F1, 1 = F3, ...), or VIEW_NONE */
uint8_t view_is_channel(uint8_t v);            /* a channel, not the status view */

/* A line of text into view v: wrapped at 79 columns, the continuation
 * indented, each row logged; drawn at once if v is showing and not
 * scrolled back, else the view is starred on the bar. An unknown v
 * goes to the status view. */
void view_line(uint8_t v, const char *text);
void view_show(uint8_t v);                     /* v onto the screen, its rows redrawn from the log */
void view_scroll(int16_t rows);                /* the showing view back (positive) or forward; clamped; 0 rows = to the newest */
uint16_t view_back(void);                      /* how far back the showing view is scrolled */
void view_bar(void);                           /* row 0: the views on their keys, the showing one in brackets, unread ones starred */

#endif
