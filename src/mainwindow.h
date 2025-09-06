#pragma once

#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSettings>
#include <QFileInfoList>

#include "spdlog/spdlog.h"
#include "spdlog/common.h"

#include <memory>
#include <atomic>

#include "FSEQFile.h"

namespace Ui {
class MainWindow;
}

struct Controller;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:

    void on_actionOpen_xLights_Controller_File_triggered();
    void on_actionSet_Folder_triggered();
    void on_actionExit_triggered();
    void on_actionOpen_Log_triggered();
    void on_actionAbout_triggered();
    void on_actionAbout_QT_triggered();

    void on_pushButtonExport_clicked();
    void on_pushButtonExportAll_clicked();
    void on_pushButtonRefresh_clicked();
    void on_comboBoxController_currentIndexChanged(int);
    void on_checkBoxSparse_stateChanged(int);
private:
    Ui::MainWindow* m_ui;
    QNetworkAccessManager* m_manager;

    std::shared_ptr<spdlog::logger> m_logger{ nullptr };
    std::unique_ptr<QSettings> m_settings{ nullptr };
    QString m_appdir;

    QString m_fseqFolder;

    std::vector<Controller> m_controllers;

    void exportFSEQFile(std::string const& in_path, std::string const& out_path, 
        int major_ver, int minor_ver, V2FSEQFile::CompressionType, 
        std::vector<std::pair<uint32_t, uint32_t>> ranges, bool sparse);


    void loadControllerFile(const QString& filename);
    void refreshList(QFileInfoList const& files);
    void searchForFSEQs();
    void searchForUSBs();
};
