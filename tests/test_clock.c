/* The host suite for src/clock.c: is the machine's clock worth checking
 * certificate dates against, and does it format the way chain_valid_at
 * compares?
 *
 * The threshold is passed in rather than taken from __DATE__, so these
 * checks mean the same thing whenever they are compiled. */
#include <stdio.h>
#include <string.h>
#include "clock.h"

static int checks, failed;
#define CHECK(cond, what) do { checks++; if (!(cond)) { failed++; printf("FAIL line %d: %s\n", __LINE__, what); } } while (0)

static clock_date at(uint8_t yy, uint8_t mm, uint8_t dd, uint8_t hh, uint8_t mi, uint8_t ss)
{
  clock_date d; d.yy = yy; d.mm = mm; d.dd = dd; d.hh = hh; d.mi = mi; d.ss = ss; return d;
}

int main(void)
{
  clock_date d;
  char out[13];

  /* ---- a clock that is set ---- */
  d = at(26, 9, 22, 13, 45, 56);
  CHECK(clock_plausible(&d, 26), "the time the machine actually read today");
  CHECK(clock_plausible(&d, 20), "and against an older build");

  /* ---- the ways a clock is plainly unset ---- */
  d = at(0, 0, 0, 0, 0, 0);
  CHECK(!clock_plausible(&d, 26), "all zeros, which is what a chip that has never been set gives");
  d = at(0, 1, 1, 0, 0, 0);
  CHECK(!clock_plausible(&d, 26), "year 2000 on a 2026 build");
  d = at(25, 12, 31, 23, 59, 59);
  CHECK(!clock_plausible(&d, 26), "the year before the software was built");
  d = at(26, 1, 1, 0, 0, 0);
  CHECK(clock_plausible(&d, 26), "the build year itself is the boundary and passes");

  /* ---- nonsense in the fields ---- */
  d = at(26, 0, 22, 13, 45, 56);  CHECK(!clock_plausible(&d, 26), "month 0");
  d = at(26, 13, 22, 13, 45, 56); CHECK(!clock_plausible(&d, 26), "month 13");
  d = at(26, 9, 0, 13, 45, 56);   CHECK(!clock_plausible(&d, 26), "day 0");
  d = at(26, 9, 32, 13, 45, 56);  CHECK(!clock_plausible(&d, 26), "day 32");
  d = at(26, 9, 31, 13, 45, 56);  CHECK(clock_plausible(&d, 26), "day 31 in a 30-day month is let through: the CA's dates decide, not ours");
  d = at(26, 9, 22, 24, 45, 56);  CHECK(!clock_plausible(&d, 26), "hour 24");
  d = at(26, 9, 22, 23, 60, 56);  CHECK(!clock_plausible(&d, 26), "minute 60");
  d = at(26, 9, 22, 23, 59, 60);  CHECK(clock_plausible(&d, 26), "second 60 is a leap second and legal");
  d = at(26, 9, 22, 23, 59, 61);  CHECK(!clock_plausible(&d, 26), "second 61 is not");

  /* ---- absurdly far ahead is unset too ---- */
  d = at(46, 9, 22, 13, 45, 56);
  CHECK(clock_plausible(&d, 26), "twenty years ahead is still allowed");
  d = at(47, 9, 22, 13, 45, 56);
  CHECK(!clock_plausible(&d, 26), "twenty-one is not");

  /* ---- the century, where a two-digit year wraps ---- */
  d = at(95, 6, 1, 12, 0, 0);
  CHECK(clock_plausible(&d, 90), "a 2090s build with a 2095 clock");
  d = at(99, 6, 1, 12, 0, 0);
  CHECK(clock_plausible(&d, 90), "and 2099, where min+20 would have wrapped to 14 and inverted the test");
  d = at(14, 6, 1, 12, 0, 0);
  CHECK(!clock_plausible(&d, 90), "a 2014 clock on a 2090s build is still refused");

  /* ---- the shape chain_valid_at compares ---- */
  d = at(26, 9, 22, 13, 45, 56);
  clock_format(&d, out);
  CHECK(!strcmp(out, "260922134556"), "twelve digits, YYMMDDHHMMSS");
  CHECK(strlen(out) == 12, "and nothing more");
  d = at(6, 1, 2, 3, 4, 5);
  clock_format(&d, out);
  CHECK(!strcmp(out, "060102030405"), "every field is padded to two digits");

  /* ---- and that it sorts the way the comparison needs ---- */
  {
    char a[13], b[13];
    clock_date e = at(26, 9, 22, 13, 45, 56), f = at(26, 10, 1, 0, 0, 0);
    clock_format(&e, a); clock_format(&f, b);
    CHECK(strcmp(a, b) < 0, "an earlier time sorts before a later one as plain digits");
  }

  printf("%d checks, %d failed\n", checks, failed);
  return failed != 0;
}
