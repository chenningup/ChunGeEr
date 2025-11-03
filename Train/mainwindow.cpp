#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QFileInfo>
#include "tesseract/TesseractTrain.h"
#include "tesseract/lstmtrainingxu.h"
#include <QDir>
#include <opencv2/opencv.hpp>
#include <QFileDialog>
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    init();
}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_boxBtn_clicked()
{
    if(ui->dir_lineEdit->text() == "")
    {
        return;
    }

//    QString path =QApplication::applicationDirPath()+"/image";
//    QString pathImageLater =QApplication::applicationDirPath()+"/ImageLater";
//    QDir dir(path);
//    QStringList filters1;
//    filters1<<"*.png";
//    QStringList bmpBeforFiles = dir.entryList(filters1, QDir::Files | QDir::NoDotAndDotDot);
//    foreach (const QString &bmpBefor, bmpBeforFiles)
//    {
//        QFileInfo info(bmpBefor);
//        QString fullpath = path+"/"+bmpBefor;
//        //QString fullbase = path+"/"+info.baseName();
//        cv::Mat imageMat = cv::imread(fullpath.toStdString()); // 请替换为你的图片路径
//        cv::Mat gray;
//        if (imageMat.empty())
//        {
//            continue;
//        }
//        if (imageMat.channels() == 4)
//        {
//            cv::cvtColor(imageMat, gray, cv::COLOR_BGRA2GRAY);
//        }
//        else
//        {
//            cv::cvtColor(imageMat, gray, cv::COLOR_BGR2GRAY);
//        }
//        // Otsu 自动阈值二值化
//        cv::Mat binary;
//        cv::threshold(gray, binary, 105, 255, cv::THRESH_BINARY);

//        // // 保存结果
//        QString output = QString(pathImageLater+"/output_%1.png").arg(info.baseName());
//        cv::imwrite(output.toStdString(), binary);
//    }

    QString lang_path = QString("./%1.traineddata").arg(ui->language_comboBox->currentText());
    QStringList filters;
    filters << QString("*.%1").arg(ui->pic_comboBox->currentText());
    QDir imageLaterDir(ui->dir_lineEdit->text());
    // 获取所有匹配的文件
    QStringList bmpFiles = imageLaterDir.entryList(filters, QDir::Files | QDir::NoDotAndDotDot);
    // // 输出结果
    foreach (const QString &file, bmpFiles)
    {
        QString fullPath = imageLaterDir.absoluteFilePath(file);
        QFileInfo fileInfo(fullPath);
        QString outputbase = ui->dir_lineEdit->text()+"/"+fileInfo.baseName();
        TesseractTrain train;
        train.setConfig("makebox");
        train.setPageMode(ui->psm_comboBox->currentText().toInt());
        train.setImage(fullPath.toStdString());
        train.setLang(ui->language_comboBox->currentText().toStdString());
        train.setOutputbase(outputbase.toStdString());
        train.doTask();
    }
}


