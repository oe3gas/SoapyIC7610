#pragma once
/**
 * SoapyIcom7610.h  –  SoapySDR-Plugin für den Icom IC-7610
 * Projekt: IC-7610 I/Q Streaming Tool
 * Autor:   OE3GAS
 * Stand:   2026-05-09
 *
 * Implementiert die SoapySDR::Device API für den IC-7610.
 * Hardware-Zugriff erfolgt direkt über FTD3XX.dll (FTDI FT601 Bridge).
 *
 * CI-V Protokoll (aus Knowledge.md):
 *   Befehl:   FE FE 98 E0 <cmd...> FD [FF Padding auf 4-Byte-Grenze]
 *   OK:       FE FE E0 98 FB FD
 *   NG:       FE FE E0 98 FA FD
 *   Daten:    FE FE E0 98 <subcmd> <value...> FD
 *
 * KRITISCH (aus Knowledge.md Abschnitt 3):
 *   - Kein CI-V read_pipe() während IQ-Stream läuft → Aussetzer!
 *   - Frequenz setzen: fire-and-forget (WritePipe only, nie ReadPipe)
 *   - Nur ein FTDI-Handle gleichzeitig möglich
 *   - DLL: CDLL (cdecl), nicht stdcall
 */

#include <SoapySDR/Device.hpp>
#include <SoapySDR/Formats.hpp>
#include <SoapySDR/Logger.hpp>
#include <SoapySDR/Registry.hpp>

#include <windows.h>
#include <FTD3XX.h>

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// FTDI Pipe-Adressen (laut DF7CB-Referenz und Knowledge.md)
// ---------------------------------------------------------------------------
constexpr UCHAR PIPE_CMD_OUT = 0x02;  // Bulk OUT  – CI-V Befehle an Radio
constexpr UCHAR PIPE_CMD_IN  = 0x82;  // Bulk IN   – CI-V Antworten vom Radio
constexpr UCHAR PIPE_IQ_IN   = 0x84;  // Bulk IN   – I/Q Datenstrom (Main RX)

// ---------------------------------------------------------------------------
// CI-V Konstanten
// ---------------------------------------------------------------------------
constexpr uint8_t CIV_PREAMBLE_1  = 0xFE;
constexpr uint8_t CIV_PREAMBLE_2  = 0xFE;
constexpr uint8_t CIV_TO_IC7610   = 0x98;  // IC-7610 Adresse
constexpr uint8_t CIV_FROM_PC     = 0xE0;  // PC/Controller Adresse
constexpr uint8_t CIV_END         = 0xFD;  // Frame-Ende
constexpr uint8_t CIV_PAD         = 0xFF;  // Padding auf 4-Byte-Grenze
constexpr uint8_t CIV_OK          = 0xFB;  // Antwort OK
constexpr uint8_t CIV_NG          = 0xFA;  // Antwort Not Good

// Unterstützte Sampleraten (Hz) laut Icom I/Q Reference Guide
constexpr double VALID_SAMPLE_RATES[] = {
    48000, 96000, 192000, 384000, 768000, 960000, 1920000
};

// Frequenzgrenzen IC-7610
constexpr double FREQ_MIN_HZ =    30000.0;   //    30 kHz
constexpr double FREQ_MAX_HZ = 74800000.0;   // 74,8 MHz

// ---------------------------------------------------------------------------
// IC7610Device – Haupt-Klasse
// ---------------------------------------------------------------------------
class IC7610Device : public SoapySDR::Device
{
public:
    // Konstruktor: öffnet FTDI-Handle, initialisiert Gerät
    explicit IC7610Device(const SoapySDR::Kwargs &args);

    // Destruktor: sauberes Shutdown (IQ aus, AbortPipe, Close)
    ~IC7610Device() override;

    // -----------------------------------------------------------------------
    // Identifikation
    // -----------------------------------------------------------------------
    std::string getDriverKey()   const override { return "IC7610"; }
    std::string getHardwareKey() const override { return "IC-7610"; }
    SoapySDR::Kwargs getHardwareInfo() const override;

    // -----------------------------------------------------------------------
    // Kanäle
    // -----------------------------------------------------------------------
    size_t getNumChannels(const int dir) const override;
    bool   getFullDuplex(const int dir, const size_t ch) const override;

    // -----------------------------------------------------------------------
    // Stream-API
    // -----------------------------------------------------------------------
    SoapySDR::Stream* setupStream(
        const int dir,
        const std::string &format,
        const std::vector<size_t> &channels,
        const SoapySDR::Kwargs &args) override;

    void closeStream(SoapySDR::Stream *stream) override;

