#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "config.h"

#include "controller.h"

#include <QTableWidgetItem>
#include <QSettings>
#include <QTimer>
#include <QNetworkRequest>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrlQuery>
#include <QFileDialog>
#include <QStorageInfo>

#include "spdlog/spdlog.h"

#include "spdlog/sinks/qt_sinks.h"
#include "spdlog/sinks/rotating_file_sink.h"

#include "pugixml.hpp"

#include <iostream>

#include <memory>
#include <filesystem>
#include <utility>
#include <fstream>
#include <sstream>
#include <QProgressDialog>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    m_ui(new Ui::MainWindow)
{
    QCoreApplication::setApplicationName(PROJECT_NAME);
    QCoreApplication::setApplicationVersion(PROJECT_VER);
    m_ui->setupUi(this);

    auto const log_name{ "log.txt" };

    m_appdir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    std::filesystem::create_directory(m_appdir.toStdString());
    QString logdir = m_appdir + "/log/";
    std::filesystem::create_directory(logdir.toStdString());

    try
    {
        auto file{ std::string(logdir.toStdString() + log_name) };
        auto rotating = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(file, 1024 * 1024, 5, false);

        m_logger = std::make_shared<spdlog::logger>("box_design", rotating);
        m_logger->flush_on(spdlog::level::debug);
        m_logger->set_level(spdlog::level::debug);
        m_logger->set_pattern("[%D %H:%M:%S] [%L] %v");
        spdlog::register_logger(m_logger);
    }
    catch (std::exception& /*ex*/)
    {
        QMessageBox::warning(this, "Logger Failed", "Logger Failed To Start.");
    }

    setWindowTitle(windowTitle() + " v" + PROJECT_VER);

    m_settings = std::make_unique< QSettings>(m_appdir + "/settings.txt", QSettings::IniFormat);

    on_checkBoxSparse_stateChanged(0);
    searchForUSBs();
    auto homeDir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    auto fseqFolder = m_settings->value("FSEQFolder", homeDir).toString();
    if (!fseqFolder.isEmpty() && QDir().exists(fseqFolder))
    {
        m_fseqFolder = fseqFolder;
        //m_ui->lineEditFolder->setText(m_fseqFolder);
        searchForFSEQs();
    }
    
}

MainWindow::~MainWindow()
{
    delete m_ui;
}

void MainWindow::on_actionSet_Folder_triggered()
{
    auto homeDir = m_fseqFolder.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::HomeLocation) : m_fseqFolder;
    auto dir = QFileDialog::getExistingDirectory(this, "Select FSEQ Folder", homeDir);

    if (!dir.isEmpty())
    {
        m_fseqFolder = dir;
        //m_ui->lineEditFolder->setText(m_fseqFolder);
        m_settings->setValue("FSEQFolder", m_fseqFolder);
        searchForFSEQs();
    }
}

void MainWindow::on_actionOpen_xLights_Controller_File_triggered()
{
    auto homeDir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    auto fn = QFileDialog::getOpenFileName(this, "Select xLights Controller File", homeDir, "xLights Controller Files (xlights_networks.xml)");

    if (!fn.isEmpty())
    {
        //m_ui->lineEditController->setText(fn);
        m_settings->setValue("xLightsControllerFile", fn);
        loadControllerFile(fn);
    }
}

void MainWindow::on_actionView_FSEQ_Header_triggered()
{
    auto homeDir = m_fseqFolder.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::HomeLocation) : m_fseqFolder;
    auto fn = QFileDialog::getOpenFileName(this, "Select FSEQ File", homeDir, "FSEQ Files (*.fseq *.eseq)");

    if (!fn.isEmpty())
    {
        std::unique_ptr<FSEQFile> src(FSEQFile::openFSEQFile(fn.toStdString()));
        if (nullptr == src) {
            spdlog::critical("Error opening input file: {}", fn.toStdString());
            return;
        }
        QString info;
        info += QString("File: %1\n").arg(src->getFilename().c_str());
        info += QString("Version: %1.%2\n").arg(src->getVersionMajor()).arg(src->getVersionMinor());
        info += QString("Channels: %1\n").arg(src->getChannelCount());
        info += QString("Frames: %1\n").arg(src->getNumFrames());
        info += QString("Step Time: %1 ms\n").arg(src->getStepTime());
        info += QString("Total Time: %1 ms\n").arg(src->getTotalTimeMS());
        if (src->getVersionMajor() == 2 ) {
            V2FSEQFile* f = (V2FSEQFile*)src.get();
            info += QString("Compression: %1\n").arg(f->CompressionTypeString().c_str());
            info += QString("Sparse Ranges:\n");
            for (const auto& a : f->m_sparseRanges) {
                info += QString("Start: %1 Len: %2\n").arg(a.first).arg(a.second);
            }
        }
        QMessageBox::information(this, "FSEQ File Info", info);
    }
}

