# SPDX-License-Identifier: MIT
"""ChallengeRegistry: freshness, single use, binding and rate limit."""
import pytest

from laura_model import ChallengeRegistry


class Clock:
    def __init__(self):
        self.t = 1000.0

    def __call__(self):
        return self.t


LV = [100] * 12


def test_issue_and_redeem_once():
    reg = ChallengeRegistry()
    cid = reg.issue(0x10, LV)
    assert len(bytes.fromhex(cid)) == 16
    assert reg.redeem(cid, 0x10, LV) == (True, "ok")
    assert reg.redeem(cid, 0x10, LV) == (False, "challenge id already used")


def test_ids_are_unique():
    reg = ChallengeRegistry(max_per_hour=1000)
    assert len({reg.issue(0x10, LV) for _ in range(500)}) == 500


def test_unknown_id():
    assert ChallengeRegistry().redeem("00" * 16, 0x10, LV) == (False, "unknown challenge id")


def test_bound_to_node_and_pattern():
    reg = ChallengeRegistry()
    cid = reg.issue(0x10, LV)
    assert reg.redeem(cid, 0x11, LV) == (False, "answer from another node")
    cid = reg.issue(0x10, LV)
    assert reg.redeem(cid, 0x10, [101] + LV[1:]) == (False, "pattern does not match the challenge")


def test_a_failed_redeem_burns_the_id():
    # Otherwise an attacker could probe with wrong answers and retry.
    reg = ChallengeRegistry()
    cid = reg.issue(0x10, LV)
    reg.redeem(cid, 0x11, LV)
    assert reg.redeem(cid, 0x10, LV) == (False, "challenge id already used")


def test_expiry():
    clock = Clock()
    reg = ChallengeRegistry(ttl_s=30, clock=clock)
    cid = reg.issue(0x10, LV)
    clock.t += 31
    assert reg.redeem(cid, 0x10, LV) == (False, "answer arrived too late")


def test_rate_limit_per_node_and_window():
    clock = Clock()
    reg = ChallengeRegistry(max_per_hour=3, clock=clock)
    for _ in range(3):
        reg.issue(0x10, LV)
    with pytest.raises(RuntimeError):
        reg.issue(0x10, LV)
    reg.issue(0x11, LV)                      # other nodes are not affected
    clock.t += 3601
    reg.issue(0x10, LV)                      # the window slides
