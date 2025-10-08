#pragma once

#include <windows.h>
#include <string>
#include <functional>
#include <QString>
#include "pch.h"
using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Graphics::Capture;
using namespace winrt::Windows::Graphics::DirectX;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;
using namespace winrt::Windows::Graphics::Imaging;
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::Streams;
namespace ScreenCaptureCore
{
struct ScreenData
{
    ScreenData() {}
    D3D11_TEXTURE2D_DESC des;
    int RowPitch;
    std::shared_ptr<std::vector<uint8_t>> data;
};
    // Error codes
    enum class ErrorCode : int
    {
        Success = 0,
        InitializationFailed = 1,
        CaptureItemCreationFailed = 2,
        CaptureSessionFailed = 3,
        TextureProcessingFailed = 4,
        FileSaveFailed = 5,
        TimeoutError = 6,
        UnknownError = 99
    };

    // Main screen capture class
    class ScreenCapture
    {
    public:
        ScreenCapture();
        ~ScreenCapture();

        // Capture primary monitor and save to PNG file
        ErrorCode CaptureToFile(const QString& outputPath);
        
        // Capture with options
        ErrorCode CaptureToFile(const QString& outputPath, bool hideBorder, bool hideCursor = true);


        static ErrorCode CaptureToMemory(ScreenData &outputBuffer, bool hideBorder = true, bool hideCursor = true);

        static com_ptr<ID3D11Device> CreateD3DDevice();

        static IDirect3DDevice CreateDirect3DDeviceFromD3D11Device(const com_ptr<ID3D11Device>& d3d11Device);

        static GraphicsCaptureItem CreateCaptureItemForMonitor();
    private:

        // Internal capture with options
        ErrorCode InternalCapture(const QString& outputPath, bool hideBorder, bool hideCursor);
        static ErrorCode InternalCaptureToMemory(ScreenData &outputBuffer, bool hideBorder, bool hideCursor);
    };
}
