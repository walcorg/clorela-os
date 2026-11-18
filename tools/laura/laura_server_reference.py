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
import json
import os
import socket
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from laura_model import (ChallengeRegistry, Fingerprint, calibration_bytes, challenge_bytes,  # noqa: E402
                         check_digest, culture_signature, growth_check, random_levels, verify)

PWM_SAMPLES = 32           # node-routine 0x02 averages 32 ADC samples per sensor in 'X'


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
    registry = ChallengeRegistry(ttl_s=60)
    failures = 0

    print(f"Calibrating node 0x{args.node:02x} ...")
    cal = acqd.call({"cmd": "laura_calibrate", "node": args.node})
    assert check_digest(cal, calibration_bytes(cal)), "calibration digest mismatch"
    fp = Fingerprint.from_record(cal)
    print(f"  M is 16x12, |M| range {fp.M.min():.0f}..{fp.M.max():.0f} counts, "
          f"signer: {cal['signer']} ({cal['signer_status']})")

    print(f"\n{args.challenges} fresh challenges:")
    last = None
    for k in range(args.challenges):
        levels = random_levels(rng, fp)
        cid = registry.issue(args.node, levels)
        rec = acqd.call({"cmd": "laura_challenge", "node": args.node, "challenge_id": cid, "levels": levels})
        digest_ok = check_digest(rec, challenge_bytes(rec))
        fresh_ok, _ = registry.redeem(rec["challenge_id"], rec["node"], rec["levels"])
        ok, d, n = verify(fp, levels, rec["readings"], pwm_samples=PWM_SAMPLES)
        ok = ok and digest_ok and fresh_ok
        failures += not ok
        print(f"  #{k + 1:2d}  d = {d:5.2f}  ({n:2d} sensors)  digest {'ok' if digest_ok else 'BAD'}  "
              f"-> {'ACCEPT' if ok else 'REJECT'}")
        last = (levels, rec)

    print("\nAttacks:")
    # 1. Replay: an old answer offered for a new challenge.
    new_levels = random_levels(rng, fp)
    ok, d, _ = verify(fp, new_levels, last[1]["readings"], pwm_samples=PWM_SAMPLES)
    print(f"  replayed answer for a new pattern:      d = {d:6.2f} -> {'ACCEPT (!)' if ok else 'REJECT'}")
    failures += ok
    # 2. Reused challenge id.
    ok, why = registry.redeem(last[1]["challenge_id"], last[1]["node"], last[1]["levels"])
    print(f"  reused challenge id:                    -> {'ACCEPT (!)' if ok else 'REJECT'} ({why})")
    failures += ok
    # 3. Readings from a different culture (here: M shuffled between sensors).
    fake = Fingerprint.from_record(cal)
    fake.M = fake.M[rng.permutation(16)]
    lv = random_levels(rng, fp)
    ok, d, _ = verify(fp, lv, fake.predict(lv), pwm_samples=PWM_SAMPLES)
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
            hist.append(Fingerprint.from_record(rec).M)
        sig = culture_signature(hist[1:], hist[0])
        noise_hist = [hist[0] * rng.uniform(0.95, 1.05, size=hist[0].shape) for _ in hist[1:]]
        sig_fake = culture_signature(noise_hist, hist[0])
        print(f"  real node:              one component explains {100 * sig:5.1f} % of the change")
        print(f"  independent fake noise: one component explains {100 * sig_fake:5.1f} % of the change")
        ok, det = growth_check(hist)
        print(f"  growth_check: {'PASS' if ok else 'FAIL'} (OD change {det['od_change']:+.3f}, "
              f"monotonic {det['monotonic']})")

    print(f"\n{'All checks behaved as expected.' if failures == 0 else f'{failures} unexpected result(s).'}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
