/**
 * SoapyIcom7610.cpp  -  SoapySDR-Plugin fuer den Icom IC-7610
 * Projekt: IC-7610 I/Q Streaming Tool
 * Autor:   OE3GAS
 * Stand:   2026-05-10  (v0.3 – Release: Debug-Logging entfernt)
 *
 * Aenderungen v0.2 gegenueber v0.1:
 *   - FTDI-Handle wird erst in activateStream() geoeffnet (ftOpen)
 *   - FTDI-Handle wird in deactivateStream() geschlossen (ftClose)
 *   - Destruktor schliesst Handle falls noch offen (Sicherheitsnetz)
 *   - getFrequency() gibt jetzt echten VFO-Wert zurueck (P7-01 fix)
 *   - UTF-8 Pragma (P7-02 fix): keine Umlaute in Strings mehr
 *   - CubicSDR Doppel-Open vermieden (P7-03 fix)
 *   - setFrequency korrekte virtuelle Signatur (mit name-Parameter)
 *   - IQ-Enable mit civWriteRead-Bestaetigung
 *   - Pipe-Drain nach FT_Create (verhindert Stale-Data)
 *   - Sleep(500) nach IQ-Enable (Radio braucht Zeit zum Hochfahren)
 */

#pragma execution_character_set("utf-8")

#include "SoapyIcom7610.h"

#include <SoapySDR/Logger.hpp>
#include <SoapySDR/Registry.hpp>
#include <SoapySDR/Formats.hpp>

#include <stdexcept>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>

 // ---------------------------------------------------------------------------
 // Konstanten
 // ---------------------------------------------------------------------------
static constexpr DWORD IO_TIMEOUT_MS = 2000;
static constexpr DWORD IO_TIMEOUT_IQ = 1000;
static constexpr ULONG MAX_READ_BYTES = 65536;

// ---------------------------------------------------------------------------
// Konstruktor / Destruktor
// ---------------------------------------------------------------------------

IC7610Device::IC7610Device(const SoapySDR::Kwargs& args)
{
    // Nur Konfiguration speichern – KEIN FT_Create hier!
    // Das Handle wird erst in activateStream() geoeffnet.
    // Grund: CubicSDR ruft makeIC7610() bereits beim Anklicken des
    // Eintrags auf, und nochmals beim Start. FT_Create zweimal auf
    // dasselbe Geraet fuehrt zu Status=3 (FT_DEVICE_NOT_OPENED).

    m_serial = "23001774";
    if (args.count("serial"))
        m_serial = args.at("serial");
    SoapySDR_logf(SOAPY_SDR_INFO,
        "IC7610: Instanz erstellt, serial=%s (Handle noch nicht geoeffnet)",
        m_serial.c_str());
}

IC7610Device::~IC7610Device()
{
    // Sicherheitsnetz: Handle schliessen falls noch offen
    ftClose();
    SoapySDR_log(SOAPY_SDR_INFO, "IC7610: Instanz zerstoert");
}

// ---------------------------------------------------------------------------
// FTDI Handle Lebenszyklus (neu in v0.2)
// ---------------------------------------------------------------------------

