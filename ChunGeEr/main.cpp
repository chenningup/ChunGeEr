#include "mainwindow.h"
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
#include "XuLog.h"
#include <windows.h>
using namespace cv;
using namespace std;

// ═══════════════════════════════════════════════════════════
// 文字预处理：Top-Hat 提取游戏文字 → 白底黑字
// ═══════════════════════════════════════════════════════════
static void preprocessForOCR(const Mat &src, Mat &dst)
{
    Mat gray;
    if (src.channels() == 3) cvtColor(src, gray, COLOR_BGR2GRAY);
    else gray = src.clone();

    Mat upscaled;
    resize(gray, upscaled, Size(), 3.0, 3.0, INTER_CUBIC);

    int ks = max(3, upscaled.cols / 20);
    Mat kernel = getStructuringElement(MORPH_ELLIPSE, Size(ks, ks));
    Mat tophat, blackhat;
    morphologyEx(upscaled, tophat, MORPH_TOPHAT, kernel);
    morphologyEx(upscaled, blackhat, MORPH_BLACKHAT, kernel);
    Mat merged = tophat + blackhat;

    Mat binary;
    threshold(merged, binary, 0, 255, THRESH_BINARY | THRESH_OTSU);
    double fg = countNonZero(binary) / (double)(binary.rows * binary.cols);
    if (fg < 0.02 || fg > 0.90) {
        adaptiveThreshold(upscaled, binary, 255,
                         ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY_INV, 13, 5);
        Scalar m = mean(binary);
        if (m[0] > 128) bitwise_not(binary, binary);
    }

    Mat dk = getStructuringElement(MORPH_ELLIPSE, Size(2, 2));
    morphologyEx(binary, binary, MORPH_OPEN, dk);
    morphologyEx(binary, binary, MORPH_CLOSE, getStructuringElement(MORPH_RECT, Size(3, 1)));
    bitwise_not(binary, binary);
    copyMakeBorder(binary, dst, 12, 12, 12, 12, BORDER_CONSTANT, Scalar(255));
}

// ═══════════════════════════════════════════════════════════
// GDI 桌面截图（全屏）
// ═══════════════════════════════════════════════════════════
static bool captureDesktopGDI(const string &path)
{
    HDC hdcScreen = GetDC(nullptr);
    if (!hdcScreen) return false;
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hbm = CreateCompatibleBitmap(hdcScreen, sw, sh);
    HGDIOBJ old = SelectObject(hdcMem, hbm);
    BitBlt(hdcMem, 0, 0, sw, sh, hdcScreen, 0, 0, SRCCOPY);
    Mat img(sh, sw, CV_8UC4);
    BITMAPINFOHEADER bi = {};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = sw;
    bi.biHeight = -sh;
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;
    GetDIBits(hdcMem, hbm, 0, sh, img.data, (BITMAPINFO*)&bi, DIB_RGB_COLORS);
    Mat bgr;
    cvtColor(img, bgr, COLOR_BGRA2BGR);
    SelectObject(hdcMem, old); DeleteObject(hbm); DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
    imwrite(path, bgr);
    return true;
}

