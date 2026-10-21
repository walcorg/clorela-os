// SPDX-License-Identifier: MIT
// clorela-acqd: acquisition daemon of the Clorela-OS node.
//
//   clorela-acqd                              # hardware, /etc/clorela/clorela.conf
//   clorela-acqd --config data/dev.conf       # simulated node on a PC
//   clorela-acqd --sensor sim                 # override acquisition/sensor
#include "acquisitionservice.h"
#include "config.h"
#include "controlserver.h"
#include "i2cnodetransport.h"
#include "simulatednode.h"
#include "statusdisplay.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QSocketNotifier>
#include <QTextStream>
#include <QThread>

#include <csignal>
#include <sys/socket.h>
#include <unistd.h>

namespace {

int g_signalFd[2] = {-1, -1};

void onSignal(int)
{
    const char c = 1;
    [[maybe_unused]] auto n = ::write(g_signalFd[0], &c, 1); // async-signal-safe
}

void log(const QString &message)
{
    QTextStream(stderr) << message << Qt::endl; // journald adds the timestamp
}

// Title and detail for the shield display. Short and upper case: the OLED fits
// 11 large characters per line.
std::pair<QString, QString> displayText(const clorela::ServiceSnapshot &s)
{
    using State = clorela::ServiceSnapshot::State;
    switch (s.state) {
    case State::Idle: return {QStringLiteral("READY"), s.online ? QStringLiteral("WI-FI OK") : QStringLiteral("NO WI-FI")};
    case State::Waiting:
    case State::Sampling: return {QStringLiteral("SAMPLING"), s.lastCycleId ? QStringLiteral("CYCLE %1").arg(s.lastCycleId) : QString()};
    case State::WaitingNetwork: return {QStringLiteral("NO WI-FI"), QStringLiteral("WAITING")};
    case State::Sterilizing: return {QStringLiteral("STERILIZING"), QStringLiteral("UV %1 S").arg(s.uvSecondsRemaining)};
    case State::Error: return {QStringLiteral("NODE ERROR"), s.detail.left(21)};
    }
    return {QString(), QString()};
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("clorela-acqd"));
    QCoreApplication::setApplicationVersion(QStringLiteral(CLORELA_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Clorela-OS acquisition daemon"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption configOpt(QStringLiteral("config"), QStringLiteral("Configuration file."), QStringLiteral("path"));
    const QCommandLineOption sensorOpt(QStringLiteral("sensor"), QStringLiteral("i2c or sim (overrides acquisition/sensor)."),
                                       QStringLiteral("name"));
    parser.addOptions({configOpt, sensorOpt});
    parser.process(app);

    clorela::Config config = clorela::Config::load(parser.value(configOpt));
    if (parser.isSet(sensorOpt))
        config.sensor = parser.value(sensorOpt).toLower();

    std::unique_ptr<clorela::NodeTransport> transport;
    if (config.sensor == QLatin1String("sim"))
        transport = std::make_unique<clorela::SimulatedNodeBus>(config.nodeAddresses);
    else if (config.sensor == QLatin1String("i2c"))
        transport = std::make_unique<clorela::I2cNodeTransport>(config.i2cBus);
    else {
        log(QStringLiteral("unknown sensor '%1' (use i2c or sim)").arg(config.sensor));
        return 2;
    }
    log(QStringLiteral("clorela-acqd %1, config %2, sensor %3, database %4")
            .arg(QStringLiteral(CLORELA_VERSION),
                 config.sourcePath.isEmpty() ? QStringLiteral("(defaults)") : config.sourcePath,
                 transport->name(), config.databasePath));

    // SIGTERM/SIGINT -> clean shutdown (LEDs off, database closed).
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, g_signalFd) == 0) {
        std::signal(SIGTERM, onSignal);
        std::signal(SIGINT, onSignal);
        auto *notifier = new QSocketNotifier(g_signalFd[1], QSocketNotifier::Read, &app);
        QObject::connect(notifier, &QSocketNotifier::activated, &app, [] {
            char c;
            [[maybe_unused]] auto n = ::read(g_signalFd[1], &c, 1);
            QCoreApplication::quit();
        });
    }

    auto display = clorela::createStatusDisplay(config);

    QThread worker;
    worker.setObjectName(QStringLiteral("acquisition"));
    auto *service = new clorela::AcquisitionService(config, std::move(transport));
    service->moveToThread(&worker);
    QObject::connect(&worker, &QThread::finished, service, &QObject::deleteLater);

    clorela::ControlServer control(service, config);
    QObject::connect(service, &clorela::AcquisitionService::snapshotChanged, &control,
                     &clorela::ControlServer::setSnapshot);
    QObject::connect(service, &clorela::AcquisitionService::logMessage, &app, [](const QString &m) { log(m); });
    QObject::connect(service, &clorela::AcquisitionService::snapshotChanged, &app,
                     [&display, last = std::pair<QString, QString>()](const clorela::ServiceSnapshot &s) mutable {
                         const auto text = displayText(s);
                         if (text == last)
                             return; // snapshots arrive every second; redraw only on change
                         last = text;
                         if (!display->show(text.first, text.second))
                             log(QStringLiteral("status display (%1): %2").arg(display->name(), display->lastError()));
                     });

    if (!control.listen(config.controlSocket)) {
        log(QStringLiteral("cannot listen on %1: %2").arg(config.controlSocket, control.errorString()));
        return 1;
    }

    worker.start();
    QMetaObject::invokeMethod(service, &clorela::AcquisitionService::initialize, Qt::QueuedConnection);

    const int rc = app.exec();
    log(QStringLiteral("shutting down"));
    QMetaObject::invokeMethod(service, &clorela::AcquisitionService::shutdown, Qt::BlockingQueuedConnection);
    worker.quit();
    worker.wait();
    display->show(QStringLiteral("STOPPED"), QString());
    return rc;
}
