// SPDX-License-Identifier: MIT
#include "numericview.h"

#include <QDateTime>
#include <QEvent>
#include <QHeaderView>
#include <QLabel>
#include <QSet>
#include <QTableWidget>
#include <QVBoxLayout>

using clorela::Measurement;

NumericView::NumericView(QWidget *parent)
    : QWidget(parent)
    , m_header(new QLabel(this))
    , m_table(new QTableWidget(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_header);
    layout->addWidget(m_table, 1);

    m_table->setColumnCount(clorela::kSensorCount);
    QStringList cols;
    for (int i = 0; i < 8; ++i)
        cols << QStringLiteral("B%1").arg(i);
    for (int i = 0; i < 8; ++i)
        cols << QStringLiteral("T%1").arg(i);
    m_table->setHorizontalHeaderLabels(cols);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    retranslate();
}

void NumericView::setMeasurements(const QList<Measurement> &latestCycle)
{
    m_data = latestCycle;
    retranslate();
}

void NumericView::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(event);
}

void NumericView::retranslate()
{
    if (m_data.isEmpty()) {
        m_header->setText(tr("No measurements yet."));
        m_table->setRowCount(0);
        return;
    }

    const Measurement &first = m_data.first();
    const QString when = QDateTime::fromMSecsSinceEpoch(first.timestampMs).toLocalTime()
                             .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    m_header->setText(tr("Cycle %1 · %2 · B = backscatter, T = transmission (ADC counts 0-1023)")
                          .arg(first.cycleId)
                          .arg(when));

    QSet<int> nodes;
    for (const Measurement &m : std::as_const(m_data))
        nodes.insert(m.node);
    const bool showNode = nodes.size() > 1;

    m_table->setRowCount(int(m_data.size()));
    QStringList rows;
    for (int r = 0; r < m_data.size(); ++r) {
        const Measurement &m = m_data.at(r);
        QString label;
        switch (m.kind) {
        case Measurement::Kind::Dark: label = tr("Dark"); break;
        case Measurement::Kind::Led: label = tr("LED %1").arg(m.param); break;
        case Measurement::Kind::Byte: label = tr("Byte 0x%1").arg(m.param, 2, 16, QLatin1Char('0')); break;
        case Measurement::Kind::Sample: label = tr("Sample"); break;
        }
        if (showNode)
            label = QStringLiteral("0x%1 · %2").arg(m.node, 2, 16, QLatin1Char('0')).arg(label);
        rows << label;
        for (int c = 0; c < clorela::kSensorCount; ++c) {
            auto *item = new QTableWidgetItem(QString::number(m.values[c]));
            item->setTextAlignment(Qt::AlignCenter);
            m_table->setItem(r, c, item);
        }
    }
    m_table->setVerticalHeaderLabels(rows);
}
