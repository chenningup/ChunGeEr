#include "mainwindow.h"
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include <QApplication>
#include <QDebug>

#include <QThread>
#include <opencv2/opencv.hpp>
#include "inference.h"

#include <iostream>
#include <iomanip>
#include <filesystem>
#include <fstream>
#include <random>
#include <QDir>
using namespace cv;
using namespace std;

unsigned char buffer[100];
int pr = 0;

enum pasreDataStatus
{
    findHeadFirtst,
    findHeadSecond,
    findEndFirtst,
    findEndSecond,
};
pasreDataStatus status = findHeadFirtst;
uint16_t crc_16(uint8_t *data, uint16_t len)
{
    uint16_t crc_reg = 0xffff;
    for (uint16_t i = 0; i < len; i++)
    {
        //infof(" crc_16{:x} i {}", data[i],i);
        crc_reg ^= data[i];
        for (uint16_t j = 0; j < 8; j++)
        {
            if (crc_reg & 0x01)
            {
                crc_reg = ((crc_reg >> 1) ^ 0xa001);
            }
            else
            {
                crc_reg = crc_reg >> 1;
            }
        }
    }
    return crc_reg;
}
void loop() {
    // 检查串口是否有数据到达[1](@ref)
    QByteArray data;
    data.append(0x66);
    data.append(0x68);
    data.append(0x02);
    data.append(0x01);
    data.append(0x9c);
    data.append(0xff);
    data.append(0xff);
    data.append(0xff);
    data.append(0x9c);
    data.append(0xff);
    data.append(0xff);
    data.append(0xff);
    data.append(0xC5);
    data.append(0xA0);
    data.append(0x5B);
    data.append(0x81);
    for (int var = 0; var < data.size(); ++var)
    {
        unsigned char inChar = data[var];
        switch(status)
        {
        case findHeadFirtst:
        {
            if(inChar == 0X66)
            {
                buffer[pr] = 0X66;
                pr++;
                status = findHeadSecond;
            }
        }
        break;
        case findHeadSecond:
        {
            if(inChar == 0X68)
            {
                buffer[pr] = 0X68;
                pr++;
                status = findEndFirtst;
            }
            else
            {
                pr=0;
                status = findHeadFirtst;
            }
        }
        break;
        case findEndFirtst:
        {
            buffer[pr] = inChar;
            if(inChar == 0X5b)
            {
                status = findEndSecond;
            }
            pr++;
        }
        break;
        case findEndSecond:
        {
            buffer[pr] = inChar;
            pr++;
            if(inChar == 0X81)
            {
                uint16_t crc = crc_16(&buffer[2],pr-2-2-2);
                uint16_t recCrc;
                memcpy(&recCrc, &buffer[pr-2-2], 2);
                if (crc != recCrc)
                {
                    pr=0;
                    status = findHeadFirtst;
                    qDebug()<<"crc error";
                }
                else
                {
                    int cmd =  buffer[3];
                    if(cmd == 1)//键盘
                    {
                        int x;
                        memcpy(&x, &buffer[4], 4);
                        int y;
                        memcpy(&y, &buffer[8], 4);
                        //Mouse.move(x, y, 0);
                        //Mouse.move(y, y, 0);
                        //Keyboard.write(buffer[4]);
                    }
                    if(cmd == 2)
                    {

                    }
                    pr=0;
                    status = findHeadFirtst;
                }
            }
            else
            {
                status = findEndFirtst;
                pr=0;
            }
        }
        break;
        }
        // 读取来自串口的字符串，直到遇到换行符'\n'
    }
}


void Detector(YOLO_V8*& p) {
    std::filesystem::path current_path = std::filesystem::current_path();
    std::filesystem::path imgs_path = current_path / "images";
    for (auto& i : std::filesystem::directory_iterator(imgs_path))
    {
        if (i.path().extension() == ".jpg" || i.path().extension() == ".png" || i.path().extension() == ".jpeg")
        {
            std::string img_path = i.path().string();
            cv::Mat img = cv::imread(img_path);
            std::vector<DL_RESULT> res;
            p->RunSession(img, res);

            for (auto& re : res)
            {
                cv::RNG rng(cv::getTickCount());
                cv::Scalar color(rng.uniform(0, 256), rng.uniform(0, 256), rng.uniform(0, 256));

                cv::rectangle(img, re.box, color, 3);

                float confidence = floor(100 * re.confidence) / 100;
                std::cout << std::fixed << std::setprecision(2);
                std::string label = p->classes[re.classId] + " " +
                                    std::to_string(confidence).substr(0, std::to_string(confidence).size() - 4);

                cv::rectangle(
                    img,
                    cv::Point(re.box.x, re.box.y - 25),
                    cv::Point(re.box.x + label.length() * 15, re.box.y),
                    color,
                    cv::FILLED
                    );

                cv::putText(
                    img,
                    label,
                    cv::Point(re.box.x, re.box.y - 5),
                    cv::FONT_HERSHEY_SIMPLEX,
                    0.75,
                    cv::Scalar(0, 0, 0),
                    2
                    );
            }
            std::cout << "Press any key to exit" << std::endl;
            cv::imshow("Result of Detection", img);
            cv::waitKey(0);
            cv::destroyAllWindows();
        }
    }
}


