# Laura Encoding (Codificado de Laura)

**Status:** protocol v1, implemented on the node side (node-routine firmware 0x02, `clorela-acqd`) and as a server-side reference in [`tools/laura/`](../tools/laura/). Tested against the simulated node only; **not yet measured on a real culture**.

Laura Encoding lets the WALCORG server check that a reading comes from **a real, specific culture measured right now**, and not from replayed or invented data. It descends from the "Laura's Cypher" idea (2024) of using the culture as a source of physical randomness. It is **authentication, not encryption**: nothing is hidden; what is proved is where a reading came from.

## 1. The physical model

Light from the 12 visible LEDs crosses the culture and reaches the 16 sensors (8 backscatter B0–B7, 8 transmission T0–T7). For sensors that respond linearly, one reading with LED intensities **x** (0–4095 each) is:

```
y = b + M · (x / 4095) + n
```

| Symbol | Meaning | Measured by |
|---|---|---|
| **y** (16) | Sensor readings, ADC counts 0–1023 | `X` (challenge) |
| **b** (16) | Dark baseline: ambient light and offset | page 1 of `C` (same as `G`) |
| **M** (16 × 12) | **The fingerprint**: counts that LED *j* at full scale adds to sensor *i* | pages 2–13 of `C`, minus page 1 |
| **n** | Sensor noise, standard deviation σ | repeated readings |

*M* depends on the biomass, on how the cells are spread, on bubbles and on the exact geometry of each node. Two nodes, or one node at two moments, have different matrices.

## 2. Who does what

| Where | Role | Must **not** hold |
|---|---|---|
| **Node** (Arduino Nano, node-routine) | Physics only: `C` measures *M* and *b*, `X` lights a pattern and reads the 16 sensors | Any secret. It does not know whether an answer is right |
| **Clorela-OS** (`clorela-acqd`) | Relays requests from the server to the node over I2C, builds the canonical record, hashes it and has the **TROPIC01** sign it | The stored *M̂*, the challenge generator, the verification |
| **Server** (WALCORG platform) | Keeps *M̂*, *b̂*, σ per node; generates fresh challenges; verifies answers, digests, signatures and the evolution of *M* over time | — |

```
Server ──challenge (id, x)──▶ clorela-acqd ──'X'──▶ Node
                                                   lights x, reads y
Server ◀─ y + digest + signature ── clorela-acqd ◀── page 0
   └─ check signature, check digest, check y ≈ b̂ + M̂·x, check id is fresh and timely
```

## 3. Node commands (node-routine 0x02)

| Command | Frame | Result |
|---|---|---|
| `C` | `'C'` | Page 1 = dark baseline, pages 2–13 = LED 0–11 alone at the configured intensity (parameter 2, default 4095) |
| `X` | `'X'` + 12 × u16 little-endian (25 bytes) | Sets LED *j* to level *j*, waits the settle time, reads the 16 sensors **averaging 32 samples each** (~3.6 ms, about 7 PWM periods of the TLC59401), turns the LEDs off. Result in page 0 |

The 32-sample average matters: the TLC59401 makes intermediate intensities with PWM at ~1.95 kHz, and a single ADC sample would catch the LED either on or off. It is not perfect, though: see finding F6 in section 8.

## 4. Control socket of `clorela-acqd`

JSON lines on `/run/clorela/acqd.sock` (see [`src/acqd/controlserver.h`](../src/acqd/controlserver.h)):

```json
-> {"cmd":"laura_calibrate","node":16}
<- {"ok":true, ..., "laura":{"type":"calibration","node":16,"dark":[16],"leds":[[16] x 12],
                              "flags":1,"firmware":2,"ts_ms":...,
                              "digest":"<64 hex>","signature":null,"signer":"none","signer_status":"not available"}}

-> {"cmd":"laura_challenge","node":16,"challenge_id":"<2-64 hex>","levels":[12 integers 0-4095]}
<- {"ok":true, ..., "laura":{"type":"challenge","node":16,"challenge_id":"...","levels":[12],"readings":[16],
                              "flags":1,"firmware":2,"ts_ms":...,"digest":"...","signature":null,...}}
```

These two commands wait for the node (a calibration takes a few seconds in the sensitive range) and run in the acquisition thread, between cycles. A node in UVC sterilization refuses them. A node with firmware 0x01 is reported as needing an update.

## 5. Signed records (v1)

Each answer is serialized to canonical bytes, all integers little-endian, then hashed with **SHA-256**. The `Signer` (TROPIC01, P-256, low-s `r ‖ s`) signs that digest. With `NullSigner`, which is what ships today, `signature` is `null` and `signer_status` is `not available`.

