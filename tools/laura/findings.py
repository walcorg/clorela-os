#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""
Laura Encoding hardening findings, measured on the physical model in simculture.py.

Each function measures one finding and returns numbers; tests/test_findings.py
asserts on them and this script prints them:

    python3 tools/laura/findings.py

The numbers come from a model with plausible constants, not from a real
culture. They show the size and direction of each effect, and they fix the
behaviour so a change in the verifier that reopens a hole fails a test.
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from laura_model import (N_LEDS, Fingerprint, challenge_bytes, check_digest,  # noqa: E402
                         digest_of, growth_check, random_levels, verify)
from simculture import LeastSquaresForger, SimCulture, synthetic_growth_history  # noqa: E402

SEEDS = (1, 2, 3)


def _rate(node, fp, n=200, seed=1, read_kw=None, **verify_kw):
    rng = np.random.default_rng(seed)
    acc, ds = [], []
    for _ in range(n):
        lv = random_levels(rng, fp)
        ok, d, _ = verify(fp, lv, node.read(lv, **(read_kw or {})), **verify_kw)
        acc.append(ok)
        ds.append(d)
    return float(np.mean(acc)), float(np.median(ds))


def _residual_std(node, fp, n=200, seed=1):
    rng = np.random.default_rng(seed)
    r = [np.array(node.read(lv)) - fp.predict(lv) for lv in (random_levels(rng, fp) for _ in range(n))]
    return float(np.std(r))


# --------------------------------------------------------------------------- F1, F2
def honest_acceptance(**verify_kw):
    """F1: answers from the real node, verified with the reference parameters."""
    return float(np.mean([_rate(SimCulture(s), SimCulture(s).fingerprint(), **verify_kw)[0]
                          for s in SEEDS]))


def attack_acceptance(n=200, **verify_kw):
    """F2: replay of an old answer, answer from another node, both for a fresh pattern."""
    replay, other = [], []
    for s in SEEDS:
        node, foreign = SimCulture(s), SimCulture(s + 100)
        fp = node.fingerprint()
        rng = np.random.default_rng(s)
        for _ in range(n):
            old = node.read(random_levels(rng, fp))
            replay.append(verify(fp, random_levels(rng, fp), old, **verify_kw)[0])
            lv = random_levels(rng, fp)
            other.append(verify(fp, lv, foreign.read(lv), **verify_kw)[0])
    return {"replay": float(np.mean(replay)), "other_node": float(np.mean(other))}


def tamper_detected():
    """F2: one count changed after the digest was taken; and domain separation."""
    rec = {"node": 0x10, "challenge_id": "00112233", "levels": [100] * 12, "readings": [500] * 16,
           "flags": 1, "firmware": 2, "ts_ms": 1795000000000}
    rec["digest"] = digest_of(challenge_bytes(rec))
    tampered = dict(rec, readings=list(rec["readings"]))
    tampered["readings"][5] += 1
    return check_digest(rec, challenge_bytes(rec)) and not check_digest(tampered, challenge_bytes(tampered))


# --------------------------------------------------------------------------- F3, F4: known weaknesses
def forger_acceptance(observations, seed=1, trials=100):
    """F3 (known weakness): a compromised Pi that has relayed `observations`
    challenges fits b and M by least squares and answers new ones by itself."""
    node = SimCulture(seed)
    fp = node.fingerprint()
    rng = np.random.default_rng(seed + 7)
    forger = LeastSquaresForger()
    for _ in range(observations):
        lv = random_levels(rng, fp)
        forger.observe(lv, node.read(lv))
    tests = [random_levels(rng, fp) for _ in range(trials)]
    return float(np.mean([verify(fp, lv, forger.answer(lv))[0] for lv in tests]))


def calibration_leak_acceptance(seed=1):
    """F4 (known weakness): one relayed calibration is the fingerprint itself."""
    node = SimCulture(seed)
    leaked = node.calibrate()                       # the Pi relays it, so it can keep a copy
    server_fp = node.fingerprint()                  # the server's own copy (a second 'C')
    forged_fp = Fingerprint(*leaked)
    rng = np.random.default_rng(seed)
    tests = [random_levels(rng, server_fp) for _ in range(100)]
    return float(np.mean([verify(server_fp, lv, [int(round(v)) for v in forged_fp.predict(lv)])[0]
                          for lv in tests]))


# --------------------------------------------------------------------------- F5: drift
def drift_acceptance(dod, seed=1):
    """F5: acceptance when the culture grew by `dod` since the last calibration,
    and after recalibrating."""
    node = SimCulture(seed)
    fp = node.fingerprint()
    node.grow(dod)
    stale = _rate(node, fp)[0]
    fresh = _rate(node, node.fingerprint())[0]
    return stale, fresh


