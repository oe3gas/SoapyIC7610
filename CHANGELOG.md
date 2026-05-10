# Changelog

All notable changes to SoapyIC7610 will be documented here.

## [0.1.0] - 2026-05-10

### Initial release

- RX streaming via FTDI FT601 USB 3 bridge
- CS16 and CF32 stream formats
- Sample rates: 48k, 96k, 192k, 384k, 768k, 960k, 1920k sps
- Frequency range: 30 kHz – 74.8 MHz
- VFO frequency read/write via CI-V (command 0x25 0x00)
- Antenna selection: ANT1, ANT2
- Automatic device discovery by serial number
- Tested with: SoapySDRUtil, GNU Radio (Python), CubicSDR (device visible)
- Platform: Windows 10/11 x64, PothosSDR 2021.07.25

### Known issues in this release
- `getFrequency()` returns 0.0 (does not affect streaming)
- Log messages contain garbled characters on some systems (cosmetic)
- CubicSDR: device appears in list but Start may fail
