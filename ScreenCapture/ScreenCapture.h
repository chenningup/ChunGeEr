#pragma once

#include <windows.h>
#include <string>
#include <functional>
#include <QString>
namespace ScreenCaptureCore
{
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

        // Capture to memory buffer (PNG format)
        ErrorCode CaptureToMemory(std::vector<uint8_t>& outputBuffer, bool hideBorder = true, bool hideCursor = true);

    private:

        // Internal capture with options
        ErrorCode InternalCapture(const QString& outputPath, bool hideBorder, bool hideCursor);
        ErrorCode InternalCaptureToMemory(std::vector<uint8_t>& outputBuffer, bool hideBorder, bool hideCursor);
    };
}