void Classifier(YOLO_V8*& p)
{
    std::filesystem::path current_path = std::filesystem::current_path();
    std::filesystem::path imgs_path = current_path;// / "images"
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(0, 255);
    for (auto& i : std::filesystem::directory_iterator(imgs_path))
    {
        if (i.path().extension() == ".jpg" || i.path().extension() == ".png")
        {
            std::string img_path = i.path().string();
            //std::cout << img_path << std::endl;
            cv::Mat img = cv::imread(img_path);
            std::vector<DL_RESULT> res;
            std::string ret = p->RunSession(img, res);

            float positionY = 50;
            for (int i = 0; i < res.size(); i++)
            {
                int r = dis(gen);
                int g = dis(gen);
                int b = dis(gen);
                cv::putText(img, std::to_string(i) + ":", cv::Point(10, positionY), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(b, g, r), 2);
                cv::putText(img, std::to_string(res.at(i).confidence), cv::Point(70, positionY), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(b, g, r), 2);
                positionY += 50;
            }

            cv::imshow("TEST_CLS", img);
            cv::waitKey(0);
            cv::destroyAllWindows();
            //cv::imwrite("E:\\output\\" + std::to_string(k) + ".png", img);
        }

    }
}

int ReadCocoYaml(YOLO_V8*& p) {
    // Open the YAML file
    std::ifstream file("data.yaml");
    if (!file.is_open())
    {
        std::cerr << "Failed to open file" << std::endl;
        return 1;
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

    p->classes = names;
    return 0;
}


void DetectTest()
{
    YOLO_V8* yoloDetector = new YOLO_V8;
    ReadCocoYaml(yoloDetector);
    DL_INIT_PARAM params;
    params.rectConfidenceThreshold = 0.05;
    params.iouThreshold = 0.5;
    params.modelPath = "best.onnx";
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
    Detector(yoloDetector);
}


void ClsTest()
{
    YOLO_V8* yoloDetector = new YOLO_V8;
    std::string model_path = "cls.onnx";
    ReadCocoYaml(yoloDetector);
    DL_INIT_PARAM params{ model_path, YOLO_CLS, {224, 224} };
    yoloDetector->CreateSession(params);
    Classifier(yoloDetector);
}



int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    //loop();
    //DetectTest();
    //QThread::sleep(10);


    tesseract::TessBaseAPI tess;
    if (tess.Init("D:\\Program Files\\Tesseract-OCR\\tessdata", "chi_sim_custom+chi_sim") != 0) {
        return 0;
    }
    QDir dir(QApplication::applicationDirPath());

    // 设置名称过滤器，只查找.bmp文件
    QStringList filters;
    filters << "*.png";

    // 获取所有匹配的文件
    QStringList bmpFiles = dir.entryList(filters, QDir::Files | QDir::NoDotAndDotDot);

    // 输出结果
    qDebug() << "Found BMP files:";
    foreach (const QString &file, bmpFiles) {
        Mat imageMat = imread(file.toStdString()); // 请替换为你的图片路径
        cv::imshow("imageMat", imageMat);
               cv::waitKey(0);
        qDebug()<<file << imageMat.channels();
        Mat gray;
        if (imageMat.empty())
        {
            cout << "Could not open or find the image!" << endl;
            return -1;
        }
        if (imageMat.channels() == 4) {
            cvtColor(imageMat, gray, COLOR_BGRA2GRAY);
        } else {
            cvtColor(imageMat, gray, COLOR_BGR2GRAY);
        }
        //        cv::imshow("gray", gray);
        //        cv::waitKey(0);
        // Otsu 自动阈值二值化
        Mat binary;
        cv::threshold(gray, binary, 105, 255, THRESH_BINARY);

        // // 保存结果
        QString output = QString("output_%1.png").arg(file);
        //        cv::imshow("binary", binary);
        //        cv::waitKey(0);
        cv::imwrite(output.toStdString(), binary);
        qDebug()<<binary.channels();
        PIX* image = nullptr;
        if (binary.channels() == 1) {  // 灰度图
            image = pixCreate(binary.cols, binary.rows, 8); // 8位深度
            for (int y = 0; y < binary.rows; y++) {
                for (int x = 0; x < binary.cols; x++) {
                    pixSetPixel(image, x, y, binary.at<uchar>(y, x));
                }
            }
        }
        tess.SetImage(image);
        char *text = tess.GetUTF8Text();
        QString ocrResult = QString::fromUtf8(text);
        qDebug()<<ocrResult;
        pixDestroy(&image);
        delete[] text;
    }

    //QString path ="D:\\123.bmp";
    //   QString path ="test.png";
    //   Pix *image = pixRead(path.toLocal8Bit().data());
    //tess.End();
    MainWindow w;
    w.show();
    return a.exec();
}