void IC7610Device::ftOpen()
{
    if (m_ftHandle != nullptr)
    {
        SoapySDR_log(SOAPY_SDR_DEBUG, "IC7610: ftOpen() - Handle bereits offen, ueberspringe");
        return;
    }

    SoapySDR_logf(SOAPY_SDR_INFO, "IC7610: Oeffne Geraet serial=%s", m_serial.c_str());

    // Windows Event fuer Overlapped I/O
    m_ioEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!m_ioEvent)
    {
        throw std::runtime_error("IC7610: CreateEventW fehlgeschlagen");
    }

    // FTDI Geraet oeffnen
    FT_STATUS status = FT_Create(
        const_cast<char*>(m_serial.c_str()),
        FT_OPEN_BY_SERIAL_NUMBER,
        &m_ftHandle
    );

    if (status != FT_OK || !m_ftHandle)
    {
        CloseHandle(m_ioEvent);
        m_ioEvent = nullptr;
        m_ftHandle = nullptr;
        std::string err = "IC7610: FT_Create fehlgeschlagen (status=" + std::to_string(status) + ")";
        throw std::runtime_error(err + ". Ist HDSDR geschlossen?");
    }

    // Stale CI-V Daten aus vorheriger Session entfernen (drain read)
    // FT_FlushPipe funktioniert nur fuer OUT-Pipes, daher manueller Drain
    {
        uint8_t drainBuf[256] = {};
        ULONG drained = 0;
        OVERLAPPED drainOv = {};
        drainOv.hEvent = m_ioEvent;
        ResetEvent(m_ioEvent);
        FT_STATUS ds = FT_ReadPipe(m_ftHandle, PIPE_CMD_IN,
            drainBuf, sizeof(drainBuf), &drained, &drainOv);
        if (ds == 24 /*FT_IO_PENDING*/) {
            // Kurz warten: max 100ms
            DWORD dw = WaitForSingleObject(m_ioEvent, 100);
            if (dw == WAIT_TIMEOUT) {
                FT_AbortPipe(m_ftHandle, PIPE_CMD_IN);
            }
            else {
                GetOverlappedResult(m_ftHandle, &drainOv, &drained, FALSE);
                char buf[64];
                SoapySDR_logf(SOAPY_SDR_DEBUG, "IC7610: ftOpen - drained stale bytes from CMD_IN");
            }
            ResetEvent(m_ioEvent);
        }
    }

    SoapySDR_log(SOAPY_SDR_INFO, "IC7610: Geraet erfolgreich geoeffnet");
}

void IC7610Device::ftClose()
{
    if (m_ftHandle == nullptr)
    {
        return;
    }

    SoapySDR_log(SOAPY_SDR_INFO, "IC7610: Schliesse Geraet...");

    FT_AbortPipe(m_ftHandle, PIPE_IQ_IN);
    FT_AbortPipe(m_ftHandle, PIPE_CMD_IN);
    FT_AbortPipe(m_ftHandle, PIPE_CMD_OUT);
    FT_Close(m_ftHandle);
    m_ftHandle = nullptr;

    if (m_ioEvent)
    {
        CloseHandle(m_ioEvent);
        m_ioEvent = nullptr;
    }

    SoapySDR_log(SOAPY_SDR_INFO, "IC7610: Geraet geschlossen");
}

// ---------------------------------------------------------------------------
// Hardware-Info
// ---------------------------------------------------------------------------

SoapySDR::Kwargs IC7610Device::getHardwareInfo() const
{
    SoapySDR::Kwargs info;
    info["serial"] = m_serial;
    info["product"] = "IC-7610 SuperSpeed-FIFO Bridge";
    info["vendor"] = "Icom Inc.";
    info["ftdi_chip"] = "FT601";
    info["usb_version"] = "USB 3.1 Gen1";
    return info;
}

// ---------------------------------------------------------------------------
// Kanaele
// ---------------------------------------------------------------------------

size_t IC7610Device::getNumChannels(const int dir) const
{
    return (dir == SOAPY_SDR_RX) ? 1 : 0;
}

bool IC7610Device::getFullDuplex(const int /*dir*/, const size_t /*ch*/) const
{
    return false;
}

// ---------------------------------------------------------------------------
// Stream-API
// ---------------------------------------------------------------------------

SoapySDR::Stream* IC7610Device::setupStream(
    const int dir,
    const std::string& format,
    const std::vector<size_t>&/*channels*/,
    const SoapySDR::Kwargs&/*args*/)
{
    SoapySDR_logf(SOAPY_SDR_DEBUG, "IC7610: >> setupStream dir=%d format=%s", dir, format.c_str());
    if (dir != SOAPY_SDR_RX)
        throw std::runtime_error("IC7610: Nur RX unterstuetzt");

    if (format != SOAPY_SDR_CS16 && format != SOAPY_SDR_CF32)
        throw std::runtime_error("IC7610: Format " + format + " nicht unterstuetzt (CS16 oder CF32)");

    return reinterpret_cast<SoapySDR::Stream*>(
        format == SOAPY_SDR_CF32 ? 2 : 1
        );
}

