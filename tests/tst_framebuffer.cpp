// SPDX-License-Identifier: MIT
#include "config.h"
#include "framebuffer.h"
#include "statusdisplay.h"

#include <QTest>

using clorela::Framebuffer;

class TstFramebuffer : public QObject
{
    Q_OBJECT

private slots:
    void pixelLayoutMatchesSsd1306()
    {
        Framebuffer fb;
        fb.setPixel(0, 0);
        fb.setPixel(5, 9); // page 1, bit 1
        QCOMPARE(int(uchar(fb.data().at(0))), 0x01);
        QCOMPARE(int(uchar(fb.data().at(128 + 5))), 0x02);
        QVERIFY(fb.pixel(5, 9));
        fb.setPixel(5, 9, false);
        QVERIFY(!fb.pixel(5, 9));
        fb.setPixel(500, 500); // out of range is ignored
        QCOMPARE(fb.data().size(), 1024);
    }

    void glyphA()
    {
        Framebuffer fb;
        fb.drawText(0, 0, QStringLiteral("A"));
        // Column 0 of 'A' is 0x7E: rows 1-6 lit, row 0 dark.
        QVERIFY(!fb.pixel(0, 0));
        for (int y = 1; y <= 6; ++y)
            QVERIFY(fb.pixel(0, y));
        // Column 2 is 0x11: rows 0 and 4.
        QVERIFY(fb.pixel(2, 0));
        QVERIFY(fb.pixel(2, 4));
        QVERIFY(!fb.pixel(2, 2));
    }

    void longestStatusFitsLarge()
    {
        // The user-facing messages must fit at scale 2 (128 px wide).
        for (const char *word : {"BOOTING", "SAMPLING", "STERILIZING", "NO WI-FI", "NODE ERROR", "READY"})
            QVERIFY2(Framebuffer::textWidth(QString::fromLatin1(word), 2) <= Framebuffer::kWidth, word);
        QVERIFY(Framebuffer::textWidth(QStringLiteral("STERILIZING!"), 2) > Framebuffer::kWidth);
    }

    void renderStatus()
    {
        Framebuffer fb;
        fb.renderStatus(QStringLiteral("sampling"), QStringLiteral("cycle 12"));
        QVERIFY(fb.litPixels() > 200);
        // Separator line under the header.
        for (int x = 0; x < Framebuffer::kWidth; ++x)
            QVERIFY(fb.pixel(x, 12));
        const QByteArray pbm = fb.toPbm();
        QVERIFY(pbm.startsWith("P1\n128 64\n"));
        QCOMPARE(pbm.count('\n'), 2 + 64);
    }

    void nonAsciiIsReplaced()
    {
        Framebuffer a, b;
        a.drawText(0, 0, QStringLiteral("é"));
        b.drawText(0, 0, QStringLiteral("?"));
        QCOMPARE(a.data(), b.data());
    }

    void consoleAndNullBackends()
    {
        clorela::Config c;
        c.displayBackend = QStringLiteral("console");
        QCOMPARE(clorela::createStatusDisplay(c)->name(), QStringLiteral("console"));
        c.displayBackend = QStringLiteral("none");
        auto none = clorela::createStatusDisplay(c);
        QCOMPARE(none->name(), QStringLiteral("none"));
        QVERIFY(none->show(QStringLiteral("X"), QString()));
    }

    void ssd1306WithoutHardwareFailsCleanly()
    {
        clorela::Ssd1306StatusDisplay d(250, 0x3C); // /dev/i2c-250 does not exist
        QVERIFY(!d.show(QStringLiteral("BOOTING"), QString()));
        QVERIFY(d.lastError().contains(QStringLiteral("i2c-250")));
        QVERIFY(d.framebuffer().litPixels() > 0); // frame was still rendered
    }
};

QTEST_GUILESS_MAIN(TstFramebuffer)
#include "tst_framebuffer.moc"