    int activateStream(
        SoapySDR::Stream *stream,
        const int flags        = 0,
        const long long timeNs = 0,
        const size_t numElems  = 0) override;

    int deactivateStream(
        SoapySDR::Stream *stream,
        const int flags        = 0,
        const long long timeNs = 0) override;

    int readStream(
        SoapySDR::Stream *stream,
        void * const *buffs,
        const size_t numElems,
        int &flags,
        long long &timeNs,
        const long timeoutUs = 100000) override;

    // Native Format: CS16 (signed 16-bit I/Q, 4 Bytes/Sample)
    std::string getNativeStreamFormat(
        const int dir,
        const size_t ch,
        double &fullScale) const override;

    std::vector<std::string> getStreamFormats(
        const int dir,
        const size_t ch) const override;

    size_t getStreamMTU(SoapySDR::Stream *stream) const override;

    // -----------------------------------------------------------------------
    // Frequenz
    // -----------------------------------------------------------------------
    void   setFrequency(const int dir, const size_t ch,
                        const double freq,
                        const SoapySDR::Kwargs &args) override;

    double getFrequency(const int dir, const size_t ch,
                        const std::string &name) const override;

    SoapySDR::RangeList getFrequencyRange(
        const int dir, const size_t ch,
        const std::string &name) const override;

    // -----------------------------------------------------------------------
    // Samplerate
    // -----------------------------------------------------------------------
    void   setSampleRate(const int dir, const size_t ch,
                         const double rate) override;

    double getSampleRate(const int dir, const size_t ch) const override;

    std::vector<double> listSampleRates(
        const int dir, const size_t ch) const override;

    SoapySDR::RangeList getSampleRateRange(
        const int dir, const size_t ch) const override;

    // -----------------------------------------------------------------------
    // Antenne
    // -----------------------------------------------------------------------
    std::vector<std::string> listAntennas(
        const int dir, const size_t ch) const override;

    void        setAntenna(const int dir, const size_t ch,
                           const std::string &name) override;

    std::string getAntenna(const int dir, const size_t ch) const override;

private:
    // -----------------------------------------------------------------------
    // CI-V Hilfsfunktionen
    // -----------------------------------------------------------------------

    // Baut CI-V Frame: FE FE 98 E0 <cmdBytes> FD [FF Padding]
    std::vector<uint8_t> buildCiv(const std::vector<uint8_t> &cmdBytes) const;

    // Sendet CI-V Frame (fire-and-forget — kein ReadPipe!)
    void civWrite(const std::vector<uint8_t> &cmdBytes) const;

    // Sendet CI-V und liest Antwort (NUR vor Stream-Start verwenden!)
    std::vector<uint8_t> civWriteRead(const std::vector<uint8_t> &cmdBytes) const;

    // Prüft ob Antwort-Frame OK (0xFB) enthält
    bool civIsOk(const std::vector<uint8_t> &response) const;

    // I/Q Output ein- oder ausschalten
    void setIqOutput(bool enable) const;

    // Frequenz via CI-V 0x25 0x00 setzen (fire-and-forget)
    void civSetFrequency(double freqHz) const;

    // Frequenz via CI-V 0x25 0x00 lesen (nur vor Stream verwenden!)
    double civGetFrequency() const;

    // BCD Kodierung für Frequenz (5 Bytes, Little-Endian BCD)
    static std::vector<uint8_t> encodeBcdFrequency(double freqHz);
    static double decodeBcdFrequency(const uint8_t *data, size_t len);

    // FTDI Overlapped I/O
    DWORD ftWritePipe(UCHAR pipe, const void *buf, ULONG len) const;
    DWORD ftReadPipe(UCHAR pipe, void *buf, ULONG len, ULONG *transferred,
                     DWORD timeoutMs = 2000) const;

    // -----------------------------------------------------------------------
    // Member-Variablen
    // -----------------------------------------------------------------------
    FT_HANDLE       m_ftHandle   = nullptr;   // FTDI Device Handle
    HANDLE          m_ioEvent    = nullptr;   // Windows Event für Overlapped I/O
    std::string     m_serial;                 // Serial-Number ("23001774")

    double          m_frequency  = 14200000.0; // Aktuelle VFO-Frequenz (Hz)
    double          m_sampleRate = 768000.0;   // Aktuelle Samplerate (Hz)
    std::string     m_antenna    = "ANT1";

    std::atomic<bool> m_streaming{ false };   // Stream läuft gerade?

    // Block-Größe für ReadPipe: 64 KiB — empirisch optimiert (Knowledge.md)
    static constexpr ULONG BLOCK_SIZE = 65536;
};