void IC7610Device::closeStream(SoapySDR::Stream*/*stream*/)
{
    // Ressourcen werden in deactivateStream / Destruktor freigegeben
}

int IC7610Device::activateStream(
    SoapySDR::Stream*/*stream*/,
    const int /*flags*/,
    const long long /*timeNs*/,
    const size_t /*numElems*/)
{
    if (m_streaming.load())
    {
        SoapySDR_log(SOAPY_SDR_WARNING, "IC7610: activateStream() - Stream bereits aktiv");
        return SOAPY_SDR_STREAM_ERROR;
    }

    // FTDI-Handle jetzt oeffnen (Lazy Open)
    try {
        ftOpen();
    }
    catch (const std::exception& e) {
        SoapySDR_logf(SOAPY_SDR_ERROR, "IC7610: ftOpen fehlgeschlagen: %s", e.what());
        return SOAPY_SDR_NOT_SUPPORTED;
    }

    // Frequenz: gecachten Wert verwenden (civGetFrequency waere hier
    // unzuverlaessig wegen moeglicher Pipe-Zustandsprobleme)

    SoapySDR_log(SOAPY_SDR_INFO, "IC7610: I/Q Stream aktivieren...");

    // I/Q einschalten MIT Bestaetigung (civWriteRead ist hier erlaubt,
    // Stream ist noch nicht aktiv also kein Konflikt)
    auto iqResp = civWriteRead({ 0x1A, 0x0B, 0x01 });

    if (!civIsOk(iqResp))
        SoapySDR_log(SOAPY_SDR_WARNING, "IC7610: IQ enable nicht bestaetigt");
    else
        SoapySDR_log(SOAPY_SDR_INFO, "IC7610: IQ enable bestaetigt");

    // Wartezeit: Radio braucht etwas Zeit um IQ-Stream zu starten
    Sleep(500);

    m_streaming = true;
    SoapySDR_log(SOAPY_SDR_INFO, "IC7610: Stream aktiv");
    return 0;
}

int IC7610Device::deactivateStream(
    SoapySDR::Stream*/*stream*/,
    const int /*flags*/,
    const long long /*timeNs*/)
{
    if (!m_streaming.load())
        return 0;

    SoapySDR_log(SOAPY_SDR_INFO, "IC7610: I/Q Stream deaktivieren...");

    // I/Q ausschalten (fire-and-forget)
    civWrite({ 0x1A, 0x0B, 0x00 });

    m_streaming = false;

    // FTDI-Handle schliessen (Lazy Close)
    ftClose();

    SoapySDR_log(SOAPY_SDR_INFO, "IC7610: Stream gestoppt");
    return 0;
}

int IC7610Device::readStream(
    SoapySDR::Stream* stream,
    void* const* buffs,
    const size_t numElems,
    int& flags,
    long long& timeNs,
    const long timeoutUs)
{
    flags = 0;
    timeNs = 0;

    if (!m_streaming.load())
        return SOAPY_SDR_NOT_SUPPORTED;
    const bool   convertToFloat = (reinterpret_cast<intptr_t>(stream) == 2);
    const size_t bytesToRead = numElems * 4;  // CS16: 4 Bytes/Sample

    std::vector<uint8_t> raw(bytesToRead);
    ULONG transferred = 0;

    DWORD timeoutMs = static_cast<DWORD>(timeoutUs / 1000);
    if (timeoutMs == 0) timeoutMs = 100;

    FT_STATUS status = ftReadPipe(
        PIPE_IQ_IN,
        raw.data(),
        static_cast<ULONG>(bytesToRead),
        &transferred,
        timeoutMs
    );

    if (status != FT_OK && status != 24)
    {
        SoapySDR_logf(SOAPY_SDR_ERROR, "IC7610: ReadPipe IQ fehlgeschlagen: %d", status);
        return SOAPY_SDR_TIMEOUT;
    }

    if (transferred == 0)
    {        // NICHT FT_AbortPipe hier aufrufen! AbortPipe wuerde den naechsten
        // Read-Versuch zerstoeren. Einfach TIMEOUT zurueckgeben, CubicSDR
        // wird readStream erneut aufrufen.
        return SOAPY_SDR_TIMEOUT;
    }

    const size_t samplesRead = transferred / 4;
    if (!convertToFloat)
    {
        std::memcpy(buffs[0], raw.data(), transferred);
    }
    else
    {
        // CS16 -> CF32: int16 normalisiert auf +/-1.0
        const int16_t* src = reinterpret_cast<const int16_t*>(raw.data());
        float* dst = reinterpret_cast<float*>(buffs[0]);
        for (size_t i = 0; i < samplesRead * 2; ++i)
            dst[i] = static_cast<float>(src[i]) / 32768.0f;
    }

    return static_cast<int>(samplesRead);
}

