// SPDX-License-Identifier: MIT
// "Graph" view: trend of the last cycles, drawn with QPainter (no Qt Charts,
// to keep the image small on the Pi Zero W).
#pragma once

#include "measurement.h"

#include <QList>
#include <QWidget>

class GraphView : public QWidget
{
    Q_OBJECT

public:
    explicit GraphView(QWidget *parent = nullptr);

    // Readings of several cycles, oldest first.
    void setMeasurements(const QList<clorela::Measurement> &data);

    struct Point
    {
        qint64 cycle = 0;
        double transmission = 0; // mean of T0..T7 over all single-LED readings
        double dark = 0;         // mean of T0..T7 in the darkness check
    };
    const QList<Point> &points() const { return m_points; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    QList<Point> m_points;
};
