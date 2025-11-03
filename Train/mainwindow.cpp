#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QFileInfo>
#include "tesseract/TesseractTrain.h"
#include "tesseract/lstmtrainingxu.h"
#include <QDir>
#include <opencv2/opencv.hpp>
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_boxBtn_clicked()
{
    QStringList filters;
    filters << "*.png"<<"*.bmp";
    QString path =QApplication::applicationDirPath()+"/Image";
    QString pathImageLater =QApplication::applicationDirPath()+"/ImageLater";
    QDir dir(path);

    QStringList bmpBeforFiles = dir.entryList(filters, QDir::Files | QDir::NoDotAndDotDot);
    foreach (const QString &bmpBefor, bmpBeforFiles)
    {
        QFileInfo info(bmpBefor);
        QString fullpath = path+"/"+bmpBefor;
        //QString fullbase = path+"/"+info.baseName();
        cv::Mat imageMat = cv::imread(fullpath.toStdString()); // 请替换为你的图片路径
        cv::Mat gray;
        if (imageMat.empty())
        {
            continue;
        }
        if (imageMat.channels() == 4)
        {
            cv::cvtColor(imageMat, gray, cv::COLOR_BGRA2GRAY);
        }
        else
        {
            cv::cvtColor(imageMat, gray, cv::COLOR_BGR2GRAY);
        }
        // Otsu 自动阈值二值化
        cv::Mat binary;
        cv::threshold(gray, binary, 105, 255, cv::THRESH_BINARY);

        // // 保存结果
        QString output = QString(pathImageLater+"/output_%1.png").arg(info.baseName());
        cv::imwrite(output.toStdString(), binary);
    }



    QDir imageLaterDir(pathImageLater);
    // 获取所有匹配的文件
    QStringList bmpFiles = imageLaterDir.entryList(filters, QDir::Files | QDir::NoDotAndDotDot);
    // // 输出结果
    qDebug() << "Found BMP files:";
    foreach (const QString &file, bmpFiles)
    {
        QFileInfo info(file);
        QString fullpath = pathImageLater+"/"+file;
        QString fullbase = pathImageLater+"/"+info.baseName();
        TesseractTrain train;
        train.setConfig("makebox");
        train.setPageMode(7);
        train.setImage(fullpath.toStdString());
        train.setLang("chi_sim");
        train.setOutputbase(fullbase.toStdString());
        train.doTask();
    }
}


void MainWindow::on_trainBtn_clicked()
{
    QString path =QApplication::applicationDirPath()+"/ImageLater";
    QString chi_sim_path = QApplication::applicationDirPath()+"/tessdata/chi_sim.traineddata";
    QString chi_sim_lstm_path = QApplication::applicationDirPath()+"/tessdata/chi_sim.lstm";
    QString checkpoint_path = QApplication::applicationDirPath()+"/checkpoint";
    QDir dir(path);
    QStringList filters;
    filters << "*.png";
    // 获取所有匹配的文件
    QStringList bmpFiles = dir.entryList(filters, QDir::Files | QDir::NoDotAndDotDot);
    foreach (const QString &file, bmpFiles)
    {
        QFileInfo info(file);
        QString fullpath = path+"/"+file;
        QString fullbase = path+"/"+info.baseName();
        TesseractTrain train;
        train.setConfig("lstm.train");
        train.setPageMode(6);
        train.setImage(fullpath.toStdString());
        train.setLang("chi_sim");
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
    train1.extractTraineddataToFile(chi_sim_path.toStdString(),chi_sim_lstm_path.toStdString());

    train1.set_model_output(QString(QApplication::applicationDirPath()+"/checkpoint/check").toStdString());
    train1.set_continue_from(chi_sim_lstm_path.toStdString());
    train1.set_traineddata(chi_sim_path.toStdString());
    train1.set_target_error_rate(0.01);
    train1.set_max_iterations(0);
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
        train1.set_traineddata(chi_sim_path.toStdString());
        train1.set_stop_training(true);
        train1.startTrain();
    }
}

