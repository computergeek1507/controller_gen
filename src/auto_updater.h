#pragma once

#include <QObject>

#include <QNetworkAccessManager>
#include <QString>
#include <QProgressDialog>

#include "spdlog/spdlog.h"

struct VersionInfo 
{
    QString Version;
    QString Changes;
    QString Date;
    QString UpdateUrl;
    //bool isPrerelease = false;
};

class AutoUpdater : public QObject
{
    Q_OBJECT

public: 
    AutoUpdater(QObject* parent = nullptr);
    ~AutoUpdater() = default;
    void checkForUpdates(bool showNoUpdateMessage = false);

    void downloadUpdateFile(const QString& url);

public Q_SLOTS:
    void onUpdateCheckFinished();
    void onUpdateDownloadFinished();
    void onUpdateError(int code, const QString& message);
    void progressUpdate(qint64 bytesReceived, qint64 bytesTotal);

    void runInstaller(const QString& path);

Q_SIGNALS:
    void updateCheckFinished(bool updateAvailable, const VersionInfo& info);
    void updateDownloadFinished(const QString& filePath);
    void updateError(int code, const QString& message);
private:

    QString getVersionFromName(const QString& name);

    std::tuple<int,int,int> parseVersionString(const QString& versionStr);

    QNetworkAccessManager _networkManager;
    std::shared_ptr<spdlog::logger> m_logger{ nullptr };
    QProgressDialog* m_progress{ nullptr };
};