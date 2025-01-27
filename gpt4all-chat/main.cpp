#include "chatlistmodel.h"
#include "config.h"
#include "download.h"
#include "llm.h"
#include "localdocs.h"
#include "logger.h"
#include "modellist.h"
#include "mysettings.h"
#include "network.h"

#include "../gpt4all-backend/llmodel.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQmlEngine>
#include <QSettings>
#include <QString>
#include <QTranslator>
#include <QUrl>
#include <Qt>

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName("nomic.ai");
    QCoreApplication::setOrganizationDomain("gpt4all.io");
    QCoreApplication::setApplicationName("GPT4All");
    QCoreApplication::setApplicationVersion(APP_VERSION);
    QSettings::setDefaultFormat(QSettings::IniFormat);

    Logger::globalInstance();

    QGuiApplication app(argc, argv);

    // Set the local and language translation before the qml engine has even been started. This will
    // use the default system locale unless the user has explicitly set it to use a different one.
    MySettings::globalInstance()->setLanguageAndLocale();

    QQmlApplicationEngine engine;

    // Add a connection here from MySettings::languageAndLocaleChanged signal to a lambda slot where I can call
    // engine.uiLanguage property
    QObject::connect(MySettings::globalInstance(), &MySettings::languageAndLocaleChanged, [&engine]() {
        engine.setUiLanguage(MySettings::globalInstance()->languageAndLocale());
    });

    QString llmodelSearchPaths = QCoreApplication::applicationDirPath();
    const QString libDir = QCoreApplication::applicationDirPath() + "/../lib/";
    if (LLM::directoryExists(libDir))
        llmodelSearchPaths += ";" + libDir;
#if defined(Q_OS_MAC)
    const QString binDir = QCoreApplication::applicationDirPath() + "/../../../";
    if (LLM::directoryExists(binDir))
        llmodelSearchPaths += ";" + binDir;
    const QString frameworksDir = QCoreApplication::applicationDirPath() + "/../Frameworks/";
    if (LLM::directoryExists(frameworksDir))
        llmodelSearchPaths += ";" + frameworksDir;
#endif
    LLModel::Implementation::setImplementationsSearchPath(llmodelSearchPaths.toStdString());

    qmlRegisterSingletonInstance("mysettings", 1, 0, "MySettings", MySettings::globalInstance());
    qmlRegisterSingletonInstance("modellist", 1, 0, "ModelList", ModelList::globalInstance());
    qmlRegisterSingletonInstance("chatlistmodel", 1, 0, "ChatListModel", ChatListModel::globalInstance());
    qmlRegisterSingletonInstance("llm", 1, 0, "LLM", LLM::globalInstance());
    qmlRegisterSingletonInstance("download", 1, 0, "Download", Download::globalInstance());
    qmlRegisterSingletonInstance("network", 1, 0, "Network", Network::globalInstance());
    qmlRegisterSingletonInstance("localdocs", 1, 0, "LocalDocs", LocalDocs::globalInstance());
    const QUrl url(u"qrc:/gpt4all/main.qml"_qs);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
        &app, [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        }, Qt::QueuedConnection);
    engine.load(url);

#if 0
    QDirIterator it("qrc:", QDirIterator::Subdirectories);
    while (it.hasNext()) {
        qDebug() << it.next();
    }
#endif

    int res = app.exec();

    // Make sure ChatLLM threads are joined before global destructors run.
    // Otherwise, we can get a heap-use-after-free inside of llama.cpp.
    ChatListModel::globalInstance()->destroyChats();

    return res;
}
