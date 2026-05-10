"""
soapy_test.py  –  SoapySDR Python-Test für den Icom IC-7610
Projekt: IC-7610 I/Q Streaming Tool
Autor:   OE3GAS
Stand:   2026-05-09

Voraussetzungen:
  - Python 3.9 (wegen PothosSDR _SoapySDR.pyd)
  - SoapyIcom7610.dll im PothosSDR modules-Ordner
  - HDSDR geschlossen, IC-7610 eingeschaltet

Ausführen:
  py -3.9 soapy_test.py
"""

import sys
import os

# PothosSDR Python 3.9 Bindings einbinden
POTHOS = r"C:\Program Files\PothosSDR"
sys.path.insert(0, os.path.join(POTHOS, r"lib\python3.9\site-packages"))

# PothosSDR\bin muss im PATH sein damit SoapySDR.dll gefunden wird
os.environ["PATH"] = os.path.join(POTHOS, "bin") + os.pathsep + os.environ["PATH"]

import SoapySDR
from SoapySDR import SOAPY_SDR_RX, SOAPY_SDR_CS16
import numpy as np

print("=" * 56)
print("  IC-7610 SoapySDR Python-Test")
print("=" * 56)

# ---------------------------------------------------------------------------
# Schritt 1: Gerät finden
# ---------------------------------------------------------------------------
print("\n[ Schritt 1 ] Suche IC-7610 ...")
results = SoapySDR.Device.enumerate(dict(driver="IC7610"))
if not results:
    print("  FEHLER: Kein IC-7610 gefunden!")
    sys.exit(1)

print(f"  Gefunden: {results[0].get('label', '?')}")
print(f"  Serial:   {results[0].get('serial', '?')}")

# ---------------------------------------------------------------------------
# Schritt 2: Gerät öffnen
# ---------------------------------------------------------------------------
print("\n[ Schritt 2 ] Öffne Gerät ...")
sdr = SoapySDR.Device(dict(driver="IC7610"))
print(f"  Hardware:  {sdr.getHardwareKey()}")
print(f"  Frequenz:  {sdr.getFrequency(SOAPY_SDR_RX, 0) / 1e6:.3f} MHz")
print(f"  Samplerate:{sdr.getSampleRate(SOAPY_SDR_RX, 0) / 1e3:.0f} kHz")

# ---------------------------------------------------------------------------
# Schritt 3: Stream einrichten und aktivieren
# ---------------------------------------------------------------------------
print("\n[ Schritt 3 ] Stream aktivieren ...")
rxStream = sdr.setupStream(SOAPY_SDR_RX, SOAPY_SDR_CS16)
sdr.activateStream(rxStream)
print("  Stream aktiv ✓")

# ---------------------------------------------------------------------------
# Schritt 4: Samples lesen
# ---------------------------------------------------------------------------
print("\n[ Schritt 4 ] Lese 16384 Samples ...")
NUM_SAMPLES = 16384
buf = np.zeros(NUM_SAMPLES * 2, dtype=np.int16)  # CS16: I und Q je int16

sr = sdr.readStream(rxStream, [buf], NUM_SAMPLES, timeoutUs=2000000)

if sr.ret < 0:
    print(f"  FEHLER: readStream returned {sr.ret}")
else:
    print(f"  Gelesen: {sr.ret} Samples ✓")

    # Komplexe Zahlen aus I/Q Paaren
    iq = buf[0::2].astype(np.float32) + 1j * buf[1::2].astype(np.float32)
    iq /= 32768.0

    print(f"  Max Amplitude: {np.max(np.abs(iq)):.4f}")
    print(f"  RMS:           {np.sqrt(np.mean(np.abs(iq)**2)):.4f}")
    print(f"  Erste 4 Samples: {iq[:4]}")

# ---------------------------------------------------------------------------
# Schritt 5: Sauber beenden
# ---------------------------------------------------------------------------
print("\n[ Schritt 5 ] Stream beenden ...")
sdr.deactivateStream(rxStream)
sdr.closeStream(rxStream)
print("  Stream geschlossen ✓")

print("\n" + "=" * 56)
print("  Test abgeschlossen!")
print("=" * 56)
