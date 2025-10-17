#ifndef OCRMNAGER_H
#define OCRMNAGER_H

#include <QObject>
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
class OcrMnager : public QObject
{
    Q_OBJECT
public:
    explicit OcrMnager(QObject *parent = nullptr);

    static OcrMnager&Instance();

    void init();
signals:

private:

    tesseract::TessBaseAPI tess;

};

#endif // OCRMNAGER_H
