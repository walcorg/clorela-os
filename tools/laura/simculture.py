# SPDX-License-Identifier: MIT
"""
A physical model of one node v4.5 and its culture, for the Laura Encoding
hardening tests. It is more detailed than the C++ SimulatedNodeBus on the
points where the findings live:

- Beer-Lambert transmission and backscatter driven by one optical density;
- the TLC59401 PWM (~1.95 kHz, all channels start together) sampled by the
  ADC every ~112 us, so the 32-sample average of the 'X' command is modelled;
- linear phototransistors (TEMT6000) or non-linear photoresistors (CdS);
- the 10-bit ADC (clips at 1023) and sensor noise.

It is a model, not a measurement: the constants are plausible, not measured.
"""
import numpy as np

from laura_model import MAX_LEVEL, N_LEDS, N_SENSORS, Fingerprint

PWM_PERIOD_US = 512.0      # 4096 GSCLK at 8 MHz
ADC_SAMPLE_US = 112.0      # analogRead() at the default prescaler, with overhead
ADC_MAX = 1023


class SimCulture:
    """One node with its own geometry and a culture of optical density `od`."""

    def __init__(self, seed, od=0.5, sensor="linear", noise=2.0, samples=32, sample_us=ADC_SAMPLE_US):
        rng = np.random.default_rng(seed)
        self.rng = np.random.default_rng(seed + 1_000_003)
        self.od = od
        self.sensor = sensor
        self.noise = noise
        self.samples = samples
        self.sample_us = sample_us
        # Geometry: unattenuated coupling of each LED to each sensor (counts at full scale).
        self.g_back = 420 * rng.uniform(0.05, 1.0, (8, N_LEDS)) ** 2
        self.g_trans = 900 * rng.uniform(0.05, 1.0, (8, N_LEDS)) ** 2
        # Path-dependent attenuation per unit OD (longer or oblique paths attenuate more).
        self.k_back = rng.uniform(1.0, 2.5, (8, N_LEDS))
        self.k_trans = rng.uniform(0.8, 1.6, (8, N_LEDS))
        self.ambient = rng.uniform(15, 40, N_SENSORS)
        # Photoresistor response: counts = 1023 * L^gamma / (L^gamma + K^gamma).
        self.ldr_gamma = 0.7
        self.ldr_k = 500.0

    # ------------------------------------------------------------------ physics
    def light_matrix(self):
        """Light (in linear-sensor counts) per LED at full scale, 16 x 12."""
        back = self.g_back * (1 - np.exp(-self.k_back * self.od))   # more cells, more scatter
        trans = self.g_trans * np.exp(-self.k_trans * self.od)      # more cells, less light through
        return np.vstack([back, trans])

    def grow(self, dod):
        self.od += dod

    def _duty(self, levels, samples, pwm):
        """Fraction of the ADC samples that catch each LED on, per sensor (16 x 12)."""
        duty = np.asarray(levels, float) / (MAX_LEVEL + 1)
        if not pwm:
            return np.tile(duty, (N_SENSORS, 1))
        out = np.empty((N_SENSORS, N_LEDS))
        for i in range(N_SENSORS):                 # the mux reads one sensor after another
            t0 = self.rng.uniform(0, PWM_PERIOD_US)
            phase = ((t0 + self.sample_us * np.arange(samples)) % PWM_PERIOD_US) / PWM_PERIOD_US
            out[i] = (phase[:, None] < duty[None, :]).mean(axis=0)
        return out

    def _respond(self, light):
        if self.sensor == "linear":
            return light
        if self.sensor == "ldr":
            lg = np.clip(light, 0, None) ** self.ldr_gamma
            return ADC_MAX * lg / (lg + self.ldr_k ** self.ldr_gamma)
        raise ValueError(self.sensor)

    def read(self, levels, samples=None, pwm=True):
        """What the node returns in page 0 after 'X' with these levels."""
        samples = samples or self.samples
        light = self.ambient + (self.light_matrix() * self._duty(levels, samples, pwm)).sum(axis=1)
        y = self._respond(light) + self.rng.normal(0, self.noise, N_SENSORS)
        return [int(v) for v in np.clip(np.rint(y), 0, ADC_MAX)]

    def calibrate(self, level=MAX_LEVEL):
        """Pages 1-13 of 'C': dark, then each LED alone."""
        dark = self.read([0] * N_LEDS)
        leds = []
        for j in range(N_LEDS):
            lv = [0] * N_LEDS
            lv[j] = level
            leds.append(self.read(lv))
        return dark, leds

    def fingerprint(self):
        return Fingerprint(*self.calibrate())


# ---------------------------------------------------------------------- attackers
class LeastSquaresForger:
    """A compromised Raspberry Pi: it relays real challenges, records the
    (pattern, answer) pairs and fits b and M by least squares. With 13 or
    more independent pairs it can answer any new pattern without the culture."""

    def __init__(self):
        self.X, self.Y = [], []

    def observe(self, levels, readings):
        self.X.append(np.r_[1.0, np.asarray(levels, float) / MAX_LEVEL])
        self.Y.append(np.asarray(readings, float))

    def answer(self, levels):
        coef, *_ = np.linalg.lstsq(np.array(self.X), np.array(self.Y), rcond=None)  # (13, 16)
        y = np.r_[1.0, np.asarray(levels, float) / MAX_LEVEL] @ coef
        return [int(v) for v in np.clip(np.rint(y), 0, ADC_MAX)]


def synthetic_growth_history(m0, steps, dod, seed=0, noise=2.0):
    """What a forger who knows Beer-Lambert can invent: M(t) = M0 * exp(-k * OD(t))
    with made-up k per coupling and a smooth growth curve. No culture needed."""
    rng = np.random.default_rng(seed)
    m0 = np.asarray(m0, float)
    k = rng.uniform(0.8, 1.6, m0.shape)
    k[:8] = -rng.uniform(0.5, 1.5, (8, m0.shape[1]))     # backscatter rises with OD
    return [np.clip(m0 * np.exp(-k * dod * t) + rng.normal(0, noise, m0.shape), 0, None)
            for t in range(steps)]
