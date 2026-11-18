#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""
Laura Encoding (Codificado de Laura): reference implementation of the SERVER side.

This file is NOT installed on the node. On the Raspberry Pi, clorela-acqd only
relays challenges to the node and returns signed answers; the calibrated matrix
M, the challenge generator and the verification belong on the WALCORG server.
It is here so the protocol can be exercised end to end on a PC against the
simulated node, and as a reference for the server implementation.

Usage (PC, simulated node):
    ./build/src/acqd/clorela-acqd --config data/dev.conf &
    python3 tools/laura/laura_server_reference.py --socket run-dev/acqd.sock

Specification: docs/laura-encoding.md
"""
import argparse
import hashlib
import json
import os
import socket
import struct
import sys

import numpy as np

CHALLENGE_DOMAIN = b"WALCORG-LAURA-CHALLENGE-v1"
CALIBRATION_DOMAIN = b"WALCORG-LAURA-CALIBRATION-v1"
MAX_LEVEL = 4095
SATURATION = 1000          # predicted readings above this are not used (ADC clips at 1023)


# --------------------------------------------------------------------------- transport
class AcqdSocket:
    """One JSON request per line over the clorela-acqd control socket."""

    def __init__(self, path):
        self.path = path

    def call(self, request):
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
            s.settimeout(60)
            s.connect(self.path)
            s.sendall(json.dumps(request).encode() + b"\n")
            buf = b""
            while not buf.endswith(b"\n"):
                chunk = s.recv(65536)
                if not chunk:
                    break
                buf += chunk
        reply = json.loads(buf)
        if not reply.get("ok"):
            raise RuntimeError(reply.get("error", "unknown error"))
        return reply["laura"]


# --------------------------------------------------------------------------- canonical bytes
def challenge_bytes(rec):
    cid = bytes.fromhex(rec["challenge_id"])
    return (CHALLENGE_DOMAIN + struct.pack("<BB", rec["node"], len(cid)) + cid
            + struct.pack("<12H", *rec["levels"]) + struct.pack("<16H", *rec["readings"])
            + struct.pack("<BBq", rec["flags"], rec["firmware"], int(rec["ts_ms"])))


def calibration_bytes(rec):
    leds = [v for page in rec["leds"] for v in page]
    return (CALIBRATION_DOMAIN + struct.pack("<B", rec["node"]) + struct.pack("<16H", *rec["dark"])
            + struct.pack(f"<{len(leds)}H", *leds)
            + struct.pack("<BBq", rec["flags"], rec["firmware"], int(rec["ts_ms"])))


def check_digest(rec, canonical):
    ok = hashlib.sha256(canonical).hexdigest() == rec["digest"]
    # With the TROPIC01, also verify rec["signature"] (P-256, r||s) over rec["digest"]
    # against the shield's registered public key. Today the signer is "none".
    return ok


# --------------------------------------------------------------------------- the model
class Fingerprint:
    """M (16x12, counts per full-scale level) and dark baseline b, from a calibration."""

    def __init__(self, rec):
        self.dark = np.array(rec["dark"], float)
        self.M = (np.array(rec["leds"], float) - self.dark).T    # (16 sensors, 12 LEDs)
        self.ts_ms = rec["ts_ms"]

    def predict(self, levels):
        return self.dark + self.M @ (np.asarray(levels, float) / MAX_LEVEL)


def verify(fp, levels, readings, sigma0=4.0, rel_tol=0.03, tau=3.0):
    """Normalized RMS residual d; accept if d < tau. Saturated channels are skipped."""
    pred = fp.predict(levels)
    use = pred < SATURATION
    sigma = np.sqrt(sigma0 ** 2 + (rel_tol * pred) ** 2)        # noise + drift since calibration
    z = (np.asarray(readings, float) - pred)[use] / sigma[use]
    d = float(np.sqrt(np.mean(z ** 2))) if use.any() else float("inf")
    return d < tau, d, int(use.sum())


def random_levels(rng, fp=None, tries=50):
    """A fresh pattern, kept in the linear range of every sensor when M is known."""
    for _ in range(tries):
        levels = rng.integers(0, MAX_LEVEL + 1, size=12)
        levels = (levels * rng.uniform(0.15, 0.45)).astype(int)  # partial intensities
        if fp is None or fp.predict(levels).max() < SATURATION:
            return [int(v) for v in levels]
    return [int(v) for v in levels]


def culture_signature(history, m0, min_counts=50):
    """Share of the variance of log(M(t)/M(0)) explained by one component.

    Close to 1: one latent quantity (the biomass) drives all the couplings, as
    Beer-Lambert predicts for a growing culture. Only couplings above
    min_counts are used: the logarithm of a near-zero coupling is mostly noise.
    The culture must grow noticeably between calibrations (hours on a real node)."""
    use = (m0 > min_counts).ravel()
    D = np.array([(np.log(np.clip(m, 1, None)) - np.log(m0.clip(1, None))).ravel()[use] for m in history])
    s = np.linalg.svd(D - D.mean(axis=0), compute_uv=False)
    return float(s[0] ** 2 / np.sum(s ** 2))


# --------------------------------------------------------------------------- demo
def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--socket", default="/run/clorela/acqd.sock")
    ap.add_argument("--node", type=lambda x: int(x, 0), default=0x10)
    ap.add_argument("--challenges", type=int, default=10)
    ap.add_argument("--history", type=int, default=6, help="calibrations for the culture signature")
    ap.add_argument("--spacing", type=int, default=150,
                    help="extra measurements between those calibrations (the simulated culture grows a little with each)")
    ap.add_argument("--seed", type=int, default=None)
    args = ap.parse_args()

    acqd = AcqdSocket(args.socket)
    rng = np.random.default_rng(args.seed)
    used_ids = set()
    failures = 0

    print(f"Calibrating node 0x{args.node:02x} ...")
    cal = acqd.call({"cmd": "laura_calibrate", "node": args.node})
    assert check_digest(cal, calibration_bytes(cal)), "calibration digest mismatch"
    fp = Fingerprint(cal)
    print(f"  M is 16x12, |M| range {fp.M.min():.0f}..{fp.M.max():.0f} counts, "
          f"signer: {cal['signer']} ({cal['signer_status']})")

    print(f"\n{args.challenges} fresh challenges:")
    last = None
    for k in range(args.challenges):
        cid = os.urandom(16).hex()
        used_ids.add(cid)
        levels = random_levels(rng, fp)
        rec = acqd.call({"cmd": "laura_challenge", "node": args.node, "challenge_id": cid, "levels": levels})
        digest_ok = check_digest(rec, challenge_bytes(rec))
        echo_ok = rec["challenge_id"] == cid and rec["levels"] == levels
        ok, d, n = verify(fp, levels, rec["readings"])
        ok = ok and digest_ok and echo_ok
        failures += not ok
        print(f"  #{k + 1:2d}  d = {d:5.2f}  ({n:2d} sensors)  digest {'ok' if digest_ok else 'BAD'}  "
              f"-> {'ACCEPT' if ok else 'REJECT'}")
        last = (levels, rec)

    print("\nAttacks:")
    # 1. Replay: an old answer offered for a new challenge.
    new_levels = random_levels(rng, fp)
    ok, d, _ = verify(fp, new_levels, last[1]["readings"])
    print(f"  replayed answer for a new pattern:      d = {d:6.2f} -> {'ACCEPT (!)' if ok else 'REJECT'}")
    failures += ok
    # 2. Reused challenge id.
    print(f"  reused challenge id:                    -> {'REJECT' if last[1]['challenge_id'] in used_ids else 'ACCEPT (!)'}")
    # 3. Readings from a different culture (here: M shuffled between sensors).
    fake = Fingerprint(cal)
    fake.M = fake.M[rng.permutation(16)]
    lv = random_levels(rng, fp)
    ok, d, _ = verify(fp, lv, fake.predict(lv))
    print(f"  answer from another culture/geometry:   d = {d:6.2f} -> {'ACCEPT (!)' if ok else 'REJECT'}")
    failures += ok
    # 4. Tampered reading: digest no longer matches.
    tampered = dict(last[1], readings=list(last[1]["readings"]))
    tampered["readings"][3] += 1
    print(f"  one reading changed after signing:      -> {'REJECT' if not check_digest(tampered, challenge_bytes(tampered)) else 'ACCEPT (!)'}")

    if args.history > 1:
        print(f"\nCulture signature over {args.history} calibrations:")
        hist = [fp.M]
        for _ in range(args.history - 1):
            for _ in range(args.spacing):          # let the (simulated) culture grow
                acqd.call({"cmd": "laura_challenge", "node": args.node,
                           "challenge_id": os.urandom(4).hex(), "levels": [0] * 12})
            rec = acqd.call({"cmd": "laura_calibrate", "node": args.node})
            hist.append(Fingerprint(rec).M)
        sig = culture_signature(hist[1:], hist[0])
        noise_hist = [hist[0] * rng.uniform(0.95, 1.05, size=hist[0].shape) for _ in hist[1:]]
        sig_fake = culture_signature(noise_hist, hist[0])
        print(f"  real node:              one component explains {100 * sig:5.1f} % of the change")
        print(f"  independent fake noise: one component explains {100 * sig_fake:5.1f} % of the change")

    print(f"\n{'All checks behaved as expected.' if failures == 0 else f'{failures} unexpected result(s).'}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