std::string IC7610Device::getNativeStreamFormat(
    const int /*dir*/, const size_t /*ch*/, double& fullScale) const
{
    fullScale = 32768.0;
    return SOAPY_SDR_CS16;
}

std::vector<std::string> IC7610Device::getStreamFormats(
    const int /*dir*/, const size_t /*ch*/) const
{
    return { SOAPY_SDR_CS16, SOAPY_SDR_CF32 };
}

size_t IC7610Device::getStreamMTU(SoapySDR::Stream*/*stream*/) const
{
    return BLOCK_SIZE / 4;  // 16384 Samples pro Block
}

// ---------------------------------------------------------------------------
// Frequenz
// ---------------------------------------------------------------------------

void IC7610Device::setFrequency(
    const int /*dir*/, const size_t /*ch*/,
    const std::string& name,
    const double freq,
    const SoapySDR::Kwargs&/*args*/)
{
    SoapySDR_logf(SOAPY_SDR_DEBUG, "IC7610: setFrequency %.0f Hz", freq);

    if (freq < FREQ_MIN_HZ || freq > FREQ_MAX_HZ)
    {
        SoapySDR_logf(SOAPY_SDR_WARNING,
            "IC7610: Frequenz %.0f Hz ausserhalb Bereich", freq);
        return;
    }

    m_frequency = freq;

    if (m_ftHandle != nullptr)
        civSetFrequency(freq);
}

double IC7610Device::getFrequency(
    const int /*dir*/, const size_t /*ch*/,
    const std::string&/*name*/) const
{
    return m_frequency;
}

SoapySDR::RangeList IC7610Device::getFrequencyRange(
    const int /*dir*/, const size_t /*ch*/,
    const std::string&/*name*/) const
{
    return { SoapySDR::Range(FREQ_MIN_HZ, FREQ_MAX_HZ) };
}

// ---------------------------------------------------------------------------
// Samplerate
// ---------------------------------------------------------------------------

void IC7610Device::setSampleRate(
    const int /*dir*/, const size_t /*ch*/, const double rate)
{
    bool valid = false;
    for (double r : VALID_SAMPLE_RATES)
        if (std::abs(r - rate) < 1.0) { valid = true; break; }

    if (!valid)
    {
        SoapySDR_logf(SOAPY_SDR_WARNING,
            "IC7610: Samplerate %.0f nicht unterstuetzt. Via HDSDR setzen.", rate);
        return;
    }

    m_sampleRate = rate;
    SoapySDR_logf(SOAPY_SDR_INFO, "IC7610: Samplerate = %.0f Hz", rate);
}

double IC7610Device::getSampleRate(
    const int /*dir*/, const size_t /*ch*/) const
{
    return m_sampleRate;
}

std::vector<double> IC7610Device::listSampleRates(
    const int /*dir*/, const size_t /*ch*/) const
{
    return std::vector<double>(
        std::begin(VALID_SAMPLE_RATES),
        std::end(VALID_SAMPLE_RATES)
    );
}