void MainWindow::on_actionExit_triggered()
{
    close();
}

void MainWindow::on_actionOpen_Log_triggered()
{
    auto logpath = m_appdir + "/log/" + "log.txt";
    if (QFile::exists(logpath))
    {
        QDesktopServices::openUrl(QUrl::fromLocalFile(logpath));
    }
    else
    {
        QMessageBox::warning(this, "No Log File", "No log file found to open.");
    }
}

void MainWindow::on_actionAbout_triggered() 
{    
    QString about_text = QString("<b>Controller FSEQ Gen</b><br>Version: %2<br><br>"
        "A tool to convert xLights FSEQ files to a Specific Controller SD Cards.<br><br>"
        "Developed by Scott</a>.<br><br>"
        "This software is provided 'as-is', without any express or implied warranty. "
        "In no event will the authors be held liable for any damages arising from the use of this software.<br><br>"
        "Licensed under the MIT License. See the LICENSE file for details.")
        
        .arg(PROJECT_VER);
    QMessageBox::about(this, "About", about_text);
}

void MainWindow::on_actionAbout_QT_triggered()
{
    QMessageBox::aboutQt(this, "About Qt");
}

void MainWindow::on_pushButtonRefresh_clicked()
{
    searchForUSBs();
}

void MainWindow::on_pushButtonExport_clicked()
{
    if (m_ui->comboBoxSDCard->currentIndex() < 0) {
        QMessageBox::warning(this, "No SD Card Selected", "Please select an SD Card from the dropdown.");
        return;
    }
    if (m_ui->tableWidgetFSEQs->rowCount() == 0) {
        QMessageBox::warning(this, "No FSEQ Files", "No FSEQ files found to export.");
        return;
    }
    QString sdcardPath = m_ui->comboBoxSDCard->currentData().toString();
    if (sdcardPath.isEmpty()) {
        QMessageBox::warning(this, "Invalid SD Card Path", "The selected SD Card path is invalid.");
        return;
    }

    int startChannel = m_ui->spinBoxStartChannel->value();
    int endChannel = m_ui->spinBoxEndChannel->value();

    bool sparse = m_ui->checkBoxSparse->isChecked();
    std::vector<std::pair<uint32_t, uint32_t>> ranges;
    if (sparse) {
        ranges.push_back(std::pair<uint32_t, uint32_t>(startChannel, endChannel));
    }

    int major_ver = 2;
    int minor_ver = 2;

    auto s_version = m_ui->comboBoxVersion->currentText();
    if (s_version.contains('.')) {
        auto const versions = s_version.split('.');
        if (versions.size() == 2) {
            major_ver = versions[0].toInt();
            minor_ver = versions[1].toInt();
        }
    } else {
        major_ver = s_version.toInt();
    }

    V2FSEQFile::CompressionType compressionType = V2FSEQFile::CompressionType::none;
    if (m_ui->comboBoxCompression->currentIndex() == 0) {
        compressionType = V2FSEQFile::CompressionType::zstd;
    } else if (m_ui->comboBoxCompression->currentIndex() == 1) {
        compressionType = V2FSEQFile::CompressionType::zlib;
    }
    QProgressDialog progress("Exporting FSEQ Files...", "Abort", 0, m_ui->tableWidgetFSEQs->rowCount(), this);
    bool working = true;
    for (int row = 0; row < m_ui->tableWidgetFSEQs->rowCount(); ++row) {
        QTableWidgetItem* checkBoxItem = m_ui->tableWidgetFSEQs->item(row, 0);
        progress.setValue(row);
        if (checkBoxItem && checkBoxItem->checkState() == Qt::Checked) {
            QTableWidgetItem* fileItem = m_ui->tableWidgetFSEQs->item(row, 1);
            if (fileItem) {
                progress.setLabelText(QString("Exporting %1...").arg(fileItem->text()));
                QCoreApplication::processEvents();
                QString filePath = fileItem->toolTip();
                if (!filePath.isEmpty()) {
                    QString outPath = sdcardPath + fileItem->text();
                    m_logger->info("Exporting {} to {}", filePath.toStdString(), outPath.toStdString());
                    working &= exportFSEQFile(filePath.toStdString(), outPath.toStdString(), major_ver, minor_ver, compressionType, ranges, sparse);
                }
            }
        }
        if (progress.wasCanceled()) {
            break;
        }
    }
    if (!working) {
        QMessageBox::warning(this, "Export Error", "One or more FSEQ files failed to export. See log for details.");
        return;
    }
    QMessageBox::information(this, "Export Complete", "FSEQ files have been exported to the SD Card.");
}

