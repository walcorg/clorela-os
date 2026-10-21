// SPDX-License-Identifier: MIT
#include "graphview.h"

#include <QEvent>
#include <QMap>
#include <QPainter>
#include <QPainterPath>

using clorela::Measurement;

GraphView::GraphView(QWidget *parent)
    : QWidget(parent)
{
    setAutoFillBackground(true);
}

void GraphView::setMeasurements(const QList<Measurement> &data)
{
    struct Acc { double tSum = 0; int tN = 0; double dSum = 0; int dN = 0; };
    QMap<qint64, Acc> byCycle;
    for (const Measurement &m : data) {
        Acc &a = byCycle[m.cycleId];
        if (m.kind == Measurement::Kind::Led) {
            a.tSum += m.meanTransmission();
            ++a.tN;
        } else if (m.kind == Measurement::Kind::Dark) {
            a.dSum += m.meanTransmission();
            ++a.dN;
        }
    }
    m_points.clear();
    for (auto it = byCycle.cbegin(); it != byCycle.cend(); ++it) {
        Point p;
        p.cycle = it.key();
        p.transmission = it->tN ? it->tSum / it->tN : 0;
        p.dark = it->dN ? it->dSum / it->dN : 0;
        m_points.append(p);
    }
    update();
}

void GraphView::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
        update();
    QWidget::changeEvent(event);
}

void GraphView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QPalette pal = palette();

    const QRect area = rect().adjusted(64, 48, -24, -48);
    p.setPen(pal.color(QPalette::WindowText));
    if (m_points.isEmpty()) {
        p.drawText(area, Qt::AlignCenter, tr("No measurements yet."));
        return;
    }

    QFont titleFont = font();
    titleFont.setBold(true);
    p.setFont(titleFont);
    p.drawText(QRect(0, 8, width(), 32), Qt::AlignCenter,
               tr("Mean transmission (sensors T0-T7), last %n cycle(s)", nullptr, int(m_points.size())));
    p.setFont(font());

    // Axes. Values are 10-bit ADC counts (0-1023).
    const double yMax = 1023.0;
    p.setPen(pal.color(QPalette::Mid));
    p.drawRect(area);
    for (int v = 0; v <= 1000; v += 250) {
        const int y = area.bottom() - int(v / yMax * area.height());
        p.drawLine(area.left() - 4, y, area.left(), y);
        p.drawText(QRect(0, y - 8, area.left() - 8, 16), Qt::AlignRight | Qt::AlignVCenter, QString::number(v));
    }

    auto xFor = [&](int i) {
        if (m_points.size() == 1)
            return double(area.center().x());
        return area.left() + double(i) / (m_points.size() - 1) * area.width();
    };
    auto yFor = [&](double v) { return area.bottom() - qBound(0.0, v, yMax) / yMax * area.height(); };

    auto drawSeries = [&](const QColor &color, auto valueOf) {
        QPainterPath path;
        for (int i = 0; i < m_points.size(); ++i) {
            const QPointF pt(xFor(i), yFor(valueOf(m_points.at(i))));
            if (i == 0)
                path.moveTo(pt);
            else
                path.lineTo(pt);
        }
        p.setPen(QPen(color, 2));
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
        p.setBrush(color);
        for (int i = 0; i < m_points.size(); ++i)
            p.drawEllipse(QPointF(xFor(i), yFor(valueOf(m_points.at(i)))), 3, 3);
    };

    const QColor transColor(0x2e, 0x9e, 0x44); // chlorophyll green
    const QColor darkColor(0x80, 0x80, 0x80);
    drawSeries(transColor, [](const Point &pt) { return pt.transmission; });
    drawSeries(darkColor, [](const Point &pt) { return pt.dark; });

    // Legend and x-axis label (cycle numbers at both ends).
    p.setPen(pal.color(QPalette::WindowText));
    const int ly = area.bottom() + 24;
    p.fillRect(QRect(area.left(), ly - 6, 12, 12), transColor);
    p.drawText(area.left() + 18, ly + 5, tr("LEDs on"));
    p.fillRect(QRect(area.left() + 120, ly - 6, 12, 12), darkColor);
    p.drawText(area.left() + 138, ly + 5, tr("Dark baseline"));
    p.drawText(QRect(area.left(), area.bottom() + 2, area.width(), 16), Qt::AlignLeft,
               tr("cycle %1").arg(m_points.first().cycle));
    p.drawText(QRect(area.left(), area.bottom() + 2, area.width(), 16), Qt::AlignRight,
               tr("cycle %1").arg(m_points.last().cycle));
}