// ═══════════════════════════════════════════════════════════
// GDI 窗口截图（仅截指定窗口区域）
// ═══════════════════════════════════════════════════════════
static bool captureWindowGDI(const string &path, HWND hwnd)
{
    RECT rect;
    if (!GetWindowRect(hwnd, &rect)) return false;
    int w = rect.right - rect.left;
    int h = rect.bottom - rect.top;
    if (w <= 0 || h <= 0) return false;
    printf("[WNDCAP] 窗口位置 (%d,%d) 大小 %dx%d\n", rect.left, rect.top, w, h);

    HDC hdcScreen = GetDC(nullptr);
    if (!hdcScreen) return false;
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hbm = CreateCompatibleBitmap(hdcScreen, w, h);
    HGDIOBJ old = SelectObject(hdcMem, hbm);
    BitBlt(hdcMem, 0, 0, w, h, hdcScreen, rect.left, rect.top, SRCCOPY);

    Mat img(h, w, CV_8UC4);
    BITMAPINFOHEADER bi = {};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = w;
    bi.biHeight = -h;
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;
    GetDIBits(hdcMem, hbm, 0, h, img.data, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

    Mat bgr;
    cvtColor(img, bgr, COLOR_BGRA2BGR);

    SelectObject(hdcMem, old); DeleteObject(hbm); DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
    imwrite(path, bgr);
    printf("[WNDCAP] 保存 %s\n", path.c_str());
    return true;
}

// ═══════════════════════════════════════════════════════════
// 文字区域检测 + 裁剪
// ═══════════════════════════════════════════════════════════
static int cropTextRegions(const string &imgPath, const string &outDir)
{
    Mat src = imread(imgPath);
    if (src.empty()) { printf("[CROP] 无法读取图片\n"); return 1; }
    printf("[CROP] 图片: %dx%d\n", src.cols, src.rows);
    filesystem::create_directories(outDir);

    Mat gray, enhanced;
    cvtColor(src, gray, COLOR_BGR2GRAY);
    Ptr<CLAHE> clahe = createCLAHE(2.0, Size(8, 8));
    clahe->apply(gray, enhanced);

    vector<Rect> candidates;
    // A: 自适应阈值
    {
        Mat binInv;
        adaptiveThreshold(enhanced, binInv, 255, ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY_INV, 15, 8);
        Mat kernel = getStructuringElement(MORPH_RECT, Size(8, 2));
        Mat closed;
        morphologyEx(binInv, closed, MORPH_CLOSE, kernel, Point(-1,-1), 2);
        vector<vector<Point>> conts;
        findContours(closed, conts, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
        for (auto &c : conts) {
            Rect r = boundingRect(c);
            double ar = (double)r.width / r.height;
            if (r.area() > 80 && r.width > 12 && r.height > 8 && ar > 0.3 && ar < 20.0 && r.width < src.cols * 0.8)
                candidates.push_back(r);
        }
    }
    // B: 形态学梯度
    {
        Mat grad;
        morphologyEx(enhanced, grad, MORPH_GRADIENT, getStructuringElement(MORPH_RECT, Size(3, 3)));
        Mat thresh;
        threshold(grad, thresh, 30, 255, THRESH_BINARY);
        Mat closed;
        morphologyEx(thresh, closed, MORPH_CLOSE, getStructuringElement(MORPH_RECT, Size(12, 3)));
        vector<vector<Point>> conts;
        findContours(closed, conts, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
        for (auto &c : conts) {
            Rect r = boundingRect(c);
            double ar = (double)r.width / r.height;
            if (r.area() > 60 && r.width > 10 && r.height > 6 && ar > 0.5 && ar < 25.0 && r.width < src.cols * 0.8)
                candidates.push_back(r);
        }
    }
    // 合并重叠
    vector<Rect> merged;
    for (size_t i = 0; i < candidates.size(); i++) {
        bool found = false;
        for (size_t j = 0; j < merged.size(); j++) {
            if ((candidates[i] & merged[j]).area() > 0) {
                merged[j] = merged[j] | candidates[i]; found = true; break;
            }
        }
        if (!found) merged.push_back(candidates[i]);
    }
    sort(merged.begin(), merged.end(), [](const Rect &a, const Rect &b) {
        if (abs(a.y - b.y) < 30) return a.x < b.x;
        return a.y < b.y;
    });

    int saved = 0;
    for (size_t i = 0; i < merged.size(); i++) {
        Rect r = merged[i];
        int px = max(0, r.x - 4), py = max(0, r.y - 4);
        int pw = min(src.cols - px, r.width + 8), ph = min(src.rows - py, r.height + 8);
        Rect safe(px, py, pw, ph);
        if (safe.width < 15 || safe.height < 10) continue;
        if ((double)safe.width / safe.height > 30) continue;
        Mat crop = src(safe).clone();
        char name[256];
        snprintf(name, sizeof(name), "%s/crop_%03zu.png", outDir.c_str(), i);
        imwrite(name, crop);
        Mat prep;
        preprocessForOCR(crop, prep);
        snprintf(name, sizeof(name), "%s/crop_%03zu_prep.png", outDir.c_str(), i);
        imwrite(name, prep);
        printf("[CROP] crop_%03zu (%d,%d) %dx%d\n", i, safe.x, safe.y, safe.width, safe.height);
        saved++;
    }
    printf("[CROP] 完成，%d 个区域\n", saved);
    return 0;
}

// ═══════════════════════════════════════════════════════════
// 找游戏窗口
// ═══════════════════════════════════════════════════════════
static HWND findGameWindow()
{
    HWND hwnd = FindWindowW(nullptr, L"大唐无双公测 - 七侠五义 (4.0.58:1041281  1.0.5:1039767)");
    if (hwnd) return hwnd;
    HWND h = FindWindowW(nullptr, nullptr);
    while (h) {
        wchar_t title[256];
        GetWindowTextW(h, title, 256);
        if (wcsstr(title, L"大唐无双")) return h;
        h = GetWindow(h, GW_HWNDNEXT);
    }
    return nullptr;
}

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
    SetConsoleOutputCP(CP_UTF8);  // 控制台UTF-8, 避免中文乱码
    // ── 命令行模式（不需要 QApplication）────────────────────
    string exePath = argv[0];
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];

        if (arg == "--crop-text" && i + 2 < argc) {
            return cropTextRegions(argv[i + 1], argv[i + 2]);
        }

        if (arg == "--capture-train" && i + 1 < argc) {
            string outDir = argv[i + 1];
            printf("[TRAIN] 全自动训练数据采集 → %s\n", outDir.c_str());

            HWND hwnd = findGameWindow();
            if (!hwnd) { printf("[TRAIN] 找不到游戏窗口!\n"); return 1; }
            printf("[TRAIN] 游戏窗口 HWND=0x%llX\n", (uint64_t)hwnd);

            filesystem::create_directories(outDir);

            ShowWindow(hwnd, SW_RESTORE);
            SetForegroundWindow(hwnd);
            Sleep(500);

            string capPath = outDir + "/capture.png";
            printf("[TRAIN] GDI 窗口截图...\n");
            if (!captureWindowGDI(capPath, hwnd)) {
                printf("[TRAIN] 截图失败!\n"); return 1;
            }
            printf("[TRAIN] 截图完成\n");
            return cropTextRegions(capPath, outDir);
        }
    }

    // ── GUI 模式 ────────────────────────────────────────
    QApplication a(argc, argv);
    XuLog::Instance()->init();

    MainWindow w;
    w.show();
    return a.exec();
}
