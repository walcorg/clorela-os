// SPDX-License-Identifier: MIT
// Status display on the shield: short messages such as BOOTING, SAMPLING or
// STERILIZING. Backends: SSD1306 OLED over I2C, console (log) or none.
#pragma once

#include "framebuffer.h"

#include <QString>
#include <memory>

namespace clorela {

struct Config;
class I2cDevice;

class StatusDisplay
{
public:
    virtual ~StatusDisplay() = default;
    virtual QString name() const = 0;
    // Returns false if the message could not be shown (the caller only logs it).
    virtual bool show(const QString &title, const QString &detail = QString()) = 0;
    QString lastError() const { return m_lastError; }

protected:
    QString m_lastError;
};

class NullStatusDisplay : public StatusDisplay
{
public:
    QString name() const override { return QStringLiteral("none"); }
    bool show(const QString &, const QString &) override { return true; }
};

class ConsoleStatusDisplay : public StatusDisplay
{
public:
    QString name() const override { return QStringLiteral("console"); }
    bool show(const QString &title, const QString &detail) override;
};

// 0.96" 128x64 module with an SSD1306 controller (the common blue/cyan
// "IIC" breakout). Address 0x3C or 0x3D, selected on the module [TO VERIFY].
class Ssd1306StatusDisplay : public StatusDisplay
{
public:
    Ssd1306StatusDisplay(int bus, int address);
    ~Ssd1306StatusDisplay() override;
    QString name() const override { return QStringLiteral("ssd1306"); }
    bool show(const QString &title, const QString &detail) override;
    const Framebuffer &framebuffer() const { return m_fb; }

private:
    bool init();
    bool command(const QByteArray &bytes);
    bool flush();

    std::unique_ptr<I2cDevice> m_i2c;
    int m_address;
    bool m_initialized = false;
    QString m_lastTitle, m_lastDetail;
    Framebuffer m_fb;
};

std::unique_ptr<StatusDisplay> createStatusDisplay(const Config &config);

} // namespace clorela
