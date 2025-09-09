#include "auto_updater.h"

#include <QNetworkReply>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QProcess>

#include "config.h"

#include "nlohmann/json.hpp"

AutoUpdater::AutoUpdater(QObject* parent) : QObject(parent)
{
    Q_UNUSED(parent);

    m_logger = spdlog::get(PROJECT_NAME);
}

void AutoUpdater::checkForUpdates(bool showNoUpdateMessage)
{
    QNetworkRequest request(QUrl("https://api.github.com/repos/computergeek1507/controller_gen/releases"));
    //request.setRawHeader("Accept", "application/vnd.github+json");
    QNetworkReply* reply = _networkManager.get(request);
    connect(reply, &QNetworkReply::finished, this, &AutoUpdater::onUpdateCheckFinished);
}

void AutoUpdater::downloadUpdateFile(const QString& url)
{
    QNetworkRequest request(url);
    //QNetworkRequest request2(QUrl(url));
    QNetworkReply* reply = _networkManager.get(request);
    connect(reply, &QNetworkReply::downloadProgress, this, &AutoUpdater::progressUpdate);
    connect(reply, &QNetworkReply::finished, this, &AutoUpdater::onUpdateDownloadFinished);
}

void AutoUpdater::onUpdateCheckFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (reply->error() != QNetworkReply::NoError) {
        m_logger->error("Update check failed: {} - {}", static_cast<int>(reply->error()), reply->errorString().toStdString());
        emit updateError(static_cast<int>(reply->error()), reply->errorString());
        reply->deleteLater();
        return;
    }
    QByteArray responseData = reply->readAll();
    // Parse the JSON response to extract version info
    nlohmann::json json = nlohmann::json::parse(responseData.toStdString(), nullptr, false);

    if (json.is_discarded() || !json.is_array() || json.empty()) {
        m_logger->error("Update check failed: Invalid JSON response");
        emit updateError(-1, "Invalid JSON response");
        reply->deleteLater();
        return;
    }

    for(const auto& release : json) {

        auto name = release.value("name", "");

        if (name.compare("ci_win") == 0) {
            
            auto releaseAsset = release["assets"];
            auto body = release.value("body", "");
            if (releaseAsset.is_array() && !releaseAsset.empty()) {
                for (const auto& asset : releaseAsset) {
                    auto const assetName = asset.value("name", "");
                    VersionInfo info;
                    info.Version = getVersionFromName(assetName.c_str());
                    info.Changes = body.c_str();
                    info.Date = release.value("updated_at", "").c_str();
                    info.UpdateUrl = asset.value("browser_download_url", "").c_str();

                    // Compare with current version
                    auto [cmajor, cminor, cpatch] = parseVersionString(PROJECT_VER);
                    auto [major, minor, patch] = parseVersionString(info.Version);

                    int const currentVersion = (cmajor * 10000) + (cminor * 100) + cpatch;
                    int const newVersion = (major * 10000) + (minor * 100) + patch;
                    m_logger->info("Current Version: {}.{}.{} ({})", cmajor, cminor, cpatch, currentVersion);
                    m_logger->info("Latest Version: {}.{}.{} ({})", major, minor, patch, newVersion);

                    if (newVersion > currentVersion) {
                        emit updateCheckFinished(true, info);
                    } else {
                        emit updateCheckFinished(false, info);
                    }
                    reply->deleteLater();
                    return;
                }
            }
        }
        break;
    }
    emit updateError(-1, "Not Found");
    reply->deleteLater();
}

void AutoUpdater::progressUpdate(qint64 bytesReceived, qint64 bytesTotal)
{
    if (m_progress == nullptr) {
        m_progress = new QProgressDialog("Downloading Update...", "Cancel", 0, static_cast<int>(bytesTotal));
        m_progress->setWindowModality(Qt::WindowModal);
        m_progress->setMinimumDuration(0);
        m_progress->setValue(0);
        m_progress->show();
    }
    if (m_progress) {
        m_progress->setMaximum(static_cast<int>(bytesTotal));
        m_progress->setValue(static_cast<int>(bytesReceived));
        if (m_progress->wasCanceled()) {
            // Handle cancellation
            m_progress->close();
            m_progress = nullptr;
            QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
            if (reply) {
                reply->abort();
                reply->deleteLater();
            }

            emit updateError(-1, "Download canceled by user");
        }
        if (bytesReceived >= bytesTotal) {
            m_progress->close();
            m_progress = nullptr;
        }
    }
}

void AutoUpdater::onUpdateDownloadFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (reply->error() != QNetworkReply::NoError) {
        emit updateError(static_cast<int>(reply->error()), reply->errorString());
        reply->deleteLater();
        return;
    }
    // Save the downloaded file
    QString filePath = QDir::tempPath() + QDir::separator() + "update_installer.exe"; // Change as needed
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(reply->readAll());
        file.close();
        emit updateDownloadFinished(filePath);
        reply->deleteLater();
        runInstaller(filePath);
        return;
    } else {
        emit updateError(-1, "Failed to save the update file");
    }
    reply->deleteLater();
}

void AutoUpdater::onUpdateError(int code, const QString& message)
{
    emit updateError(code, message);
}

void AutoUpdater::runInstaller(const QString& path)
{
#if defined _WIN32
    QProcess::startDetached(path, QStringList(), QString());
#else
    QProcess::startDetached("chmod", QStringList() << "+x" << path);
    QProcess::startDetached(path, QStringList(), QString());
#endif
    QCoreApplication::quit();
}

QString AutoUpdater::getVersionFromName(const QString& name) 
{
    QFileInfo fi(name);
    auto parts = fi.completeBaseName().split('_');
    //auto parts = name.split('_');
    if (parts.size() >= 2) {
        return parts.last(); // Assuming the version is the second part
    }
    return "0.0.0";	
}

std::tuple<int, int, int>  AutoUpdater::parseVersionString(const QString& versionStr)
{
    auto parts = versionStr.split('.');
    int major {0};
    int minor {0};
    int patch {0};
    if (parts.size() > 0){ major = parts[0].toInt();}
    if (parts.size() > 1){ minor = parts[1].toInt();}
    if (parts.size() > 2){ patch = parts[2].toInt();}
    return std::make_tuple(major, minor, patch);
}
