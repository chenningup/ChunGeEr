#ifndef DETECTORMANAGER_H
#define DETECTORMANAGER_H
#include "inference.h"
#include <QObject>

class DetectorManager : public QObject
{
    Q_OBJECT
public:
    explicit DetectorManager(QObject *parent = nullptr);

    static DetectorManager&Instance();

    void init(const QString &onnx , const QString & yamlPath);

    std::vector<DL_RESULT> detector(cv::Mat &img);
signals:

public:
    YOLO_V8* yoloDetector;
};

#endif // DETECTORMANAGER_H
