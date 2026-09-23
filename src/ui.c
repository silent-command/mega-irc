#include "mega65/memory.h"
#include "m65_screen.h"
#include "netutil.h"
#include "ui.h"

void (*ui_idle)(void);
unsigned char ui_last_mods;

static char row_buf[81];
static unsigned char last_frame;

unsigned char ui_key(void)
{
  unsigned char k = PEEK(0xd610);
  if (k) { ui_last_mods = PEEK(0xd611); POKE(0xd610, 0); }   /* the modifiers, then the pop */
  return k;
}

unsigned char ui_wait_key(void)
{
  unsigned char k;
  for (;;) {
    k = ui_key();
    if (k) return k;
    net_poll();
    if (PEEK(0xd7fa) != last_frame) {
      last_frame = PEEK(0xd7fa);
      if (ui_idle) ui_idle();
    }
  }
}

/* Builds a+b padded with spaces to 79 columns: an 80th character would
 * wrap onto the next row and overwrite it. */
static void build_row(const char *a, const char *b)
{
  unsigned char n = 0;
  if (a) while (*a && n < 79) row_buf[n++] = *a++;
  if (b) while (*b && n < 79) row_buf[n++] = *b++;
  while (n < 79) row_buf[n++] = ' ';
  row_buf[n] = 0;
}

void ui_line(unsigned char row, const char *a, const char *b)
{
  build_row(a, b);
  m65_putsxy(0, row, row_buf);
}

void ui_status(const char *a, const char *b)
{
  ui_line(UI_ROW_STATUS, a, b);
}

void ui_clear_rows(unsigned char from, unsigned char to)
{
  while (from <= to) { ui_line(from, 0, 0); from++; }
}

unsigned char ui_read_line(unsigned char row, const char *prompt, char *out, unsigned char maxlen)
{
  static char shown[81];
  unsigned char len, key, n;
  unsigned char fresh = 1;                      /* the default is still untouched */
  const char *p;

  out[maxlen] = 0;
  for (len = 0; out[len]; len++) ;
  for (;;) {
    n = 0;
    for (p = prompt; *p && n < 78; p++) shown[n++] = *p;
    for (p = out; *p && n < 78; p++) shown[n++] = *p;
    shown[n++] = '_';
    shown[n] = 0;
    ui_line(row, shown, 0);

    key = ui_wait_key();
    if (key == KEY_RETURN) return 1;
    if (key == KEY_STOP) return 0;
    if (key == KEY_DEL) { fresh = 0; if (len) out[--len] = 0; continue; }
    if (key >= 0x20 && key < 0x7f) {
      if (fresh) { len = 0; fresh = 0; }         /* typing replaces the offered default */
      if (len < maxlen) { out[len++] = (char)key; out[len] = 0; }
    }
  }
}

void ui_put_ulong(char *p, unsigned long v)
{
  char t[11];
  unsigned char n = 0;
  do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v);
  while (n) *p++ = t[--n];
  *p = 0;
}

/* Bounded writers. Every buffer here is fixed and some of what goes into
 * them comes off the network, where a line may be 512 bytes; a pair
 * without end pointers once ran off two buffers (irc 5.3). Neither
 * writes the terminator: the caller does, once. */
char *ui_cat(char *p, char *end, const char *s)
{
  if (s) while (*s && p < end) *p++ = *s++;
  return p;
}

/* Decimal by subtracting powers of ten, not by dividing: a 32-bit
 * division is __udivmodsi4, 800 bytes of it (gemini 5.8). */
char *ui_cat_num(char *p, char *end, unsigned long v)
{
  static const unsigned long pow10[9] = { 1000000000UL, 100000000UL, 10000000UL, 1000000UL, 100000UL, 10000UL, 1000UL, 100UL, 10UL };
  unsigned char i, d, started = 0;
  for (i = 0; i < 9; i++) {
    d = 0;
    while (v >= pow10[i]) { v -= pow10[i]; d++; }
    if (d || started) { started = 1; if (p < end) *p++ = (char)('0' + d); }
  }
  if (p < end) *p++ = (char)('0' + (unsigned char)v);
  return p;
}
