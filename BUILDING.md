# Building SoapyIC7610

Step-by-step build instructions for Windows.

## Prerequisites

### 1. Visual Studio 2022

Install [Visual Studio 2022 Community](https://visualstudio.microsoft.com/vs/community/)
with the **"Desktop development with C++"** workload.

### 2. PothosSDR

Download and install [PothosSDR 2021.07.25-vc16-x64](https://downloads.myriadrf.org/builds/PothosSDR/)
with default path (`C:\Program Files\PothosSDR`).

This installs SoapySDR 0.8.1, CubicSDR, GNU Radio and all dependencies.

### 3. FTDI D3XX Library

Download `FTD3XXLibrary_v1.3.0.2.zip` from [ftdichip.com](https://ftdichip.com/drivers/d3xx-drivers/).

> **Important:** Download the **library** ZIP, not the driver installer.
> Do NOT install the FTDI kernel driver — it would replace the Icom driver.

Extract and copy two files into the project directory:

| Source in ZIP | Destination |
|---|---|
| `\inc\FTD3XX.h` | `SoapyIC7610\FTD3XX.h` |
| `\lib\cpp\x64\FTD3XX.lib` | `SoapyIC7610\FTD3XX.lib` |

## Build Steps

### 1. Clone the repository

```bat
git clone https://github.com/oe3gas/SoapyIC7610.git
cd SoapyIC7610
```

### 2. Copy FTD3XX files

```bat
copy \path\to\FTD3XXLibrary\inc\FTD3XX.h .
copy \path\to\FTD3XXLibrary\lib\cpp\x64\FTD3XX.lib .
```

### 3. Open in Visual Studio

File → Open → CMake... → select the `SoapyIC7610` folder.

VS will auto-configure CMake. You should see in the output:
```
[CMake] -- SoapySDR found: 1
[CMake] -- Configuring done
```

### 4. Select Release configuration

In the toolbar dropdown, select **x64-release** (not x64-debug).

> Debug builds use MSVCP140D.dll which is not available in PothosSDR.

### 5. Build

`Ctrl+Shift+B` or Build → Build All.

Output:
```
[1/2] Building CXX object CMakeFiles\SoapyIcom7610.dir\SoapyIcom7610.cpp.obj
[2/2] Linking CXX shared library SoapyIcom7610.dll
Build succeeded.
```

The DLL is in `out\build\x64-release\SoapyIcom7610.dll`.

## Deployment

### Copy the plugin

```bat
copy "out\build\x64-release\SoapyIcom7610.dll" "C:\Program Files\PothosSDR\lib\SoapySDR\modules0.8\"
```

### Copy FTD3XX runtime

```bat
copy FTD3XX.dll "C:\Program Files\PothosSDR\bin\"
```

> `FTD3XX.dll` goes into `bin\`, **not** into `modules0.8\`.
> SoapySDR scans `modules0.8\` for plugins and would try to load FTD3XX.dll as one.

### Verify

```
SoapySDRUtil --info
```

Look for:
```
Module found: ...modules0.8/SoapyIcom7610.dll
Available factories... ..., IC7610
```

## CMake Notes

`CMakePresets.json` sets `SoapySDR_DIR` to `C:/Program Files/PothosSDR/cmake`.
This is required because PothosSDR places `SoapySDRConfig.cmake` there
instead of the standard `lib/cmake/SoapySDR/` location.

If PothosSDR is installed to a different path, update `CMakePresets.json`
and `CMakeLists.txt` accordingly.

## Troubleshooting

**`SoapySDR found: 1` but `SoapySDR include:` is empty**  
→ Known PothosSDR 2021 issue. The `INTERFACE_INCLUDE_DIRECTORIES` of the
import target is not set. The CMakeLists.txt sets the path explicitly —
this is expected behavior.

**`IC7610` not in `Available factories`**  
→ Check that `FTD3XX.dll` is in `PothosSDR\bin\` (not in `modules0.8\`).
→ Rebuild as Release (not Debug).

**`FT_Create failed (status=3)`**  
→ Another process has the device open (HDSDR, a previous instance).
→ Power-cycle the IC-7610 and try again.
