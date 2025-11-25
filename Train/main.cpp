#include "mainwindow.h"

#include <QApplication>
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include <QProcess>
#include <QDir>
#include "tesseract/TesseractTrain.h"
#include "tesseract/lstmtrainingxu.h"
#include "XuLog.h"
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setAttribute(Qt::AA_UseStyleSheetPropagationInWidgetStyles, false);
    XuLog::Instance()->init();
    // tesseract::TessBaseAPI tess;
    // if (tess.Init("D:\\Program Files\\Tesseract-OCR\\tessdata", "datang+chi_sim") != 0) {
    //     return 0;
    // }
    // QString path =QApplication::applicationDirPath()+"/Image";

    // QDir dir(path);

    // // // 设置名称过滤器，只查找.bmp文件
    // QStringList filters;
    // filters << "*.png";

    // // 获取所有匹配的文件
    // QStringList bmpFiles = dir.entryList(filters, QDir::Files | QDir::NoDotAndDotDot);
    // // // 输出结果
    // qDebug() << "Found BMP files:";
    // foreach (const QString &file, bmpFiles)
    // {
    //     QFileInfo info(file);
    //     QString fullpath = path+"/"+file;
    //     QString fullbase = path+"/"+info.baseName();
    //     TesseractTrain train;
    //     train.setConfig("makebox");
    //     train.setPageMode(7);
    //     train.setImage(fullpath.toStdString());
    //     train.setLang("chi_sim");
    //     train.setOutputbase(fullbase.toStdString());
    //     train.doTask();
    // }



    MainWindow w;
    w.show();
    return a.exec();
}
