# SPDX-License-Identifier: MIT
"""
Laura Encoding (Codificado de Laura): server-side model and checks.

Shared by laura_server_reference.py (end-to-end demo against clorela-acqd) and
by the hardening tests in tests/. Nothing here runs on the node or on the Pi.
Specification: docs/laura-encoding.md
"""
import hashlib
import os
import struct
import time

import numpy as np

CHALLENGE_DOMAIN = b"WALCORG-LAURA-CHALLENGE-v1"
CALIBRATION_DOMAIN = b"WALCORG-LAURA-CALIBRATION-v1"
MAX_LEVEL = 4095
N_LEDS = 12
N_SENSORS = 16
SATURATION = 1000          # predicted readings above this are not used (ADC clips at 1023)


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


def digest_of(canonical):
    return hashlib.sha256(canonical).hexdigest()


def check_digest(rec, canonical):
    # With the TROPIC01, also verify rec["signature"] (P-256, r||s) over rec["digest"]
    # against the shield's registered public key. Today the signer is "none".
    return digest_of(canonical) == rec["digest"]


# --------------------------------------------------------------------------- the fingerprint
class Fingerprint:
    """M (16x12, counts per full-scale level) and dark baseline b, from a calibration."""

    def __init__(self, dark, leds, ts_ms=0):
        self.dark = np.asarray(dark, float)
        self.M = (np.asarray(leds, float) - self.dark).T          # (16 sensors, 12 LEDs)
        self.ts_ms = ts_ms

    @classmethod
    def from_record(cls, rec):
        return cls(rec["dark"], rec["leds"], rec.get("ts_ms", 0))

    def predict(self, levels):
        return self.dark + self.M @ (np.asarray(levels, float) / MAX_LEVEL)


PWM_COEF = 0.3             # fitted on the simulated node (tools/laura/simculture.py), not measured


def pwm_sigma(fp, levels, samples):
    """Expected error from sampling the TLC59401 PWM with `samples` ADC readings.

    Each LED at an intermediate level is caught on in a whole number of the
    samples, so its contribution is quantized to about 1/samples of its
    coupling. Full-off and full-on LEDs add nothing."""
    x = np.asarray(levels)
    partial = (x > 0) & (x < MAX_LEVEL)
    return PWM_COEF * np.sqrt((np.clip(fp.M[:, partial], 0, None) ** 2).sum(axis=1)) / samples


def verify(fp, levels, readings, sigma0=4.0, rel_tol=0.03, tau=3.0, saturation=SATURATION,
           pwm_samples=None, min_sensors=8):
    """Normalized RMS residual d; accept if d < tau. Channels predicted above
    `saturation` are skipped (pass saturation=None to use every channel).
    With pwm_samples (32 for node-routine 0x02) the PWM quantization error is
    added to sigma; without it, rel_tol has to absorb it. Fewer than
    `min_sensors` usable channels is a rejection: a pattern that clips most
    sensors proves little."""
    pred = fp.predict(levels)
    use = pred < saturation if saturation is not None else np.ones(N_SENSORS, bool)
    var = sigma0 ** 2 + (rel_tol * pred) ** 2                    # noise + drift since calibration
    if pwm_samples:
        var = var + pwm_sigma(fp, levels, pwm_samples) ** 2
    sigma = np.sqrt(var)
    z = (np.asarray(readings, float) - pred)[use] / sigma[use]
    d = float(np.sqrt(np.mean(z ** 2))) if use.any() else float("inf")
    return bool(d < tau and use.sum() >= min_sensors), d, int(use.sum())


def random_levels(rng, fp=None, tries=50):
    """A fresh pattern, kept in the linear range of every sensor when M is known."""
    levels = None
    for _ in range(tries):
        levels = rng.integers(0, MAX_LEVEL + 1, size=N_LEDS)
        levels = (levels * rng.uniform(0.15, 0.45)).astype(int)  # partial intensities
        if fp is None or fp.predict(levels).max() < SATURATION:
            break
    return [int(v) for v in levels]


# --------------------------------------------------------------------------- evolution over time
def culture_signature(history, m0, min_counts=50):
    """Share of the variance of log(M(t)/M(0)) explained by one component.

    Close to 1: one latent quantity (the biomass) drives all the couplings, as
    Beer-Lambert predicts for a growing culture. Only couplings above
    min_counts are used: the logarithm of a near-zero coupling is mostly noise.
    The culture must grow noticeably between calibrations (hours on a real node)."""
    use = (np.asarray(m0) > min_counts).ravel()
    D = np.array([(np.log(np.clip(m, 1, None)) - np.log(np.clip(m0, 1, None))).ravel()[use]
                  for m in history])
    s = np.linalg.svd(D - D.mean(axis=0), compute_uv=False)
    return float(s[0] ** 2 / np.sum(s ** 2)) if np.sum(s ** 2) > 0 else 0.0


def growth_check(history, min_signature=0.8, min_change=0.02, min_counts=50):
    """Checks that a node's calibrations behave like ONE growing culture.

    history: list of M matrices (16x12) in time order.
    - signature: one latent component explains most of the change (Beer-Lambert);
    - change: the transmission couplings actually move (a fixed mask, a dead
      culture or a replayed calibration does not);
    - monotonic: the latent optical density does not go back and forth.
    Returns (ok, details)."""
    m0 = np.asarray(history[0])
    trans = m0[8:16]                                   # transmission sensors T0-T7
    use = trans > min_counts
    od = np.array([float(np.mean(-np.log(np.clip(np.asarray(m)[8:16][use], 1, None) / trans[use])))
                   for m in history])                  # relative OD, 0 at the first calibration
    change = float(od[-1] - od[0])
    steps = np.diff(od)
    # allow small negative steps (noise), not a reversal of the trend
    monotonic = bool(np.all(steps > -max(0.25 * abs(change), 0.005)))
    signature = culture_signature(history[1:], m0, min_counts) if len(history) > 2 else 1.0
    ok = signature >= min_signature and change >= min_change and monotonic
    return ok, {"signature": signature, "od_change": change, "monotonic": monotonic, "od": od.tolist()}


# --------------------------------------------------------------------------- challenge registry
class ChallengeRegistry:
    """Server-side bookkeeping of optical challenges.

    - ids are random (16 bytes) and bound to one node and one pattern;
    - each id is accepted once (replay of the same answer is refused);
    - an answer must arrive within `ttl_s`;
    - at most `max_per_hour` challenges per node, to limit how many
      (pattern, answer) pairs an eavesdropper collects per calibration period.
    """

    def __init__(self, ttl_s=30.0, max_per_hour=60, clock=time.monotonic):
        self.ttl_s = ttl_s
        self.max_per_hour = max_per_hour
        self.clock = clock
        self._open = {}          # id -> (node, levels, issued_at)
        self._used = set()
        self._issued = {}        # node -> [issued_at, ...]

    def issue(self, node, levels):
        now = self.clock()
        recent = [t for t in self._issued.get(node, []) if now - t < 3600]
        if len(recent) >= self.max_per_hour:
            raise RuntimeError(f"rate limit: node 0x{node:02x} already has {len(recent)} challenges this hour")
        cid = os.urandom(16).hex()
        self._open[cid] = (node, list(levels), now)
        self._issued[node] = recent + [now]
        return cid

    def redeem(self, cid, node, levels):
        """Returns (ok, reason). An id can be redeemed once, by its node, with its pattern."""
        if cid in self._used:
            return False, "challenge id already used"
        entry = self._open.pop(cid, None)
        if entry is None:
            return False, "unknown challenge id"
        self._used.add(cid)
        exp_node, exp_levels, issued = entry
        if node != exp_node:
            return False, "answer from another node"
        if list(levels) != exp_levels:
            return False, "pattern does not match the challenge"
        if self.clock() - issued > self.ttl_s:
            return False, "answer arrived too late"
        return True, "ok"
