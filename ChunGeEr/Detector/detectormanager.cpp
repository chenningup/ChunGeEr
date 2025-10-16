#include "detectormanager.h"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <random>
#include <iostream>
#include <regex>
using namespace std;

DetectorManager::DetectorManager(QObject *parent)
    : QObject{parent}
{
    yoloDetector = new YOLO_V8;
}

DetectorManager &DetectorManager::Instance()
{
    static DetectorManager mDetectorManager;
    return mDetectorManager;
}

void DetectorManager::init(const QString &onnx, const QString &yamlPath)
{
    std::ifstream file(yamlPath.toStdString());
    if (!file.is_open())
    {
        std::cerr << "Failed to open file" << std::endl;
        return ;
    }

    // Read the file line by line
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(file, line))
    {
        lines.push_back(line);
    }

    // Find the start and end of the names section
    std::size_t start = 0;
    std::size_t end = 0;
    for (std::size_t i = 0; i < lines.size(); i++)
    {
        if (lines[i].find("names:") != std::string::npos)
        {
            start = i + 1;
        }
        else if (start > 0 && lines[i].find(':') == std::string::npos)
        {
            end = i;
            break;
        }
    }

    // Extract the names
    std::vector<std::string> names;
    for (std::size_t i = start; i < end; i++)
    {
        std::stringstream ss(lines[i]);
        std::string name;
        std::getline(ss, name, ':'); // Extract the number before the delimiter
        std::getline(ss, name); // Extract the string after the delimiter
        names.push_back(name);
    }

    yoloDetector->classes = names;

    DL_INIT_PARAM params;
    params.rectConfidenceThreshold = 0.1;
    params.iouThreshold = 0.5;
    params.modelPath = onnx.toStdString();
    params.imgSize = { 640, 640 };
#ifdef USE_CUDA
    params.cudaEnable = true;

    // GPU FP32 inference
    params.modelType = YOLO_DETECT_V8;
    // GPU FP16 inference
    //Note: change fp16 onnx model
    //params.modelType = YOLO_DETECT_V8_HALF;

#else
    // CPU inference
    params.modelType = YOLO_DETECT_V8;
    params.cudaEnable = false;
#endif
    yoloDetector->CreateSession(params);
}

std::vector<DL_RESULT> DetectorManager::detector(cv::Mat &img)
{
    std::vector<DL_RESULT> res;
    yoloDetector->RunSession(img, res);
    return res;
}
