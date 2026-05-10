# SoapyIC7610

A [SoapySDR](https://github.com/pothosware/SoapySDR) plugin for the **Icom IC-7610** transceiver.

Enables the IC-7610 as a native SDR source in **CubicSDR**, **SDRangel**, **GNU Radio**, **SDR#** and other SoapySDR-compatible software — without HDSDR or Icom's ExtIO.dll.

> **Author:** OE3GAS  
> **License:** MIT  
> **Status:** Beta — RX working, TX not implemented  
> **Platform:** Windows (Linux port planned)

---

## Features

- Full I/Q streaming via USB 3 (FTDI FT601 bridge)
- Native CS16 and CF32 stream formats
- Sample rates: 48k, 96k, 192k, 384k, 768k, 960k, 1920k sps
- Frequency range: 30 kHz – 74.8 MHz
- VFO frequency read/write via CI-V
- Antenna selection: ANT1, ANT2
- Automatic device discovery

## Hardware Requirements

- Icom IC-7610 connected via USB 3
- FTDI FT601 driver installed (ships with IC-7610: `IC7610USBIQ.sys` v1.3.0.2)
- **HDSDR must be closed** — exclusive device access

## Software Requirements

- Windows 10/11 x64
- [PothosSDR 2021.07.25](https://downloads.myriadrf.org/builds/PothosSDR/) (includes SoapySDR 0.8.1 + CubicSDR)
- Visual Studio 2022 with C++ Desktop workload (for building)
- FTDI D3XX Library v1.3.0.2 (`FTD3XXLibrary_v1.3.0.2.zip` from [ftdichip.com](https://ftdichip.com/drivers/d3xx-drivers/))

---

## Installation (pre-built)

1. Download `SoapyIC7610.dll` from [Releases](../../releases)
2. Copy to `C:\Program Files\PothosSDR\lib\SoapySDR\modules0.8\`
3. Copy `FTD3XX.dll` (from FTDI SDK `\lib\cpp\x64\`) to `C:\Program Files\PothosSDR\bin\`
4. Verify: open a command prompt and run:

```
SoapySDRUtil --find="driver=IC7610"
```

Expected output:
```
Found device 0
  driver  = IC7610
  label   = Icom IC-7610
  serial  = 23001774
```

---

## Building from Source

### Prerequisites

1. Install **Visual Studio 2022** with the "Desktop development with C++" workload
2. Install **PothosSDR** (see Software Requirements above)
3. Download `FTD3XXLibrary_v1.3.0.2.zip` from ftdichip.com and extract:
   - Copy `\inc\FTD3XX.h` → `SoapyIC7610\FTD3XX.h`
   - Copy `\lib\cpp\x64\FTD3XX.lib` → `SoapyIC7610\FTD3XX.lib`

### Build

Open the `SoapyIC7610` folder in Visual Studio 2022 (File → Open → CMake...).

VS will detect `CMakeLists.txt` and configure automatically.  
Select configuration **x64-release** and build (`Ctrl+Shift+B`).

The plugin DLL is written to:
```
SoapyIC7610\out\build\x64-release\SoapyIcom7610.dll
```

### Deploy

```bat
copy "out\build\x64-release\SoapyIcom7610.dll" "C:\Program Files\PothosSDR\lib\SoapySDR\modules0.8\"
copy "FTD3XX.dll" "C:\Program Files\PothosSDR\bin\"
```

> **Note:** `FTD3XX.dll` must go into `PothosSDR\bin\`, NOT into `modules0.8\` —
> SoapySDR would try to load it as a plugin otherwise.

---

## Verification

```
SoapySDRUtil --probe="driver=IC7610"
```

Expected output (abbreviated):
```
[INFO] IC7610: Device opened successfully
[INFO] IC7610: VFO frequency = 14200000 Hz
-- Device identification
  driver=IC7610, hardware=IC-7610, serial=23001774
-- RX Channel 0
  Stream formats: CS16, CF32
  Sample rates: 0.048, 0.096, 0.192, 0.384, 0.768, 0.96, 1.92 MSps
```

### Python test (requires Python 3.9)

```
pip install numpy
py -3.9 tools\soapy_test.py
```

---

## Usage with SDR Applications

### CubicSDR
Start CubicSDR (from `C:\Program Files\PothosSDR\bin\CubicSDR.exe`).
Select **Icom IC-7610** from the device list and click **Start**.

> Note: If the device does not appear, restart the IC-7610 and try again.
> After using HDSDR, the radio must be power-cycled before SoapyIC7610 can open it.

### GNU Radio
Use the **Soapy Source** block with `driver=IC7610`.

### SDR# / SDR++
Use via RTL-TCP plugin (run `iq_rtltcp.py` separately) or
wait for native SoapySDR support in a future release.

---

## Known Issues

| Issue | Status | Notes |
|-------|--------|-------|
| `getFrequency()` returns 0.0 | Fix in progress | Does not affect streaming |
| Log messages: garbled umlauts | Fix in progress | Cosmetic only |
| CubicSDR: Start fails | Investigating | Use via `--probe` or GNU Radio |
| Sample rate cannot be set via CI-V | Planned | Set via HDSDR before use |

---

## Architecture

```
SDR Application (CubicSDR, GNU Radio, ...)
        |
   SoapySDR API
        |
  SoapyIcom7610.dll
        |
    FTD3XX.dll  (FTDI D3XX USB 3 driver)
        |
   IC-7610 USB
   (FT601 bridge)
        |
  [PIPE 0x84] I/Q stream  (CS16, up to 1.92 MSps)
  [PIPE 0x02] CI-V commands (frequency, IQ on/off)
  [PIPE 0x82] CI-V responses
```

**Critical constraint:** Only one FT_HANDLE can be open at a time.
CI-V commands during streaming are fire-and-forget (no response read) to avoid dropouts.

---

## Credits

- **[DF7CB](https://github.com/df7cb/ic7610ftdi)** — Original C reference implementation for Linux (MIT license).
  This project ports the FTDI access layer to C++ and wraps it in the SoapySDR API.
- **Icom Inc.** — IC-7610 I/Q Output Reference Guide (publicly available documentation)
- **FTDI** — FT601 chip and D3XX driver/library

---

## License

MIT License — see [LICENSE](LICENSE)

This project is not affiliated with or endorsed by Icom Inc. or FTDI.
