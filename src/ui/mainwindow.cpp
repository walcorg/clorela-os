// SPDX-License-Identifier: MIT
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "graphview.h"
#include "networkstatus.h"
#include "numericview.h"

#include <QActionGroup>
#include <QApplication>
#include <QEvent>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QSettings>
#include <QStatusBar>
#include <QTimer>

namespace {
constexpr int kGraphCycles = 60;
constexpr int kRefreshMs = 2000;
constexpr int kNetworkCheckMs = 5000;
} // namespace

MainWindow::MainWindow(const clorela::Config &config, QWidget *parent)
    : QMainWindow(parent)
    , m_config(config)
    , ui(std::make_unique<Ui::MainWindow>())
{
    ui->setupUi(this);

    // Persistent UI choices (language, view) live outside /etc.
    m_state = std::make_unique<QSettings>(m_config.uiStatePath, QSettings::IniFormat);

    m_graph = new GraphView(ui->views);
    m_numeric = new NumericView(ui->views);
    ui->views->addWidget(m_graph);
    ui->views->addWidget(m_numeric);

    auto *viewGroup = new QActionGroup(this);
    viewGroup->addAction(ui->actionGraph);
    viewGroup->addAction(ui->actionNumeric);
    connect(ui->actionGraph, &QAction::triggered, this, [this] { setView(View::Graph); });
    connect(ui->actionNumeric, &QAction::triggered, this, [this] { setView(View::Numeric); });

    auto *langGroup = new QActionGroup(this);
    langGroup->addAction(ui->actionEnglish);
    langGroup->addAction(ui->actionSpanish);
    langGroup->addAction(ui->actionFrench);
    connect(ui->actionEnglish, &QAction::triggered, this, [this] { setLanguage(QStringLiteral("en")); });
    connect(ui->actionSpanish, &QAction::triggered, this, [this] { setLanguage(QStringLiteral("es")); });
    connect(ui->actionFrench, &QAction::triggered, this, [this] { setLanguage(QStringLiteral("fr")); });

    // Items with no window behind them yet (see README, TODO list).
    const QList<QAction *> pending = {
        ui->actionLogIn, ui->actionLogOut, ui->actionUserData, ui->actionWalletAddress,
        ui->actionStartAcquisition, ui->actionStopAcquisition,
        ui->actionConfigureWifi, ui->actionConfigureServer,
        ui->actionCheckUpdates, ui->actionConnectionProperties, ui->actionReportProblem,
    };
    for (QAction *a : pending)
        connect(a, &QAction::triggered, this, &MainWindow::showNotImplemented);
    connect(ui->actionAboutClorela, &QAction::triggered, this, &MainWindow::showAboutClorela);
    connect(ui->actionAboutWalcorg, &QAction::triggered, this, &MainWindow::showAboutWalcorg);

    // Restricted mode without network: Clorela-OS still boots and the
    // interface can be browsed, but nothing that talks to the server, and no
    // acquisition (it requires Wi-Fi), is available. Configuring Wi-Fi stays
    // enabled because it is how the node gets online.
    m_networkActions = {
        ui->actionLogIn, ui->actionLogOut, ui->actionUserData, ui->actionWalletAddress,
        ui->actionStartAcquisition, ui->actionCheckUpdates, ui->actionReportProblem,
    };

    m_networkLabel = new QLabel(this);
    statusBar()->addPermanentWidget(m_networkLabel);

    const QString savedView = m_state->value(QStringLiteral("view"), QStringLiteral("graph")).toString();
    setView(savedView == QLatin1String("numeric") ? View::Numeric : View::Graph);
    setLanguage(m_state->value(QStringLiteral("language"), m_config.language).toString());

    if (!m_store.open(m_config.databasePath))
        statusBar()->showMessage(tr("Cannot open the measurement database: %1").arg(m_store.lastError()));

    auto *refresh = new QTimer(this);
    connect(refresh, &QTimer::timeout, this, &MainWindow::refreshData);
    refresh->start(kRefreshMs);

    auto *net = new QTimer(this);
    connect(net, &QTimer::timeout, this, &MainWindow::checkNetwork);
    net->start(kNetworkCheckMs);

    checkNetwork();
    refreshData();
}

MainWindow::~MainWindow() = default;

void MainWindow::setView(View v)
{
    if (v == View::Numeric) {
        ui->views->setCurrentWidget(m_numeric);
        ui->actionNumeric->setChecked(true);
    } else {
        ui->views->setCurrentWidget(m_graph);
        ui->actionGraph->setChecked(true);
    }
    saveState(QStringLiteral("view"), v == View::Numeric ? QStringLiteral("numeric") : QStringLiteral("graph"));
}

MainWindow::View MainWindow::view() const
{
    return ui->views->currentWidget() == m_numeric ? View::Numeric : View::Graph;
}

void MainWindow::setLanguage(const QString &requested)
{
    QString code = requested.toLower();
    if (code != QLatin1String("es") && code != QLatin1String("fr"))
        code = QStringLiteral("en");

    qApp->removeTranslator(&m_translator);
    if (code != QLatin1String("en")) {
        if (m_translator.load(QStringLiteral(":/i18n/clorela_%1.qm").arg(code)))
            qApp->installTranslator(&m_translator);
        else
            code = QStringLiteral("en");
    }
    m_language = code;

    // QEvent::LanguageChange also reaches the widgets, but asynchronously;
    // retranslate the menus now so the change is visible immediately.
    ui->retranslateUi(this);
    ui->actionEnglish->setChecked(code == QLatin1String("en"));
    ui->actionSpanish->setChecked(code == QLatin1String("es"));
    ui->actionFrench->setChecked(code == QLatin1String("fr"));
    saveState(QStringLiteral("language"), code);
    updateStatusBar();
}

void MainWindow::setOnline(bool online)
{
    if (m_onlineKnown && online == m_online)
        return;
    m_onlineKnown = true;
    m_online = online;
    for (QAction *a : std::as_const(m_networkActions))
        a->setEnabled(online);
    updateStatusBar();
}

void MainWindow::checkNetwork()
{
    setOnline(clorela::NetworkStatus::isOnline());
}

void MainWindow::refreshData()
{
    if (!m_store.isOpen())
        return;
    m_graph->setMeasurements(m_store.recentCycles(kGraphCycles));
    m_numeric->setMeasurements(m_store.latestCycle());
}

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
        updateStatusBar();
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::updateStatusBar()
{
    if (!m_networkLabel)
        return;
    m_networkLabel->setText(m_online ? tr("Online") : tr("Offline: restricted mode"));
}

void MainWindow::saveState(const QString &key, const QString &value)
{
    if (m_state)
        m_state->setValue(key, value);
}

void MainWindow::showNotImplemented()
{
    auto *action = qobject_cast<QAction *>(sender());
    const QString what = action ? action->text().remove(QLatin1Char('&')) : QString();
    QMessageBox::information(this, what, tr("This function is not available yet."));
}

void MainWindow::showAboutClorela()
{
    QMessageBox::about(this, tr("About Clorela-OS"),
                       tr("<b>Clorela-OS %1</b><br>"
                          "Embedded system of the WALCORG photobioreactor node "
                          "(Raspberry Pi Zero W + shield).<br><br>"
                          "Open source under the MIT license.")
                           .arg(QStringLiteral(CLORELA_VERSION)));
}

void MainWindow::showAboutWalcorg()
{
    QMessageBox::about(this, tr("About WALCORG"),
                       tr("<b>WALCORG TERRAFORMING CORP.</b><br>"
                          "Creating economies based on carbon capture.<br><br>"
                          "walcorg.com"));
}
