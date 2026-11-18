# Laura Encoding: server-side reference

`laura_server_reference.py` plays the role of the WALCORG server against `clorela-acqd`. It calibrates a node, sends fresh random challenges, verifies the answers and their digests, tries four attacks, and computes the culture signature over several calibrations.

**It is not part of the Clorela-OS image.** The node and the Raspberry Pi only relay and sign; the matrix *M* and the verification belong on the server. See [`docs/laura-encoding.md`](../../docs/laura-encoding.md).

```bash
pip install numpy
./build/src/acqd/clorela-acqd --config data/dev.conf &      # simulated node, socket run-dev/acqd.sock
python3 tools/laura/laura_server_reference.py --socket run-dev/acqd.sock --seed 3
```

Options: `--node 0x10`, `--challenges 10`, `--history 6` (calibrations for the culture signature), `--spacing 150` (extra measurements between them, so the simulated culture grows).
