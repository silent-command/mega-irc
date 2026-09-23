#include "clock.h"

uint8_t clock_plausible(const clock_date *d, uint8_t min_yy)
{
  uint8_t hi;

  if (d->mm < 1 || d->mm > 12) return 0;
  if (d->dd < 1 || d->dd > 31) return 0;         /* the month's own length is the CA's problem, not ours */
  if (d->hh > 23 || d->mi > 59) return 0;
  if (d->ss > 60) return 0;                      /* 60 is a leap second, and legal */
  if (d->yy < min_yy) return 0;                  /* older than its own software: unset */

  /* Twenty years ahead is generous and still catches a clock that has
   * come up as nonsense. The addition is clamped because a two-digit
   * year wraps at the century, and a wrap would quietly invert the
   * test rather than widen it. */
  hi = (uint8_t)(min_yy + 20);
  if (hi < min_yy) hi = 99;
  if (d->yy > hi) return 0;

  return 1;
}

static void two(char *p, uint8_t v)
{
  p[0] = (char)('0' + v / 10);
  p[1] = (char)('0' + v % 10);
}

void clock_format(const clock_date *d, char out[13])
{
  two(out + 0, d->yy);
  two(out + 2, d->mm);
  two(out + 4, d->dd);
  two(out + 6, d->hh);
  two(out + 8, d->mi);
  two(out + 10, d->ss);
  out[12] = 0;
}

#ifdef __mos__
#include "mega65/time.h"

uint8_t clock_read(clock_date *d)
{
  struct m65_tm tm;

  getrtc(&tm);
  /* The library hands the chip's month straight through as 1 to 12 and
   * the year from 1900, whatever its header says about 0 to 11; that is
   * what mega-ntp's rtc_read found and depends on. */
  d->yy = (uint8_t)(((uint16_t)(tm.tm_year + 1900)) % 100);
  d->mm = tm.tm_mon;
  d->dd = tm.tm_mday;
  d->hh = tm.tm_hour;
  d->mi = tm.tm_min;
  d->ss = tm.tm_sec;
  return clock_plausible(d, CLOCK_BUILT_YY);
}
#endif
