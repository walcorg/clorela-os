// SPDX-License-Identifier: MIT
#include "framebuffer.h"
#include "font5x7.h"

namespace clorela {

namespace {
// Glyphs are magnified but the gap between them stays 1 px, so 11 large
// characters (e.g. STERILIZING) fit in 128 px.
int advance(int scale)
{
    return kFontWidth * scale + 1;
}

const uint8_t *glyph(QChar c)
{
    char ch = c.unicode() < 0x80 ? char(c.unicode()) : '?';
    if (ch < kFontFirst || ch > kFontLast)
        ch = '?';
    return kFont5x7[ch - kFontFirst];
}
} // namespace

Framebuffer::Framebuffer()
    : m_data(kWidth * kHeight / 8, '\0')
{
}

void Framebuffer::clear()
{
    m_data.fill('\0');
}

void Framebuffer::setPixel(int x, int y, bool on)
{
    if (x < 0 || y < 0 || x >= kWidth || y >= kHeight)
        return;
    const int index = (y / 8) * kWidth + x;
    const char bit = char(1 << (y % 8));
    if (on)
        m_data[index] = char(m_data[index] | bit);
    else
        m_data[index] = char(m_data[index] & ~bit);
}

bool Framebuffer::pixel(int x, int y) const
{
    if (x < 0 || y < 0 || x >= kWidth || y >= kHeight)
        return false;
    return m_data.at((y / 8) * kWidth + x) & (1 << (y % 8));
}

int Framebuffer::textWidth(const QString &text, int scale)
{
    if (text.isEmpty())
        return 0;
    return int(text.size()) * advance(scale) - 1; // no gap after the last glyph
}

int Framebuffer::drawText(int x, int y, const QString &text, int scale)
{
    int cx = x;
    for (QChar c : text) {
        const uint8_t *g = glyph(c);
        for (int col = 0; col < kFontWidth; ++col) {
            for (int row = 0; row < kFontHeight; ++row) {
                if (!(g[col] & (1 << row)))
                    continue;
                for (int dx = 0; dx < scale; ++dx)
                    for (int dy = 0; dy < scale; ++dy)
                        setPixel(cx + col * scale + dx, y + row * scale + dy);
            }
        }
        cx += advance(scale);
    }
    return cx - x;
}

void Framebuffer::drawTextCentered(int y, const QString &text, int scale)
{
    const int w = textWidth(text, scale);
    drawText(qMax(0, (kWidth - w) / 2), y, text, scale);
}

void Framebuffer::renderStatus(const QString &title, const QString &detail)
{
    clear();
    drawTextCentered(2, QStringLiteral("CLORELA-OS"));
    for (int x = 0; x < kWidth; ++x)
        setPixel(x, 12);
    const QString t = title.toUpper();
    if (textWidth(t, 2) <= kWidth)
        drawTextCentered(24, t, 2);
    else
        drawTextCentered(28, t, 1);
    drawTextCentered(52, detail.toUpper());
}

QByteArray Framebuffer::toPbm() const
{
    QByteArray out = "P1\n128 64\n";
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x)
            out += pixel(x, y) ? '1' : '0';
        out += '\n';
    }
    return out;
}

int Framebuffer::litPixels() const
{
    int n = 0;
    for (char b : m_data)
        for (int i = 0; i < 8; ++i)
            n += (b >> i) & 1;
    return n;
}

} // namespace clorela
