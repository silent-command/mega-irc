#!/usr/bin/env python3
"""The P-384 constants in src/crypto/p384.c, derived and checked rather
than copied from anywhere: p, n, b and the generator G of FIPS 186-4
D.1.2.4. Before printing, this asserts that G lies on the curve
y^2 = x^3 - 3x + b (mod p) and that n G is the point at infinity, which
together pin every one of the five numbers -- a typo in any of them
fails one of the two.

    python3 tools/gen_p384.py          check, and print what p384.c holds

Pure Python: no packages, so the check can be re-run anywhere.
"""

P = 2**384 - 2**128 - 2**96 + 2**32 - 1
B = int("b3312fa7e23ee7e4988e056be3f82d19181d9c6efe8141120314088f5013875a"
        "c656398d8a2ed19d2a85c8edd3ec2aef", 16)
N = int("ffffffffffffffffffffffffffffffffffffffffffffffffc7634d81f4372ddf"
        "581a0db248b0a77aecec196accc52973", 16)
GX = int("aa87ca22be8b05378eb1c71ef320ad746e1d3b628ba79b9859f741e082542a38"
         "5502f25dbf55296c3a545e3872760ab7", 16)
GY = int("3617de4a96262c6f5d9e98bf9292dc29f8f41dbd289a147ce9da3113b5f0b8c0"
         "0a60b1ce1d7e819d7a431d7c90ea0e5f", 16)


def add(p, q):
    """Affine addition on y^2 = x^3 - 3x + b; None is the point at infinity."""
    if p is None:
        return q
    if q is None:
        return p
    if p[0] == q[0] and (p[1] + q[1]) % P == 0:
        return None
    if p == q:
        lam = ((3 * p[0] * p[0] - 3) * pow(2 * p[1], -1, P)) % P
    else:
        lam = ((q[1] - p[1]) * pow(q[0] - p[0], -1, P)) % P
    x = (lam * lam - p[0] - q[0]) % P
    return (x, (lam * (p[0] - x) - p[1]) % P)


def mul(k, p):
    r = None
    for bit in bin(k)[2:]:
        r = add(r, r)
        if bit == "1":
            r = add(r, p)
    return r


def rows(name, v):
    data = v.to_bytes(48, "big")
    out = [f"static const uint8_t {name}[48] = {{"]
    for i in range(0, 48, 12):
        out.append("  " + ", ".join(f"0x{b:02x}" for b in data[i:i + 12]) + ("," if i < 36 else ""))
    out.append("};")
    return "\n".join(out)


def main():
    assert P.bit_length() == 384 and (P >> 383) == 1, "p is not 384 bits with its top bit set"
    assert (GY * GY - (GX * GX * GX - 3 * GX + B)) % P == 0, "G is not on the curve"
    assert mul(N, (GX, GY)) is None, "n G is not the point at infinity"
    print("/* checked by tools/gen_p384.py: G is on the curve and n G is infinity */")
    for name, v in (("P", P), ("N", N), ("B", B), ("GX", GX), ("GY", GY)):
        print(rows(name, v))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