# --------------------------------------------------------------------------- F6: PWM
def pwm_residuals(seed=1):
    """F6: RMS residual (counts) of honest answers for several sampling schemes."""
    out = {}
    for name, kw in {"1 sample": dict(samples=1), "32 x 112 us (fw 0x02)": dict(samples=32),
                     "64 x 112 us": dict(samples=64), "256 x 118 us": dict(samples=256, sample_us=118.0)}.items():
        node = SimCulture(seed, **kw)
        out[name] = _residual_std(node, node.fingerprint())
    node = SimCulture(seed)
    fp = node.fingerprint()
    rng = np.random.default_rng(seed)
    out["no PWM (ideal)"] = float(np.std([np.array(node.read(lv, pwm=False)) - fp.predict(lv)
                                          for lv in (random_levels(rng, fp) for _ in range(200))]))
    return out


# --------------------------------------------------------------------------- F7: photoresistors
def sensor_acceptance(sensor):
    """F7: the linear verifier on linear phototransistors vs CdS photoresistors."""
    return float(np.mean([_rate(SimCulture(s, sensor=sensor), SimCulture(s, sensor=sensor).fingerprint())[0]
                          for s in SEEDS]))


# --------------------------------------------------------------------------- F8: saturation
def saturation_case(seed=1, level=3000):
    """F8: a bright pattern clips some sensors at 1023. Returns
    (accepted with the guard, accepted without it, sensors used by the guard)."""
    node = SimCulture(seed)
    fp = node.fingerprint()
    lv = [level] * N_LEDS
    y = node.read(lv)
    with_guard = verify(fp, lv, y)
    without = verify(fp, lv, y, saturation=None)
    return with_guard[0], without[0], with_guard[2], int(sum(v >= 1023 for v in y))


# --------------------------------------------------------------------------- F9, F10: evolution
def growth_histories(seed=1, steps=6, dod=0.05):
    """F9, F10: growth_check on a real growing culture and on four fakes."""
    node = SimCulture(seed)
    real = []
    for _ in range(steps):
        real.append(node.fingerprint().M)
        node.grow(dod)
    frozen_node = SimCulture(seed)
    frozen = [frozen_node.fingerprint().M for _ in range(steps)]        # optics that never change
    other = SimCulture(seed + 100)
    spliced = real[:3] + [other.fingerprint().M] + real[3:steps - 1]  # one calibration from elsewhere
    rng = np.random.default_rng(seed)
    noise = [real[0]] + [real[0] * rng.uniform(0.9, 1.1, real[0].shape) for _ in range(steps - 1)]
    synthetic = synthetic_growth_history(real[0], steps, dod, seed=seed)
    return {name: growth_check(h) for name, h in
            {"real culture": real, "frozen optics / replayed calibration": frozen,
             "one calibration from another node": spliced, "independent noise": noise,
             "synthetic Beer-Lambert (forger)": synthetic}.items()}


# --------------------------------------------------------------------------- report
def main():
    pct = lambda v: f"{100 * v:5.1f} %"  # noqa: E731
    print("Laura Encoding hardening findings (simulated node, simculture.py)\n")
    print(f"F1  honest answers accepted (reference sigma):         {pct(honest_acceptance())}")
    print(f"    ... with the PWM term (pwm_samples=32):           {pct(honest_acceptance(pwm_samples=32))}")
    a, b = attack_acceptance(), attack_acceptance(pwm_samples=32)
    print(f"F2  replayed answer accepted:                          {pct(a['replay'])}   (PWM term: {pct(b['replay'])})")
    print(f"    answer from another node accepted:                 {pct(a['other_node'])}   (PWM term: {pct(b['other_node'])})")
    print(f"    tampered reading detected by the digest:           {tamper_detected()}")
    print("F3  least-squares forger on a compromised Pi (KNOWN WEAKNESS):")
    for k in (6, 12, 13, 20, 40, 80):
        print(f"      after {k:3d} relayed challenges:                  {pct(forger_acceptance(k))}")
    print(f"F4  answers computed from one leaked calibration:      {pct(calibration_leak_acceptance())}   (KNOWN WEAKNESS)")
    print("F5  drift since the last calibration (stale M / recalibrated):")
    for dod in (0.02, 0.05, 0.1, 0.3):
        s, f = drift_acceptance(dod)
        print(f"      OD +{dod:4.2f}:                                     {pct(s)} / {pct(f)}")
    print("F6  RMS residual of honest answers, counts:")
    for k, v in pwm_residuals().items():
        print(f"      {k:24s}                         {v:6.1f}")
    print(f"F7  linear verifier, TEMT6000 (linear):                {pct(sensor_acceptance('linear'))}")
    print(f"    linear verifier, CdS photoresistor:                {pct(sensor_acceptance('ldr'))}")
    g, ng, used, clipped = saturation_case()
    print(f"F8  bright pattern, {clipped} sensors clipped at 1023: accepted with guard {g} ({used} sensors), without {ng}")
    print("F9  growth_check (F10: the synthetic history is a KNOWN WEAKNESS) over 6 calibrations:")
    for name, (ok, det) in growth_histories().items():
        print(f"      {name:38s} {'PASS' if ok else 'FAIL'}  signature {pct(det['signature'])}  "
              f"OD change {det['od_change']:+.3f}  monotonic {det['monotonic']}")


if __name__ == "__main__":
    main()
