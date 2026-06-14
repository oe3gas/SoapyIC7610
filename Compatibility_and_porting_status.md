# SoapyIC7610 — Host Application Compatibility, Cross-Platform Porting Scope, and Maintenance Status

This document summarizes which SDR applications can use the `SoapyIcom7610` SoapySDR
plugin, explains why SDR# (SDRSharp) is intentionally **not** a target, outlines what a
Linux and macOS port would require, and states the project's maintenance status.

It is intended as a reference for users and for anyone considering a fork or port.

---

## 1. Scope and constraints

`SoapyIcom7610` is a SoapySDR plugin (a Windows DLL) that exposes the Icom IC-7610 as a
fully featured SDR source, independent of Icom's ExtIO/HDSDR path. Three properties of the
current `v0.3.0` build determine where it can be used:

1. **Windows-only.** The plugin uses `FTD3XX.dll`, `windows.h`, and Windows Overlapped I/O
   (`OVERLAPPED`, `WaitForSingleObject`, `GetOverlappedResult`). No Linux/macOS binary
   exists today.
2. **SoapySDR ABI 0.8.** Built against PothosSDR 2021.07.25 (SoapySDR 0.8.1); the module
   lives in `modules0.8`. The host application must use a matching SoapySDR ABI, otherwise
   the module will not load.
3. **RX-only, CS16/CF32.** Receive direction only; native sample format `CS16`, also
   offering `CF32`.

A key distinction throughout this document: a **native SoapySDR host** loads the plugin
directly. Programs reached only over a network bridge (e.g. RTL-TCP) are a different,
indirect path and are not counted as native hosts here.

---

## 2. Compatible SoapySDR host applications (Windows)

The following native SoapySDR hosts run on Windows and can load `SoapyIcom7610.dll`.

| Application | Native SoapySDR host | Windows | IC-7610 suitability | Status / note |
|---|---|---|---|---|
| **CubicSDR** | yes | yes | spectrum, audio, tuning | Verified working with this plugin (v0.2.6a) |
| **SDRangel** | yes | yes | expected to work | Qt5/OpenGL SDR and signal analyzer; not yet tested here |
| **SDR++** | yes (`soapy_source` module) | yes | works | Requires a matching PothosSDR/SoapySDR install on Windows |
| **GNU Radio** (`gr-soapy`) | yes | yes (via PothosSDR) | works | Dynamically loaded Soapy modules; verify with `SoapySDRUtil --find` |
| **Pothos Flow** | yes (SDR source/sink blocks) | yes | works | Useful for inspecting raw streams |
| **QSpectrumAnalyzer** | yes (Python bindings) | yes (with PothosSDR) | spectrum only | Python-based analyzer |
| **GQRX** | indirect (via GrOsmoSDR, `soapy=0`) | limited | conditional | Primarily Linux; Windows support historically problematic |

Recommended practical order: **CubicSDR** (confirmed) → **SDR++** → **SDRangel** →
**GNU Radio / Pothos Flow** for stream debugging.

### Practical caveats

- **ABI / PothosSDR conflict (SDR++):** SDR++ Windows builds ship their own SoapySDR DLLs.
  If their SoapySDR version differs from the plugin's `modules0.8` ABI, the module is not
  found. Point SDR++ at the same PothosSDR installation that contains the plugin.
- **Multiple Soapy modules collide:** Disabling unused SoapySDR driver DLLs (renaming them
  to `.disabled`) avoids scan hangs and module conflicts. This matches upstream guidance to
  install only the Soapy modules actually needed.
- **Driver key is case-sensitive:** use `driver=IC7610` (uppercase) in every host.

---

## 3. SDR# (SDRSharp) is intentionally not a target

SDR# is **not** a SoapySDR host — it cannot load SoapySDR modules at all. Adapting the
plugin to SDR# would mean replacing the entire host-facing layer, not "adapting the DLL."
Even via the ExtIO route, SDR# is a dead end on current versions:

- **ExtIO support was removed from SDR#.** Native ExtIO existed only in older builds; later
  versions moved to a `FrontEnds.xml` frontend system and then whitelisted source frontends
  (around build 1500). The current x64 builds (`SDRSharp.dotnet8.exe` /
  `SDRSharp.dotnet9.exe`) have no ExtIO frontend.
- **32-bit vs 64-bit.** ExtIO is historically a 32-bit interface (HDSDR is 32-bit), so
  Icom's `ExtIO7610.dll` is almost certainly x86. A 64-bit SDR# process cannot load a
  32-bit DLL.

Icom's `ExtIO7610.dll` works as expected in **HDSDR**, which remains the correct ExtIO host.
For SDR#-style features (waterfall, plugins), the clean path is a native SoapySDR program
such as SDR++, not the legacy ExtIO bridge.

**Summary:** The IC-7610 has two mature, working paths — **ExtIO → HDSDR** and
**SoapySDR → CubicSDR/SDR++**. SDR# falls into the gap between them (neither modern ExtIO
nor SoapySDR), and closing that gap is not worthwhile.

---

## 4. Cross-platform porting

### Layered architecture

The plugin separates conceptually into two layers (currently mixed in
`SoapyIcom7610.cpp`):

