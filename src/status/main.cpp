// SPDX-License-Identifier: MIT
// clorela-status: writes one message to the shield status display.
//
//   clorela-status BOOTING                 # what clorela-boot-status.service runs
//   clorela-status SAMPLING "CYCLE 12"
//   clorela-status --pbm preview.pbm STERILIZING "UV 300 S"   # desktop preview
#include "config.h"
#include "framebuffer.h"
#include "statusdisplay.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QTextStream>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("clorela-status"));
    QCoreApplication::setApplicationVersion(QStringLiteral(CLORELA_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Show a message on the Clorela-OS status display."));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption configOpt(QStringLiteral("config"), QStringLiteral("Configuration file."), QStringLiteral("path"));
    const QCommandLineOption pbmOpt(QStringLiteral("pbm"), QStringLiteral("Write the frame to a PBM file instead of the display."),
                                    QStringLiteral("file"));
    parser.addOptions({configOpt, pbmOpt});
    parser.addPositionalArgument(QStringLiteral("title"), QStringLiteral("Main message, e.g. BOOTING."));
    parser.addPositionalArgument(QStringLiteral("detail"), QStringLiteral("Optional second line."), QStringLiteral("[detail]"));
    parser.process(app);

    const QStringList args = parser.positionalArguments();
    if (args.isEmpty())
        parser.showHelp(1);
    const QString title = args.at(0);
    const QString detail = args.value(1);

    if (parser.isSet(pbmOpt)) {
        clorela::Framebuffer fb;
        fb.renderStatus(title, detail);
        QFile f(parser.value(pbmOpt));
        if (!f.open(QIODevice::WriteOnly) || f.write(fb.toPbm()) < 0) {
            QTextStream(stderr) << "cannot write " << f.fileName() << Qt::endl;
            return 1;
        }
        return 0;
    }

    const clorela::Config config = clorela::Config::load(parser.value(configOpt));
    auto display = clorela::createStatusDisplay(config);
    if (!display->show(title, detail)) {
        QTextStream(stderr) << "clorela-status: " << display->name() << ": " << display->lastError() << Qt::endl;
        return 1;
    }
    return 0;
}
