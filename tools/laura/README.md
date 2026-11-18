# Laura Encoding: server-side reference and hardening tests

`laura_server_reference.py` plays the role of the WALCORG server against `clorela-acqd`. It calibrates a node, sends fresh random challenges, verifies the answers and their digests, tries four attacks, and computes the culture signature over several calibrations.

**It is not part of the Clorela-OS image.** The node and the Raspberry Pi only relay and sign; the matrix *M* and the verification belong on the server. See [`docs/laura-encoding.md`](../../docs/laura-encoding.md).

```bash
pip install numpy
./build/src/acqd/clorela-acqd --config data/dev.conf &      # simulated node, socket run-dev/acqd.sock
python3 tools/laura/laura_server_reference.py --socket run-dev/acqd.sock --seed 3
```

Options: `--node 0x10`, `--challenges 10`, `--history 6` (calibrations for the culture signature), `--spacing 150` (extra measurements between them, so the simulated culture grows).

## Files

| File | What it is |
|---|---|
| `laura_model.py` | Server-side logic: canonical bytes and digest, `Fingerprint`, `verify()` (with saturation guard, minimum sensor count and optional PWM term), `ChallengeRegistry`, `culture_signature()`, `growth_check()` |
| `laura_server_reference.py` | End-to-end demo against `clorela-acqd` |
| `simculture.py` | Physical model of a node and its culture (Beer-Lambert, TLC59401 PWM, ADC, linear or photoresistor sensors) and two attackers: a least-squares forger and a synthetic growth history |
| `findings.py` | Measures each finding of `docs/laura-encoding.md` section 8 and prints a report |
| `tests/` | pytest: one test per finding, and the challenge registry |

```bash
pip install numpy pytest
python3 tools/laura/findings.py
python3 -m pytest tools/laura/tests
```

Tests named `test_known_weakness_*` pass **when the attack works**: they record holes that are still open.
