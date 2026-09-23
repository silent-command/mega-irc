#include "der.h"

/* ---- the certificate being walked ------------------------------------
 * One at a time, selected by the caller. The first draft handed a
 * (read, ctx, len) triple to every call; this is the same walker x509.c
 * has, which is static there and so cannot be borrowed, that file being
 * a copy of ../mega-gemini's and staying identical to it. */
static x509_read c_read;
static void *c_ctx;
static uint16_t c_len;

void chain_select(x509_read read, void *ctx, uint16_t len) { c_read = read; c_ctx = ctx; c_len = len; }
void der_read(uint16_t off, uint8_t *dst, uint16_t n) { c_read(c_ctx, off, dst, n); }

uint8_t der_elem(uint16_t at, uint16_t end, der *d)
{
  uint8_t h[4];
  uint16_t n;
  if (end < 2 || at > (uint16_t)(end - 2)) return 0;
  der_read(at, h, (uint16_t)(at + 4 <= end ? 4 : end - at));
  d->tag = h[0];
  if (h[1] < 0x80) { n = h[1]; at = (uint16_t)(at + 2); }
  else if (h[1] == 0x81) { n = h[2]; at = (uint16_t)(at + 3); }
  else if (h[1] == 0x82) { n = (uint16_t)(((uint16_t)h[2] << 8) | h[3]); at = (uint16_t)(at + 4); }
  else return 0;                                   /* longer than 64 KB, or indefinite */
  if (at > end || n > (uint16_t)(end - at)) return 0;
  d->off = at; d->len = n;
  return 1;
}

uint16_t der_after(const der *d) { return (uint16_t)(d->off + d->len); }

uint8_t der_tbs(der *c, der *t)
{
  if (!der_elem(0, c_len, c) || c->tag != 0x30) return 0;
  return (uint8_t)(der_elem(c->off, der_after(c), t) && t->tag == 0x30);
}

/* The TBSCertificate's fields come in a fixed order after an optional
 * [0] version, which both networks' certificates carry. */
uint8_t der_tbs_field(uint8_t want, der *f)
{
  der c, t;
  uint16_t off, end;
  uint8_t i;

  if (!der_tbs(&c, &t)) return 0;
  off = t.off; end = der_after(&t);
  if (!der_elem(off, end, f)) return 0;
  if (f->tag == 0xa0) off = der_after(f);          /* step over [0] version */
  for (i = 0; ; i++) {
    if (off >= end || !der_elem(off, end, f)) return 0;
    if (i == want) return 1;
    off = der_after(f);
  }
}

/* Both halves of the chain check begin here, in their own images. */
uint8_t chain_split(x509_read read, void *ctx, uint16_t total, uint16_t *off, uint16_t *len, uint8_t max)
{
  der d;
  uint16_t at = 0;
  uint8_t n = 0;

  chain_select(read, ctx, total);
  while (n < max && at < total) {
    if (!der_elem(at, total, &d) || d.tag != 0x30) break;
    off[n] = at;
    len[n] = (uint16_t)(der_after(&d) - at);
    n++;
    at = der_after(&d);
  }
  return n;
}

uint8_t der_oid_is(const der *o, const uint8_t *want, uint8_t n, uint8_t len)
{
  uint8_t buf[9], i;
  if (o->tag != 0x06 || o->len != len) return 0;
  der_read(o->off, buf, len);
  for (i = 0; i < n; i++) if (buf[i] != want[i]) return 0;
  return 1;
}