void MainWindow::on_pushButtonExportAll_clicked()
{
    if (m_ui->comboBoxSDCard->currentIndex() < 0) {
        QMessageBox::warning(this, "No SD Card Selected", "Please select an SD Card from the dropdown.");
        return;
    }
    if (m_ui->tableWidgetFSEQs->rowCount() == 0) {
        QMessageBox::warning(this, "No FSEQ Files", "No FSEQ files found to export.");
        return;
    }
    QString sdcardPath = m_ui->comboBoxSDCard->currentData().toString();
    if (sdcardPath.isEmpty()) {
        QMessageBox::warning(this, "Invalid SD Card Path", "The selected SD Card path is invalid.");
        return;
    }

    bool sparse = m_ui->checkBoxSparse->isChecked();

    int major_ver = 2;
    int minor_ver = 2;

    auto s_version = m_ui->comboBoxVersion->currentText();
    if (s_version.contains('.')) {
        auto const versions = s_version.split('.');
        if (versions.size() == 2) {
            major_ver = versions[0].toInt();
            minor_ver = versions[1].toInt();
        }
    }
    else
    {
        major_ver = s_version.toInt();
    }

    V2FSEQFile::CompressionType compressionType = V2FSEQFile::CompressionType::none;
    if (m_ui->comboBoxCompression->currentIndex() == 0) {
        compressionType = V2FSEQFile::CompressionType::zstd;
    } else if (m_ui->comboBoxCompression->currentIndex() == 1) {
        compressionType = V2FSEQFile::CompressionType::zlib;
    }
    QProgressDialog progress("Exporting FSEQ Files...", "Abort", 0, m_ui->tableWidgetFSEQs->rowCount() * m_controllers.size(), this);
    bool working = true;
    for(int c = 0; c < m_controllers.size(); ++c) {
        auto const& controller = m_controllers[c];
        std::vector<std::pair<uint32_t, uint32_t>> ranges;
        //if (sparse) {
            ranges.push_back(std::pair<uint32_t, uint32_t>(controller.start_channel, controller.channels));
        //}
        for (int row = 0; row < m_ui->tableWidgetFSEQs->rowCount(); ++row) {
            QTableWidgetItem* checkBoxItem = m_ui->tableWidgetFSEQs->item(row, 0);
            progress.setValue(c * m_ui->tableWidgetFSEQs->rowCount() + row);
            if (checkBoxItem && checkBoxItem->checkState() == Qt::Checked) {
                QTableWidgetItem* fileItem = m_ui->tableWidgetFSEQs->item(row, 1);                
                if (fileItem) {
                    progress.setLabelText(QString("Exporting %1 to %2...").arg(fileItem->text()).arg(controller.name.c_str()));
                    QCoreApplication::processEvents();
                    QString filePath = fileItem->toolTip();
                    if (!filePath.isEmpty()) {
                        QString outPath = sdcardPath + fileItem->text();
                        if (m_controllers.size() > 1) {
                            QDir().mkpath(sdcardPath + QDir::separator() + controller.name.c_str());
                            outPath = sdcardPath + controller.name.c_str() + QDir::separator() + fileItem->text();
                        }
                        m_logger->info("Exporting {} to {}", filePath.toStdString(), outPath.toStdString());
                        working &= exportFSEQFile(filePath.toStdString(), outPath.toStdString(), major_ver, minor_ver, compressionType, ranges, sparse);
                    }
                }
            }
            if (progress.wasCanceled()) {
                break;
            }
        }
        if (progress.wasCanceled()) {
            break;
        }
    }
    if (!working) {
        QMessageBox::warning(this, "Export Error", "One or more FSEQ files failed to export. See log for details.");
        return;
    }
    QMessageBox::information(this, "Export Complete", "FSEQ files have been exported to the SD Card.");
}