void MainWindow::on_trainBtn_clicked()
{
    QString path = ui->pic_comboBox->currentText();
    QString lang_path = QString("%1/%2.traineddata").arg(QApplication::applicationDirPath()+"/tessdata").arg(ui->language_comboBox->currentText());;
    QString lang_lstm_path = QString("%1/%2.lstm").arg(QApplication::applicationDirPath()+"/tessdata").arg(ui->language_comboBox->currentText());
    QString checkpoint_path = QApplication::applicationDirPath()+"/checkpoint";
    QDir dir(path);
    QStringList filters;
    filters << "*.png";
    QStringList bmpFiles = dir.entryList(filters, QDir::Files | QDir::NoDotAndDotDot);
    foreach (const QString &file, bmpFiles)
    {
        QString fullPath = dir.absoluteFilePath(file);
        QFileInfo info(fullPath);
        QString fullbase = path+"/"+info.baseName();
        TesseractTrain train;
        train.setConfig("lstm.train");
        train.setPageMode(ui->psm_comboBox->currentText().toInt());
        train.setImage(fullPath.toStdString());
        train.setLang(ui->language_comboBox->currentText().toStdString());
        train.setOutputbase(fullbase.toStdString());
        train.doTask();
    }
    QStringList filterslstmf;
    filterslstmf << "*.lstmf";

    // 获取所有匹配的文件
    QStringList bmplstmfs = dir.entryList(filterslstmf, QDir::Files | QDir::NoDotAndDotDot);

    std::vector<std::string> filenameList;
    for (auto &var : bmplstmfs)
    {
        QString lstmfPath =path+"/"+var;
        filenameList.push_back(lstmfPath.toStdString());
    }
    lstmtraining train1;
    train1.extractTraineddataToFile(lang_path.toStdString(),lang_lstm_path.toStdString());

    train1.set_model_output(QString(QApplication::applicationDirPath()+"/checkpoint/check").toStdString());
    train1.set_continue_from(lang_lstm_path.toStdString());
    train1.set_traineddata(lang_path.toStdString());
    train1.set_target_error_rate(ui->target_error_rate_lineEdit->text().toDouble());
    train1.set_max_iterations(ui->max_iterations_lineEdit->text().toInt());
    train1.set_filenames(filenameList);
    train1.startTrain();

    QDir dircheck(checkpoint_path);
    QFileInfo newestFile;
    QDateTime newestTime;
    QFileInfoList files = dircheck.entryInfoList(QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks);

    for (const QFileInfo &fileInfo : files)
    {
        QDateTime modifiedTime = fileInfo.lastModified();
        if (!newestTime.isValid() || modifiedTime > newestTime)
        {
            newestTime = modifiedTime;
            newestFile = fileInfo;
        }
    }
    if(newestFile.exists())
    {
        train1.set_continue_from(newestFile.absoluteFilePath().toStdString());
        train1.set_model_output(QString(QApplication::applicationDirPath()+"/output/datang.traineddata").toStdString());
        train1.set_traineddata(lang_path.toStdString());
        train1.set_stop_training(true);
        train1.startTrain();
    }
}


void MainWindow::on_openImagePathBtn_clicked()
{
    QFileDialog dialog;
    dialog.setFileMode(QFileDialog::Directory);
    dialog.setOption(QFileDialog::ShowDirsOnly, true);
    dialog.setOption(QFileDialog::DontResolveSymlinks, true);
    dialog.setWindowTitle("选择目标文件夹");
    dialog.setDirectory(QDir::homePath());

    if (dialog.exec() == QFileDialog::Accepted)
    {
        QStringList selectedFolders = dialog.selectedFiles();
        if (!selectedFolders.isEmpty()) {
            QString folderPath = selectedFolders.first();
            ui->dir_lineEdit->setText(folderPath);
        }
    }
}

void MainWindow::init()
{
    ui->psm_comboBox->setCurrentText("6");
    ui->max_iterations_lineEdit->setText("0");
    ui->target_error_rate_lineEdit->setText("0.001");

    QString tessdata_path = QApplication::applicationDirPath()+"/tessdata";

    QDir dir(tessdata_path);

    if (!dir.exists())
    {
        qDebug() << "文件夹不存在：" << tessdata_path;
        return;
    }
    // 获取所有条目（文件和文件夹）
    QStringList entries = dir.entryList();

    for (const QString &entry : entries)
    {
        if (entry != "." && entry != "..")
        { // 忽略当前目录和上级目录
            QString fullPath = dir.absoluteFilePath(entry);
            QFileInfo fileInfo(fullPath);

            if (fileInfo.isFile() && fileInfo.suffix() == "traineddata" )
            {
                ui->language_comboBox->addItem(fileInfo.baseName());
            }
        }
    }
    ui->language_comboBox->setCurrentText("chi_sim");
}