```
challenge:   "WALCORG-LAURA-CHALLENGE-v1" | node u8 | len u8 | challenge_id (1-32 B)
             | levels 12 x u16 | readings 16 x u16 | flags u8 | firmware u8 | ts_ms i64
calibration: "WALCORG-LAURA-CALIBRATION-v1" | node u8 | dark 16 x u16 | leds 12 x 16 x u16
             | flags u8 | firmware u8 | ts_ms i64
```

The domain strings keep a challenge signature from being reused as a calibration signature, or as any other message the chip signs. When the Anchor Protocol receipt format is fixed, these records will be committed into the receipt (as the "optical challenge and the node's response to it").

## 6. Server checks

1. **Signature** of the digest against the shield's registered public key, and the **digest** recomputed from the record.
2. **Freshness:** the `challenge_id` is one the server issued, is used once, and the answer arrives within a short window.
3. **Match:** with *M̂*, *b̂* from the last calibration,

   ```
   ŷ = b̂ + M̂ · (x / 4095)
   σᵢ = sqrt(σ₀² + (ε · ŷᵢ)²)            σ₀ = sensor noise, ε = drift allowed since the calibration
   d  = sqrt( mean_i ((yᵢ − ŷᵢ) / σᵢ)² )  over sensors with ŷᵢ < 1000 (no ADC clipping)
   accept if d < τ                       (reference: σ₀ = 4, ε = 0.03, τ = 3)
   ```

   Challenges are drawn so that no sensor is predicted above 1000 counts, which keeps every sensor in its linear range. If fewer than 8 sensors are usable, the answer is rejected. With `pwm_samples=32` the verifier also adds the expected PWM sampling error to σᵢ (finding F6).

   The reference code is [`tools/laura/laura_model.py`](../tools/laura/laura_model.py): `verify()`, `ChallengeRegistry` (check 2: random 16-byte ids bound to one node and one pattern, single use, a failed redeem burns the id, 30 s expiry, per-node hourly cap) and `growth_check()` (check 4).

4. **Culture signature over time:** a growing culture changes all couplings through one latent quantity (Beer-Lambert): `ln M_ij(t) − ln M_ij(0) ≈ −k_ij · OD(t)`. Stack the calibrations of a node as a T × (couplings) matrix of log changes. Its first singular component should explain almost all the variance, and OD(t) should follow a growth curve. Data from another node, shuffled data or independent noise do not. `growth_check()` requires all three: signature ≥ 0.8, the transmission OD has moved by at least 0.02, and it never steps back by more than a quarter of the total change.

## 7. Results on the simulated node

`tools/laura/laura_server_reference.py` against `clorela-acqd --config data/dev.conf`:

| Check | Result |
|---|---|
| 10 fresh challenges from the real (simulated) node | all accepted, d = 0.4–0.9 |
| Old answer replayed for a new pattern | rejected, d ≈ 16 |
| Answer from another geometry (sensors permuted) | rejected, d ≈ 33 |
| One reading changed after signing | rejected (digest mismatch) |
| Culture signature, 6 calibrations, culture growing between them | one component explains 97 % (independent noise: 33 %) |
| Digest computed in Python vs `clorela-acqd` (C++) | identical |

The simulator is a simple Beer-Lambert model, not a real *Chlorella* culture. These numbers show that the protocol and the code work; they say nothing yet about how well a real culture can be told apart.

## 8. Findings and limits

Every finding below is measured on the physical model [`tools/laura/simculture.py`](../tools/laura/simculture.py) (Beer-Lambert culture, TLC59401 PWM sampled by the ADC, linear or photoresistor sensors, 10-bit ADC) and fixed by a test in [`tools/laura/tests/`](../tools/laura/tests/). `python3 tools/laura/findings.py` prints the numbers. The model's constants are plausible, not measured, so the numbers give the size and direction of each effect, not the real node's values.

Tests named `test_known_weakness_*` **pass when the attack works**. They record holes that are still open. If one starts failing, the hole may be closed: update this section.

