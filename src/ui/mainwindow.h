// SPDX-License-Identifier: MIT
#pragma once

#include "config.h"
#include "measurementstore.h"

#include <QList>
#include <QMainWindow>
#include <QTranslator>
#include <memory>

namespace Ui { class MainWindow; }

class GraphView;
class NumericView;
class QAction;
class QLabel;
class QSettings;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const clorela::Config &config, QWidget *parent = nullptr);
    ~MainWindow() override;

    enum class View { Graph, Numeric };
    void setView(View view);
    View view() const;

    // "en", "es" or "fr". Unknown codes fall back to English.
    void setLanguage(const QString &code);
    QString language() const { return m_language; }

    // Restricted mode: actions that need the network are disabled.
    void setOnline(bool online);
    bool isOnline() const { return m_online; }
    QList<QAction *> networkActions() const { return m_networkActions; }

public slots:
    void refreshData();

protected:
    void changeEvent(QEvent *event) override;

private slots:
    void checkNetwork();
    void showNotImplemented();
    void showAboutClorela();
    void showAboutWalcorg();

private:
    void updateStatusBar();
    void saveState(const QString &key, const QString &value);

    clorela::Config m_config;
    std::unique_ptr<Ui::MainWindow> ui;
    GraphView *m_graph = nullptr;
    NumericView *m_numeric = nullptr;
    QLabel *m_networkLabel = nullptr;
    QTranslator m_translator;
    QString m_language = QStringLiteral("en");
    bool m_online = true;
    bool m_onlineKnown = false;
    QList<QAction *> m_networkActions;
    clorela::MeasurementStore m_store;
    std::unique_ptr<QSettings> m_state;
};
