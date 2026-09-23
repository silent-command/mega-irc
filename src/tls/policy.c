/* The half of the chain check that is policy rather than arithmetic: is
 * the leaf for the host we dialled, and is every certificate in date.
 * No key and no signature is touched here, so on the MEGA65 this runs
 * in the client, where the clock is, and the TLS bank keeps only the
 * signatures (chain.c; REQUIREMENTS.md 5.15). */
#include "chain.h"
#include "der.h"

/* 2.5.29.17, subjectAltName. */
static const uint8_t oid_san[3] = { 0x55, 0x1d, 0x11 };

/* A UTCTime or GeneralizedTime reduced to twelve digits, YYMMDDHHMMSS,
 * so two of them compare directly. UTCTime's year is two digits already;
 * GeneralizedTime, which a certificate reaching past 2049 would use,
 * gives four and the century is dropped to line up. */
static uint8_t time12(const der *d, char out[13])
{
  uint8_t buf[20], i, skip;
  if (d->len > sizeof buf) return 0;
  der_read(d->off, buf, d->len);
  skip = (uint8_t)(d->tag == 0x18 ? 2 : 0);
  if (d->len < (uint16_t)(skip + 12)) return 0;
  for (i = 0; i < 12; i++) out[i] = (char)buf[skip + i];
  out[12] = 0;
  return 1;
}

/* Twelve digits against twelve: negative, zero or positive. */
static int8_t cmp12(const char *a, const char *b)
{
  uint8_t i;
  for (i = 0; i < 12; i++) if (a[i] != b[i]) return (int8_t)(a[i] < b[i] ? -1 : 1);
  return 0;
}

uint8_t chain_valid_at(const char *now)
{
  der v, t;
  char nb[13], na[13];
  uint16_t end;

  if (!der_tbs_field(TBS_VALIDITY, &v) || v.tag != 0x30) return 0;
  end = der_after(&v);
  if (!der_elem(v.off, end, &t) || !time12(&t, nb)) return 0;
  if (!der_elem(der_after(&t), end, &t) || !time12(&t, na)) return 0;
  return (uint8_t)(cmp12(now, nb) >= 0 && cmp12(now, na) <= 0);
}

/* One dNSName against the host, case-insensitively, with "*." matching a
 * single label. */
static uint8_t dns_matches(const uint8_t *dns, uint16_t n, const char *host)
{
  uint16_t i = 0;
  const char *h = host;
  uint8_t a, b;

  if (n >= 2 && dns[0] == '*' && dns[1] == '.') {
    while (*h && *h != '.') h++;                   /* the wildcard eats one label */
    if (*h != '.') return 0;
    i = 1;                                          /* line both up on the dot */
  }
  for (; i < n; i++, h++) {
    a = dns[i]; b = (uint8_t)*h;
    if (a >= 'A' && a <= 'Z') a = (uint8_t)(a + 32);
    if (b >= 'A' && b <= 'Z') b = (uint8_t)(b + 32);
    if (!b || a != b) return 0;
  }
  return (uint8_t)(*h == 0);
}

#ifdef __mos__
#include "../m65/lowram.h"
#define dnsbuf ((uint8_t *)LOW_LINE)   /* the stream's line buffer, idle during the handshake (5.18) */
#define DNS_CAP 128
#else
static uint8_t dnsbuf[128];
#define DNS_CAP sizeof dnsbuf
#endif

uint8_t chain_host_matches(const char *host)
{
  der e, x, o;
  uint16_t p, pend, xend;

  if (!der_tbs_field(TBS_EXTS, &e) || e.tag != 0xa3) return 0;
  if (!der_elem(e.off, der_after(&e), &x) || x.tag != 0x30) return 0;   /* SEQUENCE OF Extension */
  p = x.off; pend = der_after(&x);
  while (p < pend) {
    if (!der_elem(p, pend, &x) || x.tag != 0x30) return 0;
    xend = der_after(&x);
    if (!der_elem(x.off, xend, &o)) return 0;
    if (!der_oid_is(&o, oid_san, sizeof oid_san, sizeof oid_san)) { p = xend; continue; }

    /* after the OID: an optional critical BOOLEAN, then the OCTET STRING */
    if (!der_elem(der_after(&o), xend, &o)) return 0;
    if (o.tag == 0x01 && !der_elem(der_after(&o), xend, &o)) return 0;
    if (o.tag != 0x04) return 0;

    /* extnValue holds a SEQUENCE OF GeneralName; dNSName is context [2] */
    if (!der_elem(o.off, der_after(&o), &x) || x.tag != 0x30) return 0;
    p = x.off; pend = der_after(&x);
    while (p < pend) {
      if (!der_elem(p, pend, &o)) return 0;
      if (o.tag == 0x82 && o.len > 0 && o.len <= DNS_CAP) {
        der_read(o.off, dnsbuf, o.len);
        if (dns_matches(dnsbuf, o.len, host)) return 1;
      }
      p = der_after(&o);
    }
    return 0;                        /* the SAN was there and none of it matched */
  }
  return 0;                          /* no SAN at all: the CN is not consulted (5.8) */
}

uint8_t chain_policy(const chain_ref *certs, uint8_t n, const char *host, const char *now)
{
  uint8_t i;
  if (n == 0) return CHAIN_MALFORMED;
  chain_select(certs[0].read, certs[0].ctx, certs[0].len);
  if (host && !chain_host_matches(host)) return CHAIN_BAD_NAME;
  if (now)
    for (i = 0; i < n; i++) {
      chain_select(certs[i].read, certs[i].ctx, certs[i].len);
      if (!chain_valid_at(now)) return CHAIN_EXPIRED;
    }
  return CHAIN_OK;
}