void MainWindow::on_comboBoxController_currentIndexChanged(int)
{
    int idx = m_ui->comboBoxController->currentIndex();
    if (idx < 0 || idx >= static_cast<int>(m_controllers.size())) {
        return;
    }
    auto const& controller = m_controllers[idx];

    m_ui->spinBoxStartChannel->setValue(controller.start_channel);
    m_ui->spinBoxEndChannel->setValue(controller.channels );
    //m_logger->info("Selected Controller: {} at {} with {} channels starting at {}", controller.name, controller.ip, controller.totalChannels, controller.startChannel);
}

void MainWindow::on_checkBoxSparse_stateChanged(int)
{
    if (m_ui->checkBoxSparse->isChecked()) {
        m_ui->spinBoxStartChannel->setEnabled(true);
        m_ui->spinBoxEndChannel->setEnabled(true);
    } else {
        m_ui->spinBoxStartChannel->setEnabled(false);
        m_ui->spinBoxEndChannel->setEnabled(false);
    }
}

void MainWindow::refreshList(QFileInfoList const& files)
{
    m_ui->tableWidgetFSEQs->clearContents();
    
    m_ui->tableWidgetFSEQs->setRowCount(0);
    m_ui->tableWidgetFSEQs->setRowCount(files.size());
    int row = 0;

    for (const QFileInfo& fileInfo : files) {
        QTableWidgetItem* checkBoxItem = new QTableWidgetItem();
        checkBoxItem->setFlags(checkBoxItem->flags() | Qt::ItemIsUserCheckable);
        checkBoxItem->setCheckState(Qt::Checked);
        m_ui->tableWidgetFSEQs->setItem(row, 0, checkBoxItem);
        //m_ui->tableWidgetFSEQs->setItem(row, 0, new QTableWidgetItem());
        //m_ui->tableWidgetFSEQs->item(row, 0)->setText(fileInfo.fileName());
        //m_ui->tableWidgetFSEQs->item(row, 0)->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        m_ui->tableWidgetFSEQs->setItem(row, 1, new QTableWidgetItem());
        m_ui->tableWidgetFSEQs->item(row, 1)->setText(fileInfo.fileName());
        m_ui->tableWidgetFSEQs->item(row, 1)->setToolTip(fileInfo.absoluteFilePath());
        m_ui->tableWidgetFSEQs->item(row, 1)->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        m_ui->tableWidgetFSEQs->setItem(row, 2, new QTableWidgetItem());
        m_ui->tableWidgetFSEQs->item(row, 2)->setText(fileInfo.lastModified().toString(Qt::ISODate));
        m_ui->tableWidgetFSEQs->item(row, 2)->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        row++;
    }
    m_ui->tableWidgetFSEQs->resizeColumnsToContents();
    m_ui->tableWidgetFSEQs->horizontalHeader()->setStretchLastSection(true);
}

void MainWindow::loadControllerFile(const QString& filename)
{
    m_logger->info("Loading xLights Controller File: {}", filename.toStdString());
    m_ui->comboBoxController->clear();
    m_controllers.clear();
    pugi::xml_document doc;

    pugi::xml_parse_result result = doc.load_file(filename.toStdString().c_str());
    pugi::xml_node networks = doc.child("Networks");
    if(!networks) {
        m_logger->error("No Networks node found in the controller file.");
        return;
    }
    uint64_t startChannel{ 1 };
    for (pugi::xml_node controller = networks.child("Controller"); controller; controller = controller.next_sibling("Controller")) {
        auto name = controller.attribute("Name").value();
        auto ip = controller.attribute("IP").value();
        //std::cout << "Tool " << tool.attribute("Name").value() << "\n";
            //std::cout << "Tool " << tool.attribute("IP").value() << "\n";
        int totalChannels = {0};
        for (pugi::xml_node network = controller.child("network"); network; network = network.next_sibling("network")) {
            int size = network.attribute("MaxChannels").as_int();
            totalChannels += size;
            //std::cout << "Tool " << tool.attribute("Name").value() << "\n";
            //std::cout << "Tool " << tool.attribute("IP").value() << "\n";;
        }
        if(totalChannels != 0) {
            m_logger->info("Found Controller: {} at {} with {} channels starting at {}", name, ip, totalChannels, startChannel);
            m_controllers.emplace_back(name, ip, startChannel, totalChannels);
            m_ui->comboBoxController->addItem(QString("%1 (%2)").arg(name).arg(ip));
        } else {
            m_logger->warn("Found Controller: {} at {} with 0 channels, skipping", name, ip);
        }
        startChannel += totalChannels;
    }
}

