# Signing extension point: TROPIC01

**Status: interface only.** The shield v4 schematic includes the TROPIC01 (see [hardware-shield-v4.md](hardware-shield-v4.md)), but there is no driver yet: Clorela-OS ships only `NullSigner`, which answers `NotAvailable` to everything. Nothing in the acquisition path depends on signing today.

This page explains what the interface is for, what an implementation must guarantee, and the steps left to implement it, so that the [Anchor Protocol](https://github.com/walcorg/anchor-protocol) can be developed against it without waiting for the hardware.

## Where it fits

In the Anchor Protocol (draft v0.2), the shield signs a **CaptureReceipt** for each closed measurement window (flow C). It also signs a **Bind** when it is paired with a CiWi wallet (flow B). In both cases the host computes the 32-byte EIP-712 digest and the TROPIC01 signs that digest with the shield's *anchor key*, which never leaves the chip.

```
clorela-acqd ──(measurements, window)──▶ receipt builder (future)
                                              │ EIP-712 digest (32 bytes)
                                              ▼
                                   clorela::Signer::signDigest()
                                              │
                         ┌────────────────────┴─────────────────────┐
                    NullSigner (today)                 Tropic01Signer (future)
                    → NotAvailable                     → SPI → TROPIC01 ECC slot
```

## Interface (`src/signer/signer.h`)

| Call | Returns | Used for |
|---|---|---|
| `anchorPublicKey(&key)` | curve (1 = P-256), X, Y (32 bytes each, big-endian) | `shieldId = keccak256(curve ‖ X ‖ Y)`, via `Signer::anchorIdentifier()` |
| `signDigest(digest32, &sig)` | `r ‖ s`, 32 + 32 bytes, big-endian, **low-s** | CaptureReceipt and Bind signatures |
| `randomBytes(n, &out)` | n bytes from the chip TRNG | `CaptureReceipt.nonce` (16 bytes) |
| `isAvailable()` | false for `NullSigner` | lets the rest of the system degrade gracefully |

Status codes: `Ok`, `NotAvailable`, `NotProvisioned` (chip present, no anchor key yet), `InvalidInput`, `HardwareError`.

### Contract enforced by the base class

These checks run in `Signer` itself, so a new backend cannot skip them:

- The digest must be exactly 32 bytes; otherwise the backend is never called.
- A returned signature must be 32 + 32 bytes with `s ≤ n/2` (P-256). A high-s or malformed result becomes `HardwareError`, because the verifier contract rejects high-s signatures.
- `randomBytes` must return exactly the requested length.

`tests/tst_signer.cpp` checks all of this with a test double; a real `Tropic01Signer` must pass the same tests. The test also confirms that Qt's `QCryptographicHash::Keccak_256` is Ethereum's Keccak-256, not SHA3-256, which is what `shieldId` needs.

## Selecting the backend

`/etc/clorela/clorela.conf`:

```ini
[signer]
backend=none        # "tropic01" once it exists
```

`clorela-acqd` logs the active signer at start-up. The control socket reports it in the `signer` field of `status`.

## Implementation steps (future)

1. **Hardware.** The shield v4 schematic puts the TROPIC01 on **SPI0**: MOSI GPIO10, MISO GPIO9, SCLK GPIO11. Chip select is **GPIO25** (header pin 22, not a hardware CE line) and the chip's GPO goes to **GPIO5** (pin 29). Sensors and the OLED stay on I2C. These pins must still be checked on an assembled board **[TO VERIFY]**.
2. **Image.** Enable SPI0 in `config.txt` with a GPIO chip select on GPIO25 (for example the `spi0-1cs` overlay with `cs0_pin=25`) and add `spidev` to the image (phase 4 recipe).
3. **SDK.** Package Tropic Square's host SDK as a Yocto recipe. **[TO VERIFY]**: the name, repository, license and pinned version of that SDK. The secure session with the chip must be established with the shield's pairing key.
4. **Backend.** Add `Tropic01Signer` in `src/signer/` implementing the three `do…` methods. In `createSigner()`, return it when `signer/backend=tropic01`.
5. **Provisioning.** This runs at the WALCORG bench, not on the node. It reads and validates the factory X.509 certificate, generates the anchor key in a dedicated ECC slot, and locks the slot to the pairing key.
6. **Receipt builder.** Add EIP-712 hashing of `CaptureReceipt` and `Bind` per Anchor SPEC section 5, plus the `dataRoot` Merkle root over the stored samples. Use the shared test vectors planned for Anchor v0.3.

## Open questions that block the implementation

These come from Anchor SPEC section 9 and need answers from Tropic Square:

- Does `ECDSA_Sign` sign a caller-provided 32-byte digest as given, and does it normalise s? The base class re-checks low-s either way.
- Can signing with the anchor-key slot be restricted to one pairing key, and can the slot be locked after provisioning?
- What time source should `windowStart` and `windowEnd` use? The Pi Zero W has no RTC: either NTP over Wi-Fi only, or an RTC on the shield.
- What exactly does `fwHash` cover: the `clorela-acqd` binary or the whole image? The reproducible Yocto build (phase 4) is what lets others recompute it.
