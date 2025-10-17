#include "ocrmnager.h"

OcrMnager::OcrMnager(QObject *parent)
    : QObject{parent}
{

}

OcrMnager &OcrMnager::Instance()
{
    static OcrMnager mOcrMnager;
    return mOcrMnager;
}

void OcrMnager::init()
{
    if (tess.Init("D:\\Program Files\\Tesseract-OCR\\tessdata", "chi_sim_custom") != 0)
    {
         return ;
    }
}