- **Device layer (reusable):** FTDI D3XX I/O, CI-V frame builder/parser, BCD encoding, pipe
  logic (drain-read, lazy-open, fire-and-forget). Knows the IC-7610 and FTDI only.
- **Host layer (SoapySDR):** the `SoapySDR::Device` subclass (`readStream`,
  `setFrequency`, `activateStream`, ...).

The SoapySDR host layer is platform-independent by design. The CI-V logic and BCD encoding
also compile unchanged under gcc/clang. **Only the low-level I/O primitives and a few
MSVC-specific constructs are Windows-bound.**

### Linux — feasible (not maintained here, open to forks)

A Linux port is technically realistic. Required changes:

- Rewrite the Win32 I/O primitives (`OVERLAPPED`, `CreateEventW`, `WaitForSingleObject`,
  `GetOverlappedResult`, `Sleep`, `CloseHandle`) against the Unix `libftd3xx` overlapped
  model (`FT_InitializeOverlapped` / `FT_ReleaseOverlapped`).
- Link against `libftd3xx.so` instead of `FTD3XX.dll` / `FTD3XX.lib`.
- Remove MSVC-only items: `#pragma execution_character_set("utf-8")`,
  `__declspec(dllexport)` (use `__attribute__((visibility("default")))` or the SoapySDR
  CMake module macros), and the `/INCLUDE:SoapyIC7610_loader` linker trick (gcc/clang
  handle the static `SoapySDR::Registry` differently; use `__attribute__((used))` or the
  standard `SOAPY_SDR_MODULE_UTIL` macros).
- On Linux, `libftd3xx` accesses the device via libusb; the `ftdi_sio` kernel module
  typically must be unbound/blacklisted so it does not claim the device.

**Reference:** the open-source project `df7cb/ic7610ftdi` (MIT, by Christoph Berg DF7CB)
demonstrates that user-space D3XX access to the IC-7610 works on Linux and serves as a
proven template.

### macOS — not pursued

A macOS port is **not pursued**, primarily for lack of a reference implementation:

- The `libftd3xx.dylib` exists (FTDI provides D3XX for Windows, Linux, and macOS), so it is
  not strictly impossible.
- However, FTDI D3XX over USB 3 on macOS is far less battle-tested than on Windows/Linux,
  there is no known working IC-7610 reference on macOS, and both Apple Silicon (arm64) and
  Intel (x86_64) would need separate validation.

macOS is therefore classified as "technically possible but unvalidated, without a reference,
and out of scope."

### "Three DLLs?" — clarification

A "DLL" is a Windows artifact. The equivalent build outputs are:

| Artifact | Host layer | Platform | Source | Status |
|---|---|---|---|---|
| `SoapyIcom7610.dll` | SoapySDR | Windows | shared codebase | Final (v0.3.0) |
| `libSoapyIcom7610.so` | SoapySDR | Linux | shared codebase | Feasible; `df7cb` reference |
| `libSoapyIcom7610.dylib` | SoapySDR | macOS | shared codebase | Out of scope (no reference) |
| `ExtIO_IC7610.dll` | ExtIO (push) | Windows | shares device layer only | Separate project; not pursued |

These are **not** three separate forks. The proper design is a single codebase with a thin
platform abstraction around the I/O layer (`#ifdef _WIN32 / __linux__ / __APPLE__`). The
SoapySDR logic, CI-V, and BCD code are 100% shared; only the small I/O module branches per
platform.

---

## 5. Maintenance status

**No further development of `SoapyIcom7610.dll` is planned by the author (OE3GAS).**

- `v0.3.0` is the final release from the author's side. It is functional and stable for core
  use in CubicSDR (spectrum, audio, frequency tuning).
- The **Linux port is documented as feasible** above and is open for anyone who wishes to
  fork and implement it, using `df7cb/ic7610ftdi` as a reference.
- The **macOS port is explicitly not pursued** due to the lack of a working reference
  implementation.
- **SDR# is not a target** for the reasons given in Section 3.

Contributions and forks are welcome under the project's MIT license, but the author will not
be undertaking the Linux/macOS ports, an SDR#/ExtIO variant, or further feature work.

---

## 6. References

- SoapySDR — supported platforms (authoritative list):
  <https://github.com/pothosware/SoapySDR/wiki>
- GNU Radio — Soapy blocks: <https://wiki.gnuradio.org/index.php/Soapy>
- SDR++: <https://www.sdrpp.org/> · <https://github.com/AlexandreRouma/SDRPlusPlus>
- SDRangel: <https://github.com/f4exb/sdrangel>
- CubicSDR: <http://cubicsdr.com/>
- FTDI D3XX drivers (Windows / Linux / macOS): <https://ftdichip.com/drivers/d3xx-drivers/>
- FTDI FT600/601 software examples: <https://ftdichip.com/software-examples/ft600-601-software-examples-2/>
- Linux reference implementation — `df7cb/ic7610ftdi` (MIT)
- Community ExtIO bridge for SDR# 17xx (x86): <https://github.com/jpwichern/SDRSharp.ExtIOSDR>

---

*Project: SoapyIC7610 — <https://github.com/oe3gas/SoapyIC7610> · License: MIT · Author: OE3GAS*