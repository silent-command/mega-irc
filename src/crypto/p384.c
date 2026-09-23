#include "p384.h"
#include "mp.h"

#define L 24                       /* limbs of a P-384 number, 16 bits each */
#define BITS 384

/* The curve, FIPS 186-4 D.1.2.4: p = 2^384 - 2^128 - 2^96 + 2^32 - 1,
 * a = -3, and n the order of G. Generated and checked by
 * tools/gen_p384.py, which asserts that G is on the curve and that
 * n G is the point at infinity before printing these. */
static const uint8_t P[48] = {
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff
};
static const uint8_t N[48] = {
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xc7, 0x63, 0x4d, 0x81, 0xf4, 0x37, 0x2d, 0xdf, 0x58, 0x1a, 0x0d, 0xb2,
  0x48, 0xb0, 0xa7, 0x7a, 0xec, 0xec, 0x19, 0x6a, 0xcc, 0xc5, 0x29, 0x73
};
static const uint8_t B[48] = {
  0xb3, 0x31, 0x2f, 0xa7, 0xe2, 0x3e, 0xe7, 0xe4, 0x98, 0x8e, 0x05, 0x6b,
  0xe3, 0xf8, 0x2d, 0x19, 0x18, 0x1d, 0x9c, 0x6e, 0xfe, 0x81, 0x41, 0x12,
  0x03, 0x14, 0x08, 0x8f, 0x50, 0x13, 0x87, 0x5a, 0xc6, 0x56, 0x39, 0x8d,
  0x8a, 0x2e, 0xd1, 0x9d, 0x2a, 0x85, 0xc8, 0xed, 0xd3, 0xec, 0x2a, 0xef
};
static const uint8_t GX[48] = {
  0xaa, 0x87, 0xca, 0x22, 0xbe, 0x8b, 0x05, 0x37, 0x8e, 0xb1, 0xc7, 0x1e,
  0xf3, 0x20, 0xad, 0x74, 0x6e, 0x1d, 0x3b, 0x62, 0x8b, 0xa7, 0x9b, 0x98,
  0x59, 0xf7, 0x41, 0xe0, 0x82, 0x54, 0x2a, 0x38, 0x55, 0x02, 0xf2, 0x5d,
  0xbf, 0x55, 0x29, 0x6c, 0x3a, 0x54, 0x5e, 0x38, 0x72, 0x76, 0x0a, 0xb7
};
static const uint8_t GY[48] = {
  0x36, 0x17, 0xde, 0x4a, 0x96, 0x26, 0x2c, 0x6f, 0x5d, 0x9e, 0x98, 0xbf,
  0x92, 0x92, 0xdc, 0x29, 0xf8, 0xf4, 0x1d, 0xbd, 0x28, 0x9a, 0x14, 0x7c,
  0xe9, 0xda, 0x31, 0x13, 0xb5, 0xf0, 0xb8, 0xc0, 0x0a, 0x60, 0xb1, 0xce,
  0x1d, 0x7e, 0x81, 0x9d, 0x7a, 0x43, 0x1d, 0x7c, 0x90, 0xea, 0x0e, 0x5f
};

typedef struct { mp_limb x[L], y[L], z[L]; } point;   /* Jacobian, Montgomery form; z = 0 is infinity */

/* The shared context is the order n while u1 and u2 are made, then the
 * field p for the point arithmetic; n's limbs are kept for the end.
 * As p256.c, which this follows step for step. */
#define fp mp_shared
#define fn mp_shared

/* The scratch, this file's own: the four points at 0 (288 limbs), the
 * nine scalars at 288, the temporaries of dbl and add at 504 (add's
 * nine are dead when it calls dbl), the order's limbs at 720; 744 of
 * 744. mp_scratch is 512 limbs and cannot hold this (5.22). */
#define SC_LIMBS (12 * L + 9 * L + 9 * L + L)
static mp_limb sc[SC_LIMBS];
#define TMP (sc + 504)
#define n_limbs (sc + 720)

