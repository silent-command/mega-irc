#include "mega65/memory.h"
#include "log.h"

#define LOG_BASE 0x8000000UL     /* the attic; gemini keeps its document store from here too, one program at a time */
#define LOG_STRIDE 20            /* 1 MB a view: the view number shifted by this */
#define LOG_RING 8192U           /* rows a view keeps; a power of two, so the slot is a mask and 65536 is a whole number of rings */
#define LOG_SLOT 96              /* a row's slot: its 80 screen codes, the nick's span (3 bytes), spare; 8192 slots are 768 KB of the megabyte, and view 0 keeps the editor's history above that (ircc.c HIST_BASE) */
#define SCREEN 0x10000UL
#define COLOUR 0xff80000UL

static uint16_t total[LOG_VIEWS];   /* rows ever logged, wrapping: the next slot is total & (LOG_RING - 1) */
static uint16_t have[LOG_VIEWS];    /* rows on hand: total until the ring is full */
uint8_t log_nick_at, log_nick_len, log_nick_col;

/* Eight colours a nick can have, none of them the usual text colours
 * and none dark: cyan, green, yellow, orange, pink, light green, light
 * blue, light grey. A hash picks one; the one that matches the
 * background is stepped past when drawn, so a MEGA-B can hide no name. */
static const uint8_t palette[8] = { 3, 5, 7, 8, 10, 13, 14, 15 };

static uint32_t slot(uint8_t v, uint16_t i)
{
  return LOG_BASE + ((uint32_t)v << LOG_STRIDE) + (uint32_t)(i & (LOG_RING - 1)) * LOG_SLOT;
}

uint8_t log_init(void)
{
  /* two bytes written and read back, then one changed: a missing attic
   * reads as whatever floats, which passes one pattern more often than
   * it passes both (gemini's doc_init) */
  lpoke(LOG_BASE, 0x5a); lpoke(LOG_BASE + 1, 0xa5);
  if (lpeek(LOG_BASE) != 0x5a || lpeek(LOG_BASE + 1) != 0xa5) return 0;
  lpoke(LOG_BASE, 0xa5);
  return (uint8_t)(lpeek(LOG_BASE) == 0xa5);
}

void log_clear(uint8_t v) { total[v] = 0; have[v] = 0; }

void log_row(uint8_t v, unsigned char *row)
{
  /* the span rides in the three bytes after the row, one copy for both;
   * a separate ring for it cost a second address and six byte calls,
   * 360 bytes of this window (step 3) */
  row[LOG_ROW] = log_nick_at; row[LOG_ROW + 1] = log_nick_len; row[LOG_ROW + 2] = log_nick_col;
  log_nick_len = 0;
  lcopy((long)(unsigned int)row, (long)slot(v, total[v]), LOG_ROW + 3);
  total[v]++;
  if (have[v] < LOG_RING) have[v]++;
}

uint16_t log_count(uint8_t v) { return have[v]; }

void log_draw(uint8_t v, uint16_t top, uint8_t screen_row, uint8_t n)
{
  uint16_t i = (uint16_t)(total[v] - have[v] + top);   /* the slot of row `top` from the oldest kept; wraps with total */
  uint16_t off = (uint16_t)screen_row * LOG_ROW;       /* the row's place in the screen and in the colour RAM alike */
  uint32_t at;
  uint8_t s[3], c;
  while (n--) {
    if (top < have[v]) {
      at = slot(v, i);
      lcopy((long)at, (long)(SCREEN + off), LOG_ROW);
      lcopy((long)(at + LOG_ROW), (long)(unsigned int)s, 3);
      if (s[1]) {
        c = s[2];
        if (palette[c & 7] == (PEEK(0xd021) & 0x0f)) c++;
        lfill(COLOUR + off + s[0], palette[c & 7], s[1]);
      }
    }
    else lfill(SCREEN + off, 0x20, LOG_ROW);
    i++; top++; off += LOG_ROW;
  }
}
