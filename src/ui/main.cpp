// SPDX-License-Identifier: MIT
// clorela-ui: interface of the Clorela-OS node.
//
// On the node it runs full screen on the HDMI output (QT_QPA_PLATFORM=linuxfb,
// set by the systemd unit). On a desktop: clorela-ui --config data/dev.conf --windowed
#include "config.h"
#include "mainwindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("clorela-ui"));
    QApplication::setApplicationVersion(QStringLiteral(CLORELA_VERSION));
    QApplication::setOrganizationName(QStringLiteral("WALCORG"));
    QApplication::setOrganizationDomain(QStringLiteral("walcorg.com"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Clorela-OS node interface"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption configOpt(QStringLiteral("config"),
                                       QStringLiteral("Configuration file (default: $CLORELA_CONFIG or /etc/clorela/clorela.conf)."),
                                       QStringLiteral("path"));
    const QCommandLineOption fullscreenOpt(QStringLiteral("fullscreen"), QStringLiteral("Full screen (overrides ui/fullscreen)."));
    const QCommandLineOption windowedOpt(QStringLiteral("windowed"), QStringLiteral("Window mode (overrides ui/fullscreen)."));
    const QCommandLineOption languageOpt(QStringLiteral("language"), QStringLiteral("en, es or fr (this session only)."),
                                         QStringLiteral("code"));
    const QCommandLineOption viewOpt(QStringLiteral("view"), QStringLiteral("graph or numeric."), QStringLiteral("name"));
    const QCommandLineOption screenshotOpt(QStringLiteral("screenshot"),
                                           QStringLiteral("Save a PNG of the window after start-up and exit (for docs and tests)."),
                                           QStringLiteral("file"));
    parser.addOptions({configOpt, fullscreenOpt, windowedOpt, languageOpt, viewOpt, screenshotOpt});
    parser.process(app);

    const clorela::Config config = clorela::Config::load(parser.value(configOpt));

    MainWindow window(config);
    if (parser.isSet(languageOpt))
        window.setLanguage(parser.value(languageOpt));
    if (parser.isSet(viewOpt))
        window.setView(parser.value(viewOpt) == QLatin1String("numeric") ? MainWindow::View::Numeric
                                                                          : MainWindow::View::Graph);

    bool fullscreen = config.fullscreen;
    if (parser.isSet(fullscreenOpt))
        fullscreen = true;
    if (parser.isSet(windowedOpt))
        fullscreen = false;

    if (parser.isSet(screenshotOpt)) {
        window.resize(1024, 600);
        window.show();
        const QString file = parser.value(screenshotOpt);
        QTimer::singleShot(300, &window, [&window, file] {
            window.grab().save(file);
            QApplication::quit();
        });
    } else if (fullscreen) {
        window.showFullScreen();
    } else {
        window.show();
    }
    return app.exec();
}