static void fmul(mp_limb *r, const mp_limb *a, const mp_limb *b) { mp_mont_mul(&fp, r, a, b); }
static void fsqr(mp_limb *r, const mp_limb *a) { mp_mont_mul(&fp, r, a, a); }
static void fadd(mp_limb *r, const mp_limb *a, const mp_limb *b) { mp_add(&fp, r, a, b); }
static void fsub(mp_limb *r, const mp_limb *a, const mp_limb *b) { mp_sub(&fp, r, a, b); }

static uint8_t is_inf(const point *p) { return mp_is_zero(&fp, p->z); }

/* r = 2p (a = -3): S = 4XY^2, M = 3(X - Z^2)(X + Z^2), X' = M^2 - 2S,
 * Y' = M(S - X') - 8Y^4, Z' = 2YZ. */
static void dbl(point *r, const point *p)
{
  mp_limb *s = TMP, *m = TMP + L, *t = TMP + 2 * L, *u = TMP + 3 * L;
  if (is_inf(p)) { *r = *p; return; }
  fsqr(t, p->z);                                   /* Z^2 */
  fsub(u, p->x, t); fadd(t, p->x, t); fmul(m, u, t);
  fadd(t, m, m); fadd(m, m, t);                    /* M = 3(X - Z^2)(X + Z^2) */
  fsqr(t, p->y);                                   /* Y^2 */
  fmul(s, p->x, t); fadd(s, s, s); fadd(s, s, s);  /* S = 4XY^2 */
  fsqr(u, t); fadd(u, u, u); fadd(u, u, u); fadd(u, u, u);   /* 8Y^4 */
  fmul(r->z, p->y, p->z); fadd(r->z, r->z, r->z);  /* Z' = 2YZ */
  fsqr(t, m); fsub(t, t, s); fsub(r->x, t, s);     /* X' = M^2 - 2S */
  fsub(t, s, r->x); fmul(t, m, t); fsub(r->y, t, u);   /* Y' = M(S - X') - 8Y^4 */
}

/* r = p + q, general Jacobian addition. */
static void add(point *r, const point *p, const point *q)
{
  mp_limb *z1z1 = TMP, *z2z2 = TMP + L, *u1 = TMP + 2 * L, *u2 = TMP + 3 * L, *s1 = TMP + 4 * L, *s2 = TMP + 5 * L, *h = TMP + 6 * L, *rr = TMP + 7 * L, *t = TMP + 8 * L;
  if (is_inf(p)) { *r = *q; return; }
  if (is_inf(q)) { *r = *p; return; }
  fsqr(z1z1, p->z); fsqr(z2z2, q->z);
  fmul(u1, p->x, z2z2); fmul(u2, q->x, z1z1);
  fmul(t, z2z2, q->z); fmul(s1, p->y, t);
  fmul(t, z1z1, p->z); fmul(s2, q->y, t);
  fsub(h, u2, u1); fsub(rr, s2, s1);
  if (mp_is_zero(&fp, h)) {
    if (mp_is_zero(&fp, rr)) { dbl(r, p); return; }
    mp_set_small(&fp, r->z, 0); return;            /* p = -q */
  }
  fmul(t, p->z, q->z); fmul(r->z, t, h);           /* Z3 = Z1 Z2 H */
  fsqr(t, h);                                      /* H^2 */
  fmul(u1, u1, t);                                 /* U1 H^2 */
  fmul(t, t, h);                                   /* H^3 */
  fmul(s1, s1, t);                                 /* S1 H^3 */
  fsqr(r->x, rr); fsub(r->x, r->x, t); fsub(r->x, r->x, u1); fsub(r->x, r->x, u1);   /* X3 = R^2 - H^3 - 2 U1 H^2 */
  fsub(t, u1, r->x); fmul(t, rr, t); fsub(r->y, t, s1);   /* Y3 = R(U1 H^2 - X3) - S1 H^3 */
}

static uint8_t bit_of(const mp_limb *k, uint16_t i) { return (uint8_t)((k[i / 16] >> (i % 16)) & 1); }

