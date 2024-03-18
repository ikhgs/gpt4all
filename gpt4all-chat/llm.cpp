#include "llm.h"
#include "../gpt4all-backend/llmodel.h"
#include "../gpt4all-backend/sysinfo.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QResource>
#include <QSettings>
#include <QUrl>
#include <QNetworkInformation>
#include <fstream>

#ifndef GPT4ALL_OFFLINE_INSTALLER
#include "network.h"
#endif

class MyLLM: public LLM { };
Q_GLOBAL_STATIC(MyLLM, llmInstance)
LLM *LLM::globalInstance()
{
    return llmInstance();
}

LLM::LLM()
    : QObject{nullptr}
    , m_compatHardware(LLModel::Implementation::cpuSupportsAVX())
{
    QNetworkInformation::loadDefaultBackend();
    auto * netinfo = QNetworkInformation::instance();
    if (netinfo) {
        connect(netinfo, &QNetworkInformation::reachabilityChanged,
            this, &LLM::isNetworkOnlineChanged);
    }
}

bool LLM::hasSettingsAccess() const
{
    QSettings settings;
    settings.sync();
    return settings.status() == QSettings::NoError;
}

bool LLM::checkForUpdates() const
{
    #ifdef GPT4ALL_OFFLINE_INSTALLER
    #pragma message "offline installer build will not check for updates!"
    return QDesktopServices::openUrl(QUrl("https://gpt4all.io/"));
    #else
    Network::globalInstance()->sendCheckForUpdates();

#if defined(Q_OS_LINUX)
    QString tool("maintenancetool");
#elif defined(Q_OS_WINDOWS)
    QString tool("maintenancetool.exe");
#elif defined(Q_OS_DARWIN)
    QString tool("../../../maintenancetool.app/Contents/MacOS/maintenancetool");
#endif

    QString fileName = QCoreApplication::applicationDirPath()
        + "/../" + tool;
    if (!QFileInfo::exists(fileName)) {
        qDebug() << "Couldn't find tool at" << fileName << "so cannot check for updates!";
        return false;
    }

    return QProcess::startDetached(fileName);
    #endif
}

bool LLM::directoryExists(const QString &path)
{
    const QUrl url(path);
    const QString localFilePath = url.isLocalFile() ? url.toLocalFile() : path;
    const QFileInfo info(localFilePath);
    return info.exists() && info.isDir();
}

bool LLM::fileExists(const QString &path)
{
    const QUrl url(path);
    const QString localFilePath = url.isLocalFile() ? url.toLocalFile() : path;
    const QFileInfo info(localFilePath);
    return info.exists() && info.isFile();
}

qint64 LLM::systemTotalRAMInGB() const
{
    return getSystemTotalRAMInGB();
}

QString LLM::systemTotalRAMInGBString() const
{
    return QString::fromStdString(getSystemTotalRAMInGBString());
}

bool LLM::isNetworkOnline() const
{
    auto * netinfo = QNetworkInformation::instance();
    return !netinfo || netinfo->reachability() == QNetworkInformation::Reachability::Online;
}