SoapySDR::RangeList IC7610Device::getSampleRateRange(
    const int /*dir*/, const size_t /*ch*/) const
{
    SoapySDR::RangeList ranges;
    for (double r : VALID_SAMPLE_RATES)
        ranges.push_back(SoapySDR::Range(r, r));
    return ranges;
}

// ---------------------------------------------------------------------------
// Antenne
// ---------------------------------------------------------------------------

std::vector<std::string> IC7610Device::listAntennas(
    const int /*dir*/, const size_t /*ch*/) const
{
    return { "ANT1", "ANT2" };
}

void IC7610Device::setAntenna(
    const int /*dir*/, const size_t /*ch*/, const std::string& name)
{
    m_antenna = name;
}

std::string IC7610Device::getAntenna(
    const int /*dir*/, const size_t /*ch*/) const
{
    return m_antenna;
}

// ---------------------------------------------------------------------------
// CI-V Hilfsfunktionen (privat)
// ---------------------------------------------------------------------------

std::vector<uint8_t> IC7610Device::buildCiv(
    const std::vector<uint8_t>& cmdBytes) const
{
    std::vector<uint8_t> frame = {
        CIV_PREAMBLE_1, CIV_PREAMBLE_2,
        CIV_TO_IC7610, CIV_FROM_PC
    };
    frame.insert(frame.end(), cmdBytes.begin(), cmdBytes.end());
    frame.push_back(CIV_END);

    while (frame.size() % 4 != 0)
        frame.push_back(CIV_PAD);

    return frame;
}

void IC7610Device::civWrite(const std::vector<uint8_t>& cmdBytes) const
{
    auto frame = buildCiv(cmdBytes);
    ftWritePipe(PIPE_CMD_OUT, frame.data(), static_cast<ULONG>(frame.size()));
    // fire-and-forget: keine Antwort lesen!
}

std::vector<uint8_t> IC7610Device::civWriteRead(
    const std::vector<uint8_t>& cmdBytes) const
{
    // NUR vor Stream-Start aufrufen!
    civWrite(cmdBytes);
    std::vector<uint8_t> response(64, 0);
    ULONG transferred = 0;
    ftReadPipe(PIPE_CMD_IN, response.data(),
        static_cast<ULONG>(response.size()),
        &transferred, IO_TIMEOUT_MS);
    response.resize(transferred);
    return response;
}

bool IC7610Device::civIsOk(const std::vector<uint8_t>& resp) const
{
    if (resp.size() < 6) return false;
    return resp[4] == CIV_OK;
}

void IC7610Device::setIqOutput(bool enable) const
{
    civWrite({ 0x1A, 0x0B, enable ? uint8_t(0x01) : uint8_t(0x00) });
}

void IC7610Device::civSetFrequency(double freqHz) const
{
    auto bcd = encodeBcdFrequency(freqHz);
    std::vector<uint8_t> cmd = { 0x25, 0x00 };
    cmd.insert(cmd.end(), bcd.begin(), bcd.end());
    civWrite(cmd);
}

double IC7610Device::civGetFrequency() const
{
    // NUR vor Stream-Start aufrufen!
    auto resp = civWriteRead({ 0x25, 0x00 });
    // Antwort: FE FE E0 98 25 00 <5 Bytes BCD> FD
    if (resp.size() >= 11 && resp[4] == 0x25 && resp[5] == 0x00)
        return decodeBcdFrequency(&resp[6], 5);

    SoapySDR_log(SOAPY_SDR_WARNING, "IC7610: Ungueltige Frequenz-Antwort, nutze Default");
    return 14200000.0;  // Fallback: 14,2 MHz
}

// ---------------------------------------------------------------------------
// BCD Frequenz-Kodierung / Dekodierung
// ---------------------------------------------------------------------------