uint8_t p384_verify(const uint8_t pub[96], const uint8_t hash[48], const uint8_t r[48], const uint8_t s[48])
{
  point *g = (point *)sc, *q = g + 1, *gq = g + 2, *acc = g + 3;   /* 4 x 72 limbs */
  mp_limb *rn = sc + 288, *sn = rn + L, *e = sn + L, *w = e + L, *u1 = w + L, *u2 = u1 + L, *t = u2 + L, *x = t + L, *b = x + L;   /* to limb 504 */
  int16_t i;
  uint8_t i1, i2;

  /* the order first: r and s in [1, n-1], w = s^-1, u1 = e w, u2 = r w */
  mp_init(&fn, N, 48);
  mp_copy(&fn, n_limbs, fn.m);
  mp_from_be(&fn, rn, r, 48); mp_from_be(&fn, sn, s, 48);
  if (mp_is_zero(&fn, rn) || mp_is_zero(&fn, sn)) return 0;
  if (mp_cmp(&fn, rn, fn.m) >= 0 || mp_cmp(&fn, sn, fn.m) >= 0) return 0;
  /* e = the whole digest, which is exactly the order's 384 bits, reduced:
   * one subtraction is enough, n being above 2^383 (as p256.c's is) */
  mp_from_be(&fn, t, hash, 48);
  if (mp_cmp(&fn, t, fn.m) >= 0) mp_sub(&fn, t, t, fn.m);
  mp_to_mont(&fn, e, t);
  mp_to_mont(&fn, t, sn); mp_mont_inv(&fn, w, t);
  mp_mont_mul(&fn, u1, e, w); mp_from_mont(&fn, u1, u1);
  mp_to_mont(&fn, t, rn); mp_mont_mul(&fn, u2, t, w); mp_from_mont(&fn, u2, u2);

  /* then the field: the public key, on the curve (y^2 = x^3 - 3x + b) */
  mp_init(&fp, P, 48);
  mp_from_be(&fp, t, pub, 48); mp_to_mont(&fp, q->x, t);
  mp_from_be(&fp, t, pub + 48, 48); mp_to_mont(&fp, q->y, t);
  if (mp_cmp(&fp, q->x, fp.m) >= 0 || mp_cmp(&fp, q->y, fp.m) >= 0) return 0;
  mp_from_be(&fp, t, B, 48); mp_to_mont(&fp, b, t);
  fsqr(t, q->x); fmul(t, t, q->x);                   /* x^3 */
  fsub(t, t, q->x); fsub(t, t, q->x); fsub(t, t, q->x); fadd(t, t, b);
  fsqr(x, q->y);
  if (mp_cmp(&fp, t, x) != 0) return 0;
  mp_from_mont(&fp, q->z, fp.rr);                   /* Z = 1 */

  /* G, and G + Q for Shamir's trick */
  mp_from_be(&fp, t, GX, 48); mp_to_mont(&fp, g->x, t);
  mp_from_be(&fp, t, GY, 48); mp_to_mont(&fp, g->y, t);
  mp_copy(&fp, g->z, q->z);
  add(gq, g, q);

  /* acc = u1 G + u2 Q */
  mp_set_small(&fp, acc->z, 0);
  for (i = BITS - 1; i >= 0; i--) {
    dbl(acc, acc);
    i1 = bit_of(u1, (uint16_t)i); i2 = bit_of(u2, (uint16_t)i);
    if (i1 && i2) add(acc, acc, gq);
    else if (i1) add(acc, acc, g);
    else if (i2) add(acc, acc, q);
    if (crypto_yield && (i & 7) == 0) crypto_yield();
  }
  if (is_inf(acc)) return 0;

  /* x = X / Z^2 mod p, then mod n, against r */
  mp_mont_inv(&fp, t, acc->z); fsqr(t, t); fmul(x, acc->x, t);
  mp_from_mont(&fp, x, x);
  if (mp_cmp(&fp, x, n_limbs) >= 0) mp_sub(&fp, x, x, n_limbs);   /* x mod n: one subtraction suffices, n > p/2, and no borrow */
  return (uint8_t)(mp_cmp(&fp, x, rn) == 0);
}
