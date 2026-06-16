// train_capture.cpp - 独立训练数据采集工具
// 不依赖 Qt，直接使用 ScreenCapture (WinRT DXGI) + OpenCV
// 编译: 作为独立 target 加入 CMakeLists，链接 ScreenCapture.lib + OpenCV

#include <windows.h>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <filesystem>
#include <vector>
#include <string>

// 引用 ScreenCapture 库（编译在同一项目内，通过 include path 找到）
#include "screencapturemanager.h"  // 用 ScreenCaptureManager 而非直接 WinRT

// XuLog
#include "XuLog.h"

using namespace cv;
using namespace std;
namespace fs = std::filesystem;

// ═══════════════════════════════════════════════════════════
// 预处理：Top-Hat 提取游戏文字 → 白底黑字
// ═══════════════════════════════════════════════════════════
static void preprocessForOCR(const Mat &src, Mat &dst)
{
    Mat gray;
    if (src.channels() == 3)
        cvtColor(src, gray, COLOR_BGR2GRAY);
    else
        gray = src.clone();

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
// 文字区域检测 + 裁剪
// ═══════════════════════════════════════════════════════════
static int cropTextRegions(const string &imgPath, const string &outDir)
{
    Mat src = imread(imgPath);
    if (src.empty()) {
        errorf("[CROP] 无法读取图片: {}", imgPath);
        return 1;
    }
    infof("[CROP] 图片: {}x{}", src.cols, src.rows);

    fs::create_directories(outDir);

    Mat gray, enhanced;
    cvtColor(src, gray, COLOR_BGR2GRAY);
    Ptr<CLAHE> clahe = createCLAHE(2.0, Size(8, 8));
    clahe->apply(gray, enhanced);

    vector<Rect> candidates;

    // 方案A: 自适应阈值
    {
        Mat binInv;
        adaptiveThreshold(enhanced, binInv, 255, ADAPTIVE_THRESH_GAUSSIAN_C,
                         THRESH_BINARY_INV, 15, 8);
        Mat kernel = getStructuringElement(MORPH_RECT, Size(8, 2));
        Mat closed;
        morphologyEx(binInv, closed, MORPH_CLOSE, kernel, Point(-1,-1), 2);
        vector<vector<Point>> conts;
        findContours(closed, conts, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
        for (auto &c : conts) {
            Rect r = boundingRect(c);
            double ar = (double)r.width / r.height;
            if (r.area() > 80 && r.width > 12 && r.height > 8 &&
                ar > 0.3 && ar < 20.0 && r.width < src.cols * 0.8) {
                candidates.push_back(r);
            }
        }
    }

    // 方案B: 形态学梯度
    {
        Mat grad;
        Mat kernel = getStructuringElement(MORPH_RECT, Size(3, 3));
        morphologyEx(enhanced, grad, MORPH_GRADIENT, kernel);
        Mat thresh;
        threshold(grad, thresh, 30, 255, THRESH_BINARY);
        Mat kernel2 = getStructuringElement(MORPH_RECT, Size(12, 3));
        Mat closed;
        morphologyEx(thresh, closed, MORPH_CLOSE, kernel2);
        vector<vector<Point>> conts;
        findContours(closed, conts, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
        for (auto &c : conts) {
            Rect r = boundingRect(c);
            double ar = (double)r.width / r.height;
            if (r.area() > 60 && r.width > 10 && r.height > 6 &&
                ar > 0.5 && ar < 25.0 && r.width < src.cols * 0.8) {
                candidates.push_back(r);
            }
        }
    }

    // 合并重叠区域
    vector<Rect> merged;
    for (size_t i = 0; i < candidates.size(); i++) {
        bool found = false;
        for (size_t j = 0; j < merged.size(); j++) {
            Rect inter = candidates[i] & merged[j];
            if (inter.area() > 0) {
                merged[j] = merged[j] | candidates[i];
                found = true;
                break;
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

        infof("[CROP] crop_{:03d} ({},{}) {}x{}", (int)i, safe.x, safe.y, safe.width, safe.height);
        saved++;
    }
    infof("[CROP] 完成，{} 个区域", saved);
    return 0;
}

// ═══════════════════════════════════════════════════════════
// main: --capture-train <outdir>
// ═══════════════════════════════════════════════════════════
int main(int argc, char *argv[])
{
    XuLog::Instance()->init();

    if (argc < 3 || string(argv[1]) != "--capture-train") {
        printf("用法: train_capture.exe --capture-train <输出目录>\n");
        return 1;
    }

    string outDir = argv[2];
    infof("[TRAIN] 全自动训练数据采集 → {}", outDir);

    // 1. 找游戏窗口
    HWND hwnd = FindWindowW(nullptr, L"大唐无双公测 - 七侠五义 (4.0.58:1041281  1.0.5:1039767)");
    if (!hwnd) {
        hwnd = nullptr;
        HWND h = FindWindowW(nullptr, nullptr);
        while (h) {
            wchar_t title[256];
            GetWindowTextW(h, title, 256);
            if (wcsstr(title, L"大唐无双")) { hwnd = h; break; }
            h = GetWindow(h, GW_HWNDNEXT);
        }
    }
    if (!hwnd) {
        errorf("[TRAIN] 找不到游戏窗口");
        return 1;
    }
    infof("[TRAIN] 游戏窗口 HWND=0x{:X}", (uintptr_t)hwnd);

    // 2. 激活窗口
    ShowWindow(hwnd, SW_RESTORE);
    SetForegroundWindow(hwnd);
    Sleep(500);

    // 3. 使用 ScreenCaptureManager 截图
    //    注意：ScreenCaptureManager 需要 WinRT，这里偷懒用 GDI 桌面截
    //    如果游戏在桌面上可见，桌面截图会包含游戏画面
    infof("[TRAIN] GDI 桌面截图...");
    HDC hdcScreen = GetDC(nullptr);
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    infof("[TRAIN] 桌面尺寸: {}x{}", sw, sh);

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

    SelectObject(hdcMem, old);
    DeleteObject(hbm);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);

    string capPath = outDir + "/capture.png";
    imwrite(capPath, bgr);
    infof("[TRAIN] 截图完成: {}", capPath);

    // 4. 文字区域检测
    return cropTextRegions(capPath, outDir);
}
