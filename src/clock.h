/* The machine's clock, and whether it is worth believing.
 *
 * Certificate validity needs a date (REQUIREMENTS.md 5.8), and this
 * machine may simply not have one set. The user settled what to do on
 * 2026-09-22: say so, and let them fix it by hand or with mega-ntp.
 * Refusing every certificate when the clock is unset would be wrong,
 * and skipping the check silently would be worse than saying so.
 *
 * The threshold for "plainly unset" is the day this program was built.
 * A clock reading earlier than its own software cannot be right, and
 * unlike a constant in a header that threshold never goes stale.
 * __DATE__ is "Mmm DD YYYY", so the year's last two digits sit at 9
 * and 10.
 */
#ifndef CLOCK_H
#define CLOCK_H

#include <stdint.h>

#define CLOCK_BUILT_YY ((uint8_t)((__DATE__[9] - '0') * 10 + (__DATE__[10] - '0')))

/* Two-digit year, as UTCTime carries it and as chain_valid_at compares. */
typedef struct {
  uint8_t yy, mm, dd, hh, mi, ss;
} clock_date;

/* 1 if this could be a real time, at or after `min_yy`. The threshold is
 * a parameter rather than CLOCK_BUILT_YY so the host suite does not
 * depend on the day it was compiled. */
uint8_t clock_plausible(const clock_date *d, uint8_t min_yy);

/* Twelve digits, YYMMDDHHMMSS, which is what chain_valid_at wants. */
void clock_format(const clock_date *d, char out[13]);

#ifdef __mos__
/* Reads the machine's clock. 1 if it is worth checking dates against, 0
 * if it is plainly unset, in which case the caller says so on screen and
 * verifies the chain without the dates. */
uint8_t clock_read(clock_date *d);
#endif

#endif
