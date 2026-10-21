// SPDX-License-Identifier: MIT
// 1-bit framebuffer in SSD1306 layout (8 pages of 8 rows; one byte per column
// per page, bit 0 = top row of the page). Kept free of I/O so it can be tested
// and previewed on a desktop.
#pragma once

#include <QByteArray>
#include <QString>

namespace clorela {

class Framebuffer
{
public:
    static constexpr int kWidth = 128;
    static constexpr int kHeight = 64;

    Framebuffer();
    void clear();
    void setPixel(int x, int y, bool on = true);
    bool pixel(int x, int y) const;

    // Draws ASCII text (5x7 font) with glyphs magnified `scale` times and a
    // 1 px gap between them.
    // Characters outside 0x20-0x7E are drawn as '?'. Returns the width used.
    int drawText(int x, int y, const QString &text, int scale = 1);
    static int textWidth(const QString &text, int scale = 1);
    // Centred horizontally; text that does not fit is cut at the right edge.
    void drawTextCentered(int y, const QString &text, int scale = 1);

    // Status screen used by the daemon: small header, big title, small detail.
    // The title is drawn at scale 2 if it fits (<= 11 characters), else scale 1.
    void renderStatus(const QString &title, const QString &detail);

    const QByteArray &data() const { return m_data; }
    // Plain PBM (P1) image, for previews: clorela-status --pbm out.pbm TEXT
    QByteArray toPbm() const;
    int litPixels() const;

private:
    QByteArray m_data;
};

} // namespace clorela
