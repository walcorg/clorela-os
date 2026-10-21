// SPDX-License-Identifier: MIT
#include "config.h"
#include "mainwindow.h"

#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QTemporaryDir>
#include <QTest>

namespace {

QStringList titles(const QList<QAction *> &actions)
{
    QStringList out;
    for (QAction *a : actions)
        if (!a->isSeparator())
            out << a->text().remove(QLatin1Char('&'));
    return out;
}

QMenu *topMenu(MainWindow &w, int index)
{
    return w.menuBar()->actions().at(index)->menu();
}

} // namespace

class TstMainWindow : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    clorela::Config config()
    {
        clorela::Config c;
        c.databasePath = m_dir.filePath(QStringLiteral("m.db"));
        c.uiStatePath = m_dir.filePath(QStringLiteral("ui.conf"));
        c.language = QStringLiteral("en");
        return c;
    }

private slots:
    void init() { qputenv("CLORELA_NETWORK", "online"); }

    void menuTreeMatchesV14()
    {
        MainWindow w(config());
        QCOMPARE(titles(w.menuBar()->actions()),
                 (QStringList{"Account", "View", "Configure", "Options", "Help"}));
        QCOMPARE(titles(topMenu(w, 0)->actions()),
                 (QStringList{"Log in", "My profile", "Start acquisition", "Stop acquisition", "Log out"}));
        QCOMPARE(titles(topMenu(w, 0)->actions().at(1)->menu()->actions()),
                 (QStringList{"User data", "Wallet address"}));
        QCOMPARE(titles(topMenu(w, 1)->actions()), (QStringList{"Display", "Language"}));
        QCOMPARE(titles(topMenu(w, 2)->actions()), (QStringList{"Configure Wi-Fi", "Configure server"}));
        QCOMPARE(titles(topMenu(w, 3)->actions()), (QStringList{"Check for updates", "Connection properties"}));
        QCOMPARE(titles(topMenu(w, 4)->actions()),
                 (QStringList{"About Clorela-OS", "About WALCORG", "Report a problem"}));
    }

    void shortcuts()
    {
        MainWindow w(config());
        const QList<QAction *> all = w.findChildren<QAction *>();
        int found = 0;
        for (QAction *a : all) {
            if (a->objectName() == QLatin1String("actionStartAcquisition")) {
                QCOMPARE(a->shortcut(), QKeySequence(Qt::Key_F1));
                ++found;
            } else if (a->objectName() == QLatin1String("actionStopAcquisition")) {
                QCOMPARE(a->shortcut(), QKeySequence(Qt::Key_F5));
                ++found;
            }
        }
        QCOMPARE(found, 2);
    }

    void languageSwitch()
    {
        MainWindow w(config());
        w.setLanguage(QStringLiteral("es"));
        QCOMPARE(w.language(), QStringLiteral("es"));
        QCOMPARE(titles(w.menuBar()->actions()),
                 (QStringList{"Cuenta", "Vista", "Configurar", "Opciones", "Ayuda"}));
        w.setLanguage(QStringLiteral("fr"));
        QCOMPARE(titles(w.menuBar()->actions()),
                 (QStringList{"Compte", "Affichage", "Configurer", "Options", "Aide"}));
        w.setLanguage(QStringLiteral("xx"));
        QCOMPARE(w.language(), QStringLiteral("en"));
        QCOMPARE(titles(w.menuBar()->actions()).first(), QStringLiteral("Account"));
    }

    void languageIsRemembered()
    {
        {
            MainWindow w(config());
            w.setLanguage(QStringLiteral("fr"));
        }
        MainWindow again(config());
        QCOMPARE(again.language(), QStringLiteral("fr"));
        again.setLanguage(QStringLiteral("en"));
    }

    void restrictedModeOffline()
    {
        qputenv("CLORELA_NETWORK", "offline");
        MainWindow w(config());
        QVERIFY(!w.isOnline());
        QVERIFY(!w.networkActions().isEmpty());
        for (QAction *a : w.networkActions())
            QVERIFY2(!a->isEnabled(), qPrintable(a->objectName()));
        // Browsing and getting online stay available.
        for (QAction *a : w.findChildren<QAction *>()) {
            const QString n = a->objectName();
            if (n == QLatin1String("actionConfigureWifi") || n == QLatin1String("actionGraph")
                || n == QLatin1String("actionNumeric") || n == QLatin1String("actionSpanish"))
                QVERIFY2(a->isEnabled(), qPrintable(n));
        }
        w.setOnline(true);
        for (QAction *a : w.networkActions())
            QVERIFY(a->isEnabled());
    }

    void viewSwitch()
    {
        MainWindow w(config());
        w.setView(MainWindow::View::Numeric);
        QCOMPARE(w.view(), MainWindow::View::Numeric);
        w.setView(MainWindow::View::Graph);
        QCOMPARE(w.view(), MainWindow::View::Graph);
    }
};

QTEST_MAIN(TstMainWindow)
#include "tst_mainwindow.moc"
