/* The scrollback: every view's rows in attic RAM, each row the 80 screen
 * codes the chat area shows, so a page is a copy and a view switch and
 * a scroll are the same redraw (REQUIREMENTS.md section 2). A view has
 * 1 MB, a ring of 8192 rows; the oldest are overwritten. Rows are
 * counted in 16 bits: the ring is smaller than that, and 32-bit
 * arithmetic here cost 900 bytes of the HIGH window (5.18). */
#ifndef LOG_H
#define LOG_H
#include <stdint.h>

#define LOG_VIEWS 8
#define LOG_ROW 80

/* 1 if the attic (the 8 MB expansion) answers; without it there is no scrollback and no client. */
uint8_t log_init(void);
void log_clear(uint8_t v);
/* One row, LOG_ROW screen codes, appended to view v; the three bytes
 * after them are the caller's too, and are written with the nick's span. */
void log_row(uint8_t v, unsigned char *row);
/* Rows on hand for view v: at most the ring's 8192. */
uint16_t log_count(uint8_t v);
/* Rows top .. top + n - 1 of view v, counted from the oldest kept, onto
 * screen rows from `screen_row`, blank past the newest; the colour under
 * them is the caller's to set, and a row's nick is then painted in its
 * own colour over that. */
void log_draw(uint8_t v, uint16_t top, uint8_t screen_row, uint8_t n);

/* The nick on the next row logged: `log_nick_len` cells from column
 * `log_nick_at`, in palette entry `log_nick_col` (0-7), painted over
 * the text colour whenever the row is drawn, so a name keeps its colour
 * through a scroll and a MEGA-F. log_row clears the length, so the
 * continuation rows of a wrapped line and the lines after it carry
 * none. Kept beside the ring, 3 bytes a row (step 3). */
extern uint8_t log_nick_at, log_nick_len, log_nick_col;

#endif