void MainWindow::searchForFSEQs()
{
    QDir dir(m_fseqFolder);
    QFileInfoList files = dir.entryInfoList(QStringList() << "*.fseq", QDir::Files | QDir::NoDotAndDotDot);

    if (files.isEmpty()) {
        qDebug() << "No .fseq files found in the directory.";
        return;
    }
    refreshList(files);

    auto const file = m_fseqFolder + QDir::separator() + "xlights_networks.xml";

    if (QFile::exists(file)) {
        loadControllerFile(file);
    } else {
        m_logger->warn("No xLights Controller File found in the FSEQ folder.");
    }
}
void MainWindow::searchForUSBs()
{
    m_ui->comboBoxSDCard->clear();
    auto const mountedVolumes = QStorageInfo::mountedVolumes();
    for ( QStorageInfo const& storage : mountedVolumes) {
        if (storage.isReady() && storage.isValid()) {
            m_ui->comboBoxSDCard->addItem(storage.displayName() + " (" + storage.rootPath() + ")", storage.rootPath());
            qDebug() << "  Device:" << storage.device();
            qDebug() << "  Name:" << storage.name();
            qDebug() << "  Root Path:" << storage.rootPath();
            qDebug() << "  File System Type:" << storage.fileSystemType();
        }
    }
}

bool MainWindow::exportFSEQFile(std::string const& in_path, std::string const& out_path,int major_ver, int minor_ver, V2FSEQFile::CompressionType compressionType, std::vector<std::pair<uint32_t, uint32_t>> ranges, bool sparse)
{
    int compressionLevel = -99;
    std::unique_ptr<FSEQFile> src(FSEQFile::openFSEQFile(in_path));
    if (nullptr == src) {
        spdlog::critical("Error opening input file: {}", in_path);
        return false;
    }
    uint32_t channelCount{ 0 };
    for (auto const& [start, count] : ranges) {
        channelCount += count;
    }
    uint32_t const ogNumber_of_Frames = src->getNumFrames();
    uint32_t const ogNum_Channels = src->getChannelCount();
    int const ogFrame_Rate = src->getStepTime();
    if (ranges.empty()) {
        ranges.push_back(std::pair<uint32_t, uint32_t>(0, ogNum_Channels));
        channelCount = ogNum_Channels;
    }
    std::unique_ptr<FSEQFile> dest(FSEQFile::createFSEQFile(out_path,
        major_ver,
        compressionType,
        compressionLevel));
    if (nullptr == dest) {
        spdlog::critical("Failed to create FSEQ file (returned nullptr)!");
        return false;
    }
    dest->enableMinorVersionFeatures(minor_ver);

    if (major_ver == 2 && sparse) {
        V2FSEQFile* f = (V2FSEQFile*)dest.get();
        f->m_sparseRanges = ranges;
    }
    src->prepareRead(ranges);

    dest->initializeFromFSEQ(*src);
    dest->setChannelCount(channelCount);
    dest->writeHeader();

    uint8_t* WriteBuf = new uint8_t[channelCount];

    // read buff
    uint8_t* tmpBuf = new uint8_t[ogNum_Channels];

    uint32_t frame{ 0 };

    while (frame < ogNumber_of_Frames) {
        FSEQFile::FrameData* data = src->getFrame(frame);

        data->readFrame(tmpBuf, ogNum_Channels); // we have a read frame

        uint8_t* destBuf = WriteBuf;

        // Loop through ranges
        for (auto const& [start, count] : ranges) {
            uint8_t* tempSrc = tmpBuf + start;
            memmove(destBuf, tempSrc, count);
            destBuf += count;
        }
        dest->addFrame(frame, WriteBuf);

        delete data;
        frame++;
    }

    dest->finalize();
    delete[] tmpBuf;
    delete[] WriteBuf;
    return true;
}