std::vector<uint8_t> IC7610Device::encodeBcdFrequency(double freqHz)
{
    uint64_t freq = static_cast<uint64_t>(freqHz);
    std::string digits = std::to_string(freq);
    while (digits.size() < 10) digits = "0" + digits;

    std::vector<uint8_t> result;
    for (int i = 0; i < 10; i += 2)
    {
        int low = digits[9 - i] - '0';
        int high = digits[8 - i] - '0';
        result.push_back(static_cast<uint8_t>((high << 4) | low));
    }
    return result;
}

double IC7610Device::decodeBcdFrequency(const uint8_t* data, size_t len)
{
    double freq = 0.0;
    double multiplier = 1.0;
    for (size_t i = 0; i < len && i < 5; ++i)
    {
        freq += (data[i] & 0x0F) * multiplier;  multiplier *= 10.0;
        freq += ((data[i] >> 4) & 0x0F) * multiplier;  multiplier *= 10.0;
    }
    return freq;
}

// ---------------------------------------------------------------------------
// FTDI Overlapped I/O (privat)
// ---------------------------------------------------------------------------

DWORD IC7610Device::ftWritePipe(
    UCHAR pipe, const void* buf, ULONG len) const
{
    OVERLAPPED ov = {};
    ov.hEvent = m_ioEvent;
    ResetEvent(m_ioEvent);

    ULONG sent = 0;
    FT_STATUS status = FT_WritePipe(
        m_ftHandle, pipe,
        const_cast<PUCHAR>(static_cast<const UCHAR*>(buf)),
        len, &sent, &ov
    );

    if (status == 24 /*FT_IO_PENDING*/)
    {
        WaitForSingleObject(m_ioEvent, IO_TIMEOUT_MS);
        GetOverlappedResult(m_ftHandle, &ov, &sent, FALSE);
        ResetEvent(m_ioEvent);
        return FT_OK;
    }
    return status;
}

DWORD IC7610Device::ftReadPipe(
    UCHAR pipe, void* buf, ULONG len,
    ULONG* transferred, DWORD timeoutMs) const
{
    OVERLAPPED ov = {};
    ov.hEvent = m_ioEvent;
    ResetEvent(m_ioEvent);

    *transferred = 0;
    FT_STATUS status = FT_ReadPipe(
        m_ftHandle, pipe,
        static_cast<PUCHAR>(buf),
        len, transferred, &ov
    );

    if (status == 24 /*FT_IO_PENDING*/)
    {
        DWORD wait = WaitForSingleObject(m_ioEvent, timeoutMs);
        if (wait == WAIT_TIMEOUT)
        {
            FT_AbortPipe(m_ftHandle, pipe);
            return 19 /*FT_TIMEOUT*/;
        }
        GetOverlappedResult(m_ftHandle, &ov, transferred, FALSE);
        ResetEvent(m_ioEvent);
        return FT_OK;
    }
    return status;
}

// ---------------------------------------------------------------------------
// SoapySDR Plugin-Registrierung
// ---------------------------------------------------------------------------

static std::vector<SoapySDR::Kwargs> findIC7610(
    const SoapySDR::Kwargs& args)
{
    std::vector<SoapySDR::Kwargs> results;

    SoapySDR::Kwargs devArgs;
    devArgs["driver"] = "IC7610";
    devArgs["label"] = "Icom IC-7610";
    devArgs["serial"] = "23001774";
    devArgs["product"] = "IC-7610 SuperSpeed-FIFO Bridge";

    if (args.count("serial") && args.at("serial") != devArgs["serial"])
        return results;

    results.push_back(devArgs);
    return results;
}

static SoapySDR::Device* makeIC7610(const SoapySDR::Kwargs& args)
{
    return new IC7610Device(args);
}

// Statische Registry – einmal beim DLL-Load
// KRITISCH: /INCLUDE:SoapyIC7610_loader in CMakeLists.txt verhindert
// dass MSVC diese Variable wegoptimiert.
static SoapySDR::Registry registerIC7610(
    "IC7610",
    &findIC7610,
    &makeIC7610,
    SOAPY_SDR_ABI_VERSION
);

extern "C" __declspec(dllexport) void SoapyIC7610_loader() {}