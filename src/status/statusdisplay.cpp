// SPDX-License-Identifier: MIT
#include "statusdisplay.h"

#include "config.h"
#include "i2cdevice.h"

#include <QTextStream>

namespace clorela {

bool ConsoleStatusDisplay::show(const QString &title, const QString &detail)
{
    QTextStream err(stderr);
    err << "[status] " << title;
    if (!detail.isEmpty())
        err << " | " << detail;
    err << Qt::endl;
    return true;
}

namespace {
// SSD1306 control bytes.
constexpr char kCommandStream = 0x00;
constexpr char kDataStream = 0x40;
constexpr int kDataChunk = 32; // bytes of pixel data per I2C transaction
} // namespace

Ssd1306StatusDisplay::Ssd1306StatusDisplay(int bus, int address)
    : m_i2c(std::make_unique<I2cDevice>(bus))
    , m_address(address)
{
}

Ssd1306StatusDisplay::~Ssd1306StatusDisplay() = default;

bool Ssd1306StatusDisplay::command(const QByteArray &bytes)
{
    QByteArray frame(1, kCommandStream);
    frame += bytes;
    if (!m_i2c->write(m_address, frame)) {
        m_lastError = m_i2c->lastError();
        return false;
    }
    return true;
}

bool Ssd1306StatusDisplay::init()
{
    // Standard 128x64 initialisation with the internal charge pump.
    static const char seq[] = {
        char(0xAE),             // display off
        char(0xD5), char(0x80), // clock divide
        char(0xA8), char(0x3F), // multiplex 64
        char(0xD3), char(0x00), // display offset 0
        char(0x40),             // start line 0
        char(0x8D), char(0x14), // charge pump on
        char(0x20), char(0x00), // horizontal addressing
        char(0xA1),             // segment remap (column 127 = SEG0)
        char(0xC8),             // COM scan descending
        char(0xDA), char(0x12), // COM pins for 128x64
        char(0x81), char(0xCF), // contrast
        char(0xD9), char(0xF1), // pre-charge
        char(0xDB), char(0x40), // VCOMH
        char(0xA4),             // resume from RAM
        char(0xA6),             // normal (not inverted)
        char(0xAF),             // display on
    };
    m_initialized = command(QByteArray(seq, sizeof(seq)));
    return m_initialized;
}

bool Ssd1306StatusDisplay::flush()
{
    // Whole frame: columns 0-127, pages 0-7.
    if (!command(QByteArray("\x21\x00\x7F\x22\x00\x07", 6)))
        return false;
    const QByteArray &d = m_fb.data();
    for (int i = 0; i < d.size(); i += kDataChunk) {
        QByteArray frame(1, kDataStream);
        frame += d.mid(i, kDataChunk);
        if (!m_i2c->write(m_address, frame)) {
            m_lastError = m_i2c->lastError();
            return false;
        }
    }
    return true;
}

bool Ssd1306StatusDisplay::show(const QString &title, const QString &detail)
{
    if (m_initialized && title == m_lastTitle && detail == m_lastDetail)
        return true; // nothing changed: spare the bus
    m_fb.renderStatus(title, detail);
    if (!m_initialized && !init())
        return false;
    if (!flush()) {
        m_initialized = false; // re-initialise on the next attempt (module unplugged?)
        return false;
    }
    m_lastTitle = title;
    m_lastDetail = detail;
    return true;
}

std::unique_ptr<StatusDisplay> createStatusDisplay(const Config &config)
{
    if (config.displayBackend == QLatin1String("ssd1306"))
        return std::make_unique<Ssd1306StatusDisplay>(config.displayI2cBus, config.displayAddress);
    if (config.displayBackend == QLatin1String("console"))
        return std::make_unique<ConsoleStatusDisplay>();
    return std::make_unique<NullStatusDisplay>();
}

} // namespace clorela
