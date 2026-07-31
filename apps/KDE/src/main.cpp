/*
 * main.cpp -- application entry point.
 *
 * Standard KDE application bootstrap: QApplication, KLocalizedString domain,
 * KAboutData (which is also what gives KXmlGuiWindow its component name, and so
 * decides where ihaveissuesui.rc is looked up), then the command line.
 */
#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QString>
#include <QStringList>

#include <KAboutData>
#include <KLocalizedString>

#include "github/SyncWorker.h"
#include "ui/MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // VERIFY: KF6 declares this as setApplicationDomain(const QByteArray &).
    // A string literal converts implicitly, so this line compiles against both
    // the KF5 (const char *) and KF6 signatures.
    KLocalizedString::setApplicationDomain("ihaveissues");

    KAboutData about(QStringLiteral("ihaveissues"),
                     i18n("I Have Issues"),
                     QStringLiteral("1.0"),
                     i18n("A document-based issue tracker for small projects."),
                     KAboutLicense::GPL_V3,
                     i18n("(c) 2026 Druware Software Designs"));
    about.addAuthor(i18n("Druware Software Designs"),
                    i18n("Author"),
                    QStringLiteral("dru@openbcm.com"));
    about.setDesktopFileName(QStringLiteral("com.druware.IHaveIssues"));
    KAboutData::setApplicationData(about);

    // The application icon falls back to a stock theme icon: this port ships no
    // icon of its own, exactly as the Haiku port ships no HVIF icon.
    QApplication::setWindowIcon(
        QIcon::fromTheme(QStringLiteral("com.druware.IHaveIssues"),
                         QIcon::fromTheme(QStringLiteral("tools-report-bug"))));

    QCommandLineParser parser;
    about.setupCommandLine(&parser);
    parser.addPositionalArgument(QStringLiteral("file"),
                                 i18n("A .issues document to open."),
                                 QStringLiteral("[file]"));
    parser.process(app);
    about.processCommandLine(&parser);

    ihaveissues::registerSyncMetaTypes();

    // KMainWindow sets Qt::WA_DeleteOnClose on itself, so the window owns its own
    // lifetime and closing it ends the application.
    auto *window = new ihaveissues::MainWindow();

    const QStringList positional = parser.positionalArguments();
    if (!positional.isEmpty()) {
        window->openDocument(positional.first());
    }

    window->show();
    return app.exec();
}