| # | Finding | Model result | Test |
|---|---|---|---|
| F1 | Honest answers pass with the reference σ | 97 % accepted; 100 % with the PWM term | `test_honest_answers_are_accepted`, `test_pwm_term_removes_false_rejections` |
| F2 | Replay, another node's answer and edited readings are stopped | replay ≤ 1 %, other node ≤ 0.2 %, any edit breaks the digest; the domains keep record types apart | `test_replay_and_foreign_node_are_rejected`, `test_tampered_reading_breaks_the_digest`, `test_domains_keep_record_types_apart` |
| F3 | **Known weakness.** A compromised Pi that relays challenges can fit *b* and *M* by least squares and answer by itself | 6–13 pairs: ≤ 15 % (with 13 unknowns per sensor, noise ruins an exact fit); 20 pairs: 67 %; **40 pairs: 100 %**, more consistent than the real node | `test_known_weakness_least_squares_forger_on_compromised_pi` |
| F4 | **Known weakness.** One relayed calibration *is* the fingerprint | 100 % of answers computed from it pass | `test_known_weakness_one_leaked_calibration_is_enough` |
| F5 | *M* drifts as the culture grows | stale *M*: 96 % at OD +0.02, 20 % at OD +0.1, 0 % at +0.3; recalibrating restores 97 % | `test_drift_requires_frequent_calibration` |
| F6 | The 32-sample average of `X` is marginal | RMS error 17 counts (ideal: 3). 32 × 112 µs is exactly 7 PWM periods, so **64 samples at the same rate give the same error**: the phases repeat. 256 samples spaced 118 µs (59/256 of a period) give 3.6 | `test_pwm_single_sample_is_useless_and_32_is_marginal`, `test_pwm_more_samples_at_the_same_rate_do_not_help`, `test_pwm_incommensurate_spacing_fixes_it` |
| F7 | A linear model needs linear sensors | TEMT6000: 97 %; CdS photoresistors: 47 % | `test_photoresistors_break_the_linear_model` |
| F8 | Clipped sensors must be left out | with 8 sensors clipped, the guard accepts and the unguarded verifier rejects; with most sensors clipped the answer is rejected (< 8 usable) | `test_saturation_guard`, `test_too_few_unsaturated_sensors_is_a_rejection` |
| F9 | `growth_check` tells one growing culture from simple fakes | real: pass (signature 95 %). Frozen optics or a replayed calibration, a calibration spliced in from another node, independent noise: fail | `test_growth_check_*` |
| F10 | **Known weakness.** A forger who knows Beer-Lambert can invent a history that passes | synthetic `M0·exp(−k·OD(t))` with made-up *k*: pass (signature 97 %) | `test_known_weakness_synthetic_growth_passes_growth_check` |
| — | Challenge registry: single use, binding, expiry, rate limit | — | `tests/test_registry.py` |

What follows from them:

- **Against a compromised Raspberry Pi, the optical check does not hold on its own** (F3, F4, F10). The Pi sees every calibration and answer, and it can ask the TROPIC01 to sign whatever it builds. What is left is the per-node hourly cap (it slows F3 down, it does not stop F4), the comparison with reference cultures and between nodes, the Anchor Protocol's per-shield caps, and review. Closing this needs the measurement to happen where the Pi cannot read it, for example calibration results encrypted on the node for the server, which the current Arduino Nano firmware does not do.
- **What it does stop:** naive replay of recorded data, data copied from another node, readings edited after signing, answers from a box without a culture, a frozen optical mask, and a calibration taken from another node (F2, F9).
- **Calibrate often** (F5): the tolerance ε = 3 % covers about OD +0.02–0.05 since the last calibration. Calibrate every acquisition window and keep the history.
- **The PWM sampling of node-routine 0x02 should change** (F6): spacing the samples so they do not repeat their phase (for example 118 µs, 256 samples, about 0.5 s for the 16 sensors), or an RC low-pass on each sensor with a longer settle time. Until then the server can add the PWM term (`pwm_samples=32`), which removes the false rejections at the cost of a little margin (replay acceptance 0.2 % → 1 %). The ADC timing used in the model must be measured on the Nano.
- **Linear sensors are assumed** (F7). TEMT6000 phototransistors are close to linear. With photoresistors (CdS) the server needs a per-pair response curve, for example `C` at several intensities, and `X` must stay close to calibrated levels.
- **Not a key.** Because *M* changes, it cannot be turned into a fixed cryptographic key. It authenticates within a time window.

## 9. Still to do

- Measure σ₀, ε, the settle time and the ADC sample spacing on a real node and a real culture, and rerun the findings with the measured constants.
- node-routine: PWM sampling that does not repeat its phase (F6), or an RC filter on the sensors in the next board revision.
- Server service (in the WALCORG platform or the [Anchor Protocol](https://github.com/walcorg/anchor-protocol)): storage of calibrations, challenge issuing with expiry, verification, culture-signature monitoring.
- Pi ↔ server transport (authenticated, with the shield's key), and the `Tropic01Signer` backend.
- Store calibrations and challenge answers in the local SQLite database for audit.
- Commit the records into the Anchor Protocol capture receipt.
