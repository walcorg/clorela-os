# SPDX-License-Identifier: MIT
"""
One test per Laura Encoding finding (docs/laura-encoding.md, section 8).

Tests named test_known_weakness_* PASS WHEN THE ATTACK WORKS. They record a
hole that is still open, so nobody reads the protocol as stronger than it is.
If one of them starts failing, the hole may have been closed: update the
documentation and turn the test into a defence test.
"""
import numpy as np

import findings as F
from laura_model import (CALIBRATION_DOMAIN, CHALLENGE_DOMAIN, calibration_bytes,
                         challenge_bytes, verify)
from simculture import SimCulture


# --------------------------------------------------------------------------- what it stops
def test_honest_answers_are_accepted():
    assert F.honest_acceptance() >= 0.95


def test_pwm_term_removes_false_rejections():
    assert F.honest_acceptance(pwm_samples=32) >= 0.99


def test_replay_and_foreign_node_are_rejected():
    for kw in ({}, {"pwm_samples": 32}):
        a = F.attack_acceptance(**kw)
        assert a["replay"] <= 0.02, kw
        assert a["other_node"] <= 0.02, kw


def test_tampered_reading_breaks_the_digest():
    assert F.tamper_detected()


def test_domains_keep_record_types_apart():
    ch = {"node": 16, "challenge_id": "aa", "levels": [0] * 12, "readings": [0] * 16,
          "flags": 1, "firmware": 2, "ts_ms": 0}
    cal = {"node": 16, "dark": [0] * 16, "leds": [[0] * 16] * 12, "flags": 1, "firmware": 2, "ts_ms": 0}
    assert challenge_bytes(ch).startswith(CHALLENGE_DOMAIN)
    assert calibration_bytes(cal).startswith(CALIBRATION_DOMAIN)
    assert not CHALLENGE_DOMAIN.startswith(CALIBRATION_DOMAIN)
    assert not CALIBRATION_DOMAIN.startswith(CHALLENGE_DOMAIN)


# --------------------------------------------------------------------------- known weaknesses
def test_known_weakness_least_squares_forger_on_compromised_pi():
    # Few observations: the fit is under- or exactly determined and noise ruins it.
    assert F.forger_acceptance(6) <= 0.5
    assert F.forger_acceptance(13) <= 0.5
    # Enough relayed challenges: the Pi answers new patterns without the culture.
    assert F.forger_acceptance(40) >= 0.9


def test_known_weakness_one_leaked_calibration_is_enough():
    assert F.calibration_leak_acceptance() >= 0.9


def test_known_weakness_synthetic_growth_passes_growth_check():
    ok, _ = F.growth_histories()["synthetic Beer-Lambert (forger)"]
    assert ok


# --------------------------------------------------------------------------- operating limits
def test_drift_requires_frequent_calibration():
    stale, fresh = F.drift_acceptance(0.02)
    assert stale >= 0.9                       # small growth: the 3 % tolerance covers it
    stale, fresh = F.drift_acceptance(0.1)
    assert stale <= 0.5                       # OD +0.1 since calibrating: honest node rejected
    assert fresh >= 0.95                      # recalibrating restores it


def test_pwm_single_sample_is_useless_and_32_is_marginal():
    r = F.pwm_residuals()
    assert r["1 sample"] > 100
    assert r["32 x 112 us (fw 0x02)"] < 25
    assert r["32 x 112 us (fw 0x02)"] > 3 * r["no PWM (ideal)"]   # PWM dominates the error budget


def test_pwm_more_samples_at_the_same_rate_do_not_help():
    # 32 x 112 us = 7 PWM periods exactly: sample 33 lands on the phase of sample 1.
    r = F.pwm_residuals()
    assert abs(r["64 x 112 us"] - r["32 x 112 us (fw 0x02)"]) < 0.5


def test_pwm_incommensurate_spacing_fixes_it():
    r = F.pwm_residuals()
    assert r["256 x 118 us"] < 2 * r["no PWM (ideal)"]


def test_photoresistors_break_the_linear_model():
    assert F.sensor_acceptance("linear") >= 0.95
    assert F.sensor_acceptance("ldr") <= 0.7


def test_saturation_guard():
    with_guard, without, used, clipped = F.saturation_case(level=3000)
    assert clipped > 0
    assert with_guard and not without
    assert used >= 8


def test_too_few_unsaturated_sensors_is_a_rejection():
    node = SimCulture(1)
    fp = node.fingerprint()
    lv = [4095] * 12                          # clips most sensors
    ok, _, used = verify(fp, lv, node.read(lv))
    assert used < 8 and not ok


# --------------------------------------------------------------------------- growth check
def test_growth_check_accepts_a_growing_culture():
    ok, det = F.growth_histories()["real culture"]
    assert ok and det["signature"] >= 0.8 and det["od_change"] > 0.1


def test_growth_check_rejects_frozen_optics():
    ok, det = F.growth_histories()["frozen optics / replayed calibration"]
    assert not ok and abs(det["od_change"]) < 0.02


def test_growth_check_rejects_a_spliced_calibration():
    ok, det = F.growth_histories()["one calibration from another node"]
    assert not ok and not det["monotonic"]


def test_growth_check_rejects_independent_noise():
    ok, det = F.growth_histories()["independent noise"]
    assert not ok and det["signature"] < 0.8


def test_every_finding_is_deterministic():
    # Same seeds, same numbers: the thresholds above are not flaky.
    assert F.honest_acceptance() == F.honest_acceptance()
    assert np.isclose(F.pwm_residuals()["32 x 112 us (fw 0x02)"], F.pwm_residuals()["32 x 112 us (fw 0x02)"])
