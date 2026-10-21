// SPDX-License-Identifier: MIT
// "Numeric" view: the 16 sensor values of every reading in the latest cycle.
#pragma once

#include "measurement.h"

#include <QList>
#include <QWidget>

class QLabel;
class QTableWidget;

class NumericView : public QWidget
{
    Q_OBJECT

public:
    explicit NumericView(QWidget *parent = nullptr);
    void setMeasurements(const QList<clorela::Measurement> &latestCycle);

protected:
    void changeEvent(QEvent *event) override;

private:
    void retranslate();

    QLabel *m_header = nullptr;
    QTableWidget *m_table = nullptr;
    QList<clorela::Measurement> m_data;
};
