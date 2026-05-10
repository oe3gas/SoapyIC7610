#pragma once
/**
 * SoapyIcom7610.h  -  SoapySDR-Plugin fuer den Icom IC-7610
 * Projekt: IC-7610 I/Q Streaming Tool
 * Autor:   OE3GAS
 * Stand:   2026-05-10  (v0.3)  (v0.2 – Lazy-Open Umbau)
 *
 * Aenderungen v0.2:
 *   - FTDI-Handle wird NICHT mehr im Konstruktor geoeffnet
 *   - Handle wird erst in activateStream() geoeffnet (ftOpen)
 *   - Handle wird in deactivateStream() / Destruktor geschlossen (ftClose)
 *   - Behebt CubicSDR-Doppel-Open Bug (P7-03)
 *   - Behebt getFrequency()=0.0 Bug (P7-01): Frequenz wird in activateStream gelesen
 *
 * CI-V Protokoll:
 *   Befehl:   FE FE 98 E0 <cmd...> FD [FF Padding auf 4-Byte-Grenze]
 *   OK:       FE FE E0 98 FB FD
 *   NG:       FE FE E0 98 FA FD
 *   Daten:    FE FE E0 98 <subcmd> <value...> FD
 *
 * KRITISCH:
 *   - Kein CI-V read_pipe() waehrend IQ-Stream laeuft -> Aussetzer!
 *   - Frequenz setzen: fire-and-forget (WritePipe only, nie ReadPipe)
 *   - Nur ein FTDI-Handle gleichzeitig moeglich
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
 // FTDI Pipe-Adressen
 // ---------------------------------------------------------------------------
constexpr UCHAR PIPE_CMD_OUT = 0x02;  // Bulk OUT  - CI-V Befehle an Radio
constexpr UCHAR PIPE_CMD_IN = 0x82;  // Bulk IN   - CI-V Antworten vom Radio
constexpr UCHAR PIPE_IQ_IN = 0x84;  // Bulk IN   - I/Q Datenstrom (Main RX)

// ---------------------------------------------------------------------------
// CI-V Konstanten
// ---------------------------------------------------------------------------
constexpr uint8_t CIV_PREAMBLE_1 = 0xFE;
constexpr uint8_t CIV_PREAMBLE_2 = 0xFE;
constexpr uint8_t CIV_TO_IC7610 = 0x98;
constexpr uint8_t CIV_FROM_PC = 0xE0;
constexpr uint8_t CIV_END = 0xFD;
constexpr uint8_t CIV_PAD = 0xFF;
constexpr uint8_t CIV_OK = 0xFB;
constexpr uint8_t CIV_NG = 0xFA;

// Unterstuetzte Sampleraten (Hz) laut Icom I/Q Reference Guide
constexpr double VALID_SAMPLE_RATES[] = {
    48000, 96000, 192000, 384000, 768000, 960000, 1920000
};

// Frequenzgrenzen IC-7610
constexpr double FREQ_MIN_HZ = 30000.0;   //    30 kHz
constexpr double FREQ_MAX_HZ = 74800000.0;   // 74,8 MHz

// ---------------------------------------------------------------------------
// IC7610Device – Haupt-Klasse
// ---------------------------------------------------------------------------
class IC7610Device : public SoapySDR::Device
{
public:
    // Konstruktor: speichert nur Konfiguration, oeffnet KEIN FTDI-Handle
    explicit IC7610Device(const SoapySDR::Kwargs& args);

    // Destruktor: schliesst Handle falls noch offen
    ~IC7610Device() override;

    // -----------------------------------------------------------------------
    // Identifikation
    // -----------------------------------------------------------------------
    std::string getDriverKey()   const override { return "IC7610"; }
    std::string getHardwareKey() const override { return "IC-7610"; }
    SoapySDR::Kwargs getHardwareInfo() const override;

    // -----------------------------------------------------------------------
    // Kanaele
    // -----------------------------------------------------------------------
    size_t getNumChannels(const int dir) const override;
    bool   getFullDuplex(const int dir, const size_t ch) const override;

    // -----------------------------------------------------------------------
    // Stream-API
    // -----------------------------------------------------------------------
    SoapySDR::Stream* setupStream(
        const int dir,
        const std::string& format,
        const std::vector<size_t>& channels,
        const SoapySDR::Kwargs& args) override;

    void closeStream(SoapySDR::Stream* stream) override;

    // activateStream: oeffnet FTDI-Handle, liest Frequenz, startet I/Q
    int activateStream(
        SoapySDR::Stream* stream,
        const int flags = 0,
        const long long timeNs = 0,
        const size_t numElems = 0) override;

    // deactivateStream: stoppt I/Q, schliesst FTDI-Handle
    int deactivateStream(
        SoapySDR::Stream* stream,
        const int flags = 0,
        const long long timeNs = 0) override;

    int readStream(
        SoapySDR::Stream* stream,
        void* const* buffs,
        const size_t numElems,
        int& flags,
        long long& timeNs,
        const long timeoutUs = 100000) override;

    std::string getNativeStreamFormat(
        const int dir,
        const size_t ch,
        double& fullScale) const override;

    std::vector<std::string> getStreamFormats(
        const int dir,
        const size_t ch) const override;

    size_t getStreamMTU(SoapySDR::Stream* stream) const override;

    // -----------------------------------------------------------------------
    // Frequenz
    // -----------------------------------------------------------------------
    // Korrekte virtuelle Signatur (mit name-Parameter):
    void   setFrequency(const int dir, const size_t ch,
        const std::string& name,
        const double freq,
        const SoapySDR::Kwargs& args) override;

    double getFrequency(const int dir, const size_t ch,
        const std::string& name) const override;

    SoapySDR::RangeList getFrequencyRange(
        const int dir, const size_t ch,
        const std::string& name) const override;

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
        const std::string& name) override;

    std::string getAntenna(const int dir, const size_t ch) const override;

private:
    // -----------------------------------------------------------------------
    // FTDI Handle Lebenszyklus (neu in v0.2)
    // -----------------------------------------------------------------------

    // Oeffnet FTDI-Handle + Event. Wirft std::runtime_error bei Fehler.
    // Darf nur aufgerufen werden wenn m_ftHandle == nullptr.
    void ftOpen();

    // Schliesst FTDI-Handle + Event sauber. Idempotent.
    void ftClose();

    // -----------------------------------------------------------------------
    // CI-V Hilfsfunktionen
    // -----------------------------------------------------------------------
    std::vector<uint8_t> buildCiv(const std::vector<uint8_t>& cmdBytes) const;
    void civWrite(const std::vector<uint8_t>& cmdBytes) const;
    std::vector<uint8_t> civWriteRead(const std::vector<uint8_t>& cmdBytes) const;
    bool civIsOk(const std::vector<uint8_t>& response) const;
    void setIqOutput(bool enable) const;
    void civSetFrequency(double freqHz) const;
    double civGetFrequency() const;

    static std::vector<uint8_t> encodeBcdFrequency(double freqHz);
    static double decodeBcdFrequency(const uint8_t* data, size_t len);

    // FTDI Overlapped I/O
    DWORD ftWritePipe(UCHAR pipe, const void* buf, ULONG len) const;
    DWORD ftReadPipe(UCHAR pipe, void* buf, ULONG len, ULONG* transferred,
        DWORD timeoutMs = 2000) const;

    // -----------------------------------------------------------------------
    // Member-Variablen
    // -----------------------------------------------------------------------
    std::string     m_serial;                       // Serial-Number
    double          m_frequency = 14200000.0;      // VFO-Frequenz (Hz)
    double          m_sampleRate = 768000.0;        // Samplerate (Hz)
    std::string     m_antenna = "ANT1";

    // FTDI-Handle und Event: nullptr solange Geraet geschlossen
    FT_HANDLE       m_ftHandle = nullptr;
    HANDLE          m_ioEvent = nullptr;

    std::atomic<bool> m_streaming{ false };

    // Block-Groesse fuer ReadPipe: 64 KiB
    static constexpr ULONG BLOCK_SIZE = 65536;
};