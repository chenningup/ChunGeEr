#include "ScreenCapture.h"
#include "pch.h"
#include <windows.graphics.directx.direct3d11.interop.h>
#include <iostream>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <thread>
#include <filesystem>
#include <QFile>
#include <QImage>
#include <QDateTime>
#include <windows.h>
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
    // Helper function to create D3D11 device
    com_ptr<ID3D11Device> ScreenCapture::CreateD3DDevice()
    {
        UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#if defined(_DEBUG)
        creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        D3D_FEATURE_LEVEL featureLevels[] =
        {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
            D3D_FEATURE_LEVEL_9_3,
            D3D_FEATURE_LEVEL_9_2,
            D3D_FEATURE_LEVEL_9_1
        };

        com_ptr<ID3D11Device> device;
        com_ptr<ID3D11DeviceContext> context;
        HRESULT hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            0,
            creationFlags,
            featureLevels,
            ARRAYSIZE(featureLevels),
            D3D11_SDK_VERSION,
            device.put(),
            nullptr,
            context.put()
        );

        if (FAILED(hr))
        {
            throw hresult_error(hr, L"Failed to create D3D11 device");
        }

        return device;
    }

    // Helper function to create Direct3D11Device from ID3D11Device
    IDirect3DDevice ScreenCapture:: CreateDirect3DDeviceFromD3D11Device(const com_ptr<ID3D11Device>& d3d11Device)
    {
        com_ptr<IDXGIDevice> dxgiDevice;
        winrt::check_hresult(d3d11Device->QueryInterface(dxgiDevice.put()));

        winrt::com_ptr<winrt::Windows::Foundation::IInspectable> inspectable;
        winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.get(), reinterpret_cast<::IInspectable**>(inspectable.put())));

        return inspectable.as<IDirect3DDevice>();
    }

    // Helper function to create GraphicsCaptureItem for primary monitor
    GraphicsCaptureItem ScreenCapture::CreateCaptureItemForMonitor()
    {
        auto factory = winrt::get_activation_factory<GraphicsCaptureItem>();
        auto interop = factory.as<IGraphicsCaptureItemInterop>();

        // Get primary monitor
        HMONITOR primaryMonitor = MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);

        GraphicsCaptureItem item{ nullptr };
        winrt::check_hresult(interop->CreateForMonitor(
            primaryMonitor,
            winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(),
            winrt::put_abi(item)
        ));

        return item;
    }

    // ScreenCapture implementation
    ScreenCapture::ScreenCapture()
    {
        try
        {
            init_apartment(apartment_type::single_threaded);
        }
        catch (...)
        {
            // Apartment may already be initialized
        }
    }

    ScreenCapture::~ScreenCapture()
    {
    }


    ErrorCode ScreenCapture::CaptureToFile(const QString& outputPath)
    {
        // Default: hide border and cursor for cleaner capture
        return CaptureToFile(outputPath, false, false);
    }

    ErrorCode ScreenCapture::CaptureToFile(const QString& outputPath, bool hideBorder, bool hideCursor)
    {
        return InternalCapture(outputPath, hideBorder, hideCursor);
    }

    ErrorCode ScreenCapture::CaptureToMemory(ScreenData &outputBuffer, bool hideBorder, bool hideCursor)
    {
        return InternalCaptureToMemory(outputBuffer, hideBorder, hideCursor);
    }

    ErrorCode ScreenCapture::InternalCapture(const QString& outputPath, bool hideBorder, bool hideCursor)
    {
        try
        {
            QDateTime time = QDateTime::currentDateTime();
            qDebug()<<"1"<<time.currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
            // 1. Create D3D11 Device
            auto d3d11Device = CreateD3DDevice();
            auto direct3DDevice = CreateDirect3DDeviceFromD3D11Device(d3d11Device);
            time = QDateTime::currentDateTime();
            qDebug()<<"2"<<time.currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
            // 2. Create capture item for primary monitor
            auto captureItem = CreateCaptureItemForMonitor();
            time = QDateTime::currentDateTime();
            qDebug()<<"3"<<time.currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
            // 3. Create Direct3D11CaptureFramePool
            auto framePool = Direct3D11CaptureFramePool::Create(
                direct3DDevice,
                DirectXPixelFormat::B8G8R8A8UIntNormalized,
                1,
                captureItem.Size()
            );
            time = QDateTime::currentDateTime();
            qDebug()<<"3"<<time.currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
            // 4. Create capture session
            auto session = framePool.CreateCaptureSession(captureItem);
            time = QDateTime::currentDateTime();
            qDebug()<<"3"<<time.currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
            // 5. Configure capture session options
            if (hideCursor)
            {
                session.IsCursorCaptureEnabled(false);
            }

            if (hideBorder)
            {
                if (auto session3 = session.try_as<winrt::Windows::Graphics::Capture::IGraphicsCaptureSession3>())
                {
                    session3.IsBorderRequired(false);
                }
                else
                {
                    // 在 Win10 上这里不会报错，只是接口不存在，不做任何处理
                }
            }

            // 6. Setup frame processing
            bool frameReceived = false;
            bool captureSuccess = false;
            std::mutex frameMutex;
            std::condition_variable frameCondition;


            framePool.FrameArrived([&](auto const& sender, auto const& args)
            {
                time = QDateTime::currentDateTime();
                qDebug()<<"3"<<time.currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
                auto frame = sender.TryGetNextFrame();
                if (frame)
                {
                    try
                    {

                        // Get the Direct3D11 surface
                        auto surface = frame.Surface();

                        // Get the underlying D3D11 texture using interop
                        com_ptr<ID3D11Texture2D> texture;
                        auto dxgiInterfaceAccess = surface.as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
                        winrt::check_hresult(dxgiInterfaceAccess->GetInterface(IID_PPV_ARGS(&texture)));

                        // Get texture description
                        D3D11_TEXTURE2D_DESC desc;
                        texture->GetDesc(&desc);

                        // Create staging texture for CPU access
                        desc.Usage = D3D11_USAGE_STAGING;
                        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                        desc.BindFlags = 0;
                        desc.MiscFlags = 0;

                        com_ptr<ID3D11Texture2D> stagingTexture;
                        winrt::check_hresult(d3d11Device->CreateTexture2D(&desc, nullptr, stagingTexture.put()));

                        // Copy to staging texture
                        com_ptr<ID3D11DeviceContext> context;
                        d3d11Device->GetImmediateContext(context.put());
                        context->CopyResource(stagingTexture.get(), texture.get());

                        // Map the texture
                        D3D11_MAPPED_SUBRESOURCE mappedResource;
                        winrt::check_hresult(context->Map(stagingTexture.get(), 0, D3D11_MAP_READ, 0, &mappedResource));

                        // Create bitmap data
                        uint32_t dataSize = mappedResource.RowPitch * desc.Height;
                        std::vector<uint8_t> bitmapData(dataSize);
                        memcpy(bitmapData.data(), mappedResource.pData, dataSize);

                        context->Unmap(stagingTexture.get(), 0);
                        QImage img((const uchar*)mappedResource.pData,desc.Width,desc.Height,mappedResource.RowPitch,QImage::Format_ARGB32);

                        // D3D11 输出通常是 BGRA8，所以要做通道交换
                        //img.save("123123123123123.bmp");
                        //QImage converted = img.rgbSwapped();

                        // 保存为 PNG
                        bool ok = img.save(outputPath);
                        if (!ok) {
                            //qWarning() << "Failed to save screenshot to" << outputPath;
                        }

                        // Signal completion
                        {
                            std::lock_guard<std::mutex> lock(frameMutex);
                            frameReceived = true;
                        }
                        frameCondition.notify_one();
                    }
                    catch (hresult_error const& ex)
                    {
                        {
                            std::lock_guard<std::mutex> lock(frameMutex);
                            frameReceived = true;
                        }
                        frameCondition.notify_one();
                    }
                }
                else
                {

                }
            });

            // 7. Start capture
            session.StartCapture();

            // Wait for frame with timeout and message pumping

            {
                std::unique_lock<std::mutex> lock(frameMutex);
                auto start = std::chrono::steady_clock::now();
                auto timeout = std::chrono::seconds(10);

                while (!frameReceived && (std::chrono::steady_clock::now() - start) < timeout)
                {
                    lock.unlock();

                    // Pump messages to allow Windows events to be processed
                    MSG msg;
                    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
                    {
                        TranslateMessage(&msg);
                        DispatchMessage(&msg);
                    }

                    // Small sleep to prevent busy waiting
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));

                    lock.lock();
                }

                if (frameReceived)
                {
                }
                else
                {
                    return ErrorCode::TimeoutError;
                }
            }

            // Cleanup
            session.Close();
            framePool.Close();


            return captureSuccess ? ErrorCode::Success : ErrorCode::FileSaveFailed;
        }
        catch (hresult_error const& ex)
        {
            return ErrorCode::CaptureSessionFailed;
        }
        catch (std::exception const& ex)
        {
            std::wstring errorMsg(ex.what(), ex.what() + strlen(ex.what()));
            return ErrorCode::UnknownError;
        }
        catch (...)
        {
            return ErrorCode::UnknownError;
        }
    }

    // Helper function to setup capture session
    std::tuple<winrt::Windows::Graphics::Capture::GraphicsCaptureSession, 
               winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool,
               winrt::com_ptr<ID3D11Device>> SetupCaptureSession(bool hideBorder, bool hideCursor)
    {
        // 1. Create D3D11 Device
		QDateTime time = QDateTime::currentDateTime();
        qDebug()<<"1"<<time.currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
        auto d3d11Device = ScreenCapture::CreateD3DDevice();
        auto direct3DDevice = ScreenCapture::CreateDirect3DDeviceFromD3D11Device(d3d11Device);

        // 2. Create capture item for primary monitor
        auto captureItem = ScreenCapture::CreateCaptureItemForMonitor();
		time = QDateTime::currentDateTime();
        qDebug()<<"2"<<time.currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
        // 3. Create Direct3D11CaptureFramePool
        auto framePool = Direct3D11CaptureFramePool::Create(
            direct3DDevice,
            DirectXPixelFormat::B8G8R8A8UIntNormalized,
            1,
            captureItem.Size()
        );
		time = QDateTime::currentDateTime();
        qDebug()<<"3"<<time.currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
        // 4. Create capture session
        auto session = framePool.CreateCaptureSession(captureItem);
		time = QDateTime::currentDateTime();
        qDebug()<<"4"<<time.currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
        // 5. Configure capture session options
        if (hideCursor)
        {
            session.IsCursorCaptureEnabled(false);
        }

        if (hideBorder)
        {
            try
            {
                session.IsBorderRequired(false);
            }
            catch (...)
            {
                // Ignore if not supported
            }
        }

        return std::make_tuple(session, framePool, d3d11Device);
    }

    ErrorCode ScreenCapture::InternalCaptureToMemory(ScreenData &outputBuffer, bool hideBorder, bool hideCursor)
    {
        try
        {

            auto [session, framePool, d3d11Device] = SetupCaptureSession(hideBorder, hideCursor);

            // 6. Setup frame processing
            bool frameReceived = false;
            bool captureSuccess = false;
            std::mutex frameMutex;
            std::condition_variable frameCondition;
			QDateTime time = QDateTime::currentDateTime();
			qDebug()<<"4"<<time.currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");

            framePool.FrameArrived([&](auto const& sender, auto const& args)
            {
			QDateTime time = QDateTime::currentDateTime();
			qDebug()<<"5"<<time.currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
                auto frame = sender.TryGetNextFrame();
                if (frame)
                {
                    try
                    {

                        // Get the Direct3D11 surface
                        auto surface = frame.Surface();

                        // Get the underlying D3D11 texture using interop
                        com_ptr<ID3D11Texture2D> texture;
                        auto dxgiInterfaceAccess = surface.as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
                        winrt::check_hresult(dxgiInterfaceAccess->GetInterface(IID_PPV_ARGS(&texture)));

                        // Get texture description
                        D3D11_TEXTURE2D_DESC desc;
                        texture->GetDesc(&desc);



                        // Create staging texture for CPU access
                        desc.Usage = D3D11_USAGE_STAGING;
                        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                        desc.BindFlags = 0;
                        desc.MiscFlags = 0;

                        com_ptr<ID3D11Texture2D> stagingTexture;
                        winrt::check_hresult(d3d11Device->CreateTexture2D(&desc, nullptr, stagingTexture.put()));

                        // Copy to staging texture
                        com_ptr<ID3D11DeviceContext> context;
                        d3d11Device->GetImmediateContext(context.put());
                        context->CopyResource(stagingTexture.get(), texture.get());

                        // Map the texture
                        D3D11_MAPPED_SUBRESOURCE mappedResource;
                        winrt::check_hresult(context->Map(stagingTexture.get(), 0, D3D11_MAP_READ, 0, &mappedResource));

                        // Create bitmap data
                        uint32_t dataSize = mappedResource.RowPitch * desc.Height;
                        std::shared_ptr<std::vector<uint8_t>> buffer = std::make_shared<std::vector<uint8_t>>(dataSize);
                        memcpy(buffer->data(), mappedResource.pData, dataSize);
                        outputBuffer.data = buffer;
                        outputBuffer.RowPitch = mappedResource.RowPitch;
                        outputBuffer.des = desc;
                        context->Unmap(stagingTexture.get(), 0);
                        captureSuccess = true;
                        // Signal completion
                        {
                            std::lock_guard<std::mutex> lock(frameMutex);
                            frameReceived = true;
                        }
                        frameCondition.notify_one();
                    }
                    catch (hresult_error const& ex)
                    {
                        {
                            std::lock_guard<std::mutex> lock(frameMutex);
                            frameReceived = true;
                        }
                        frameCondition.notify_one();
                    }
                }
                else
                {
                }
            });

            // 7. Start capture
            session.StartCapture();

            // Wait for frame with timeout and message pumping
            {
                std::unique_lock<std::mutex> lock(frameMutex);
                auto start = std::chrono::steady_clock::now();
                auto timeout = std::chrono::seconds(10);

                while (!frameReceived && (std::chrono::steady_clock::now() - start) < timeout)
                {
                    lock.unlock();

                    // Pump messages to allow Windows events to be processed
                    MSG msg;
                    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
                    {
                        TranslateMessage(&msg);
                        DispatchMessage(&msg);
                    }

                    // Small sleep to prevent busy waiting
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));

                    lock.lock();
                }

                if (frameReceived)
                {
                }
                else
                {
                    return ErrorCode::TimeoutError;
                }
            }

            // Cleanup
            session.Close();
            framePool.Close();


            return captureSuccess ? ErrorCode::Success : ErrorCode::TextureProcessingFailed;
        }
        catch (hresult_error const& ex)
        {

            return ErrorCode::CaptureSessionFailed;
        }
        catch (std::exception const& ex)
        {
            std::wstring errorMsg(ex.what(), ex.what() + strlen(ex.what()));
            return ErrorCode::UnknownError;
        }
        catch (...)
        {
            return ErrorCode::UnknownError;
        }
    }
} // namespace ScreenCaptureCore

// ═══════════════════════════════════════════════════════════
// C wrapper: 供外部 C/C++ 代码调用，不依赖 WinRT 头文件
// ═══════════════════════════════════════════════════════════
bool captureScreenToFile(const wchar_t* path)
{
    ScreenCaptureCore::ScreenCapture cap;
    return cap.CaptureToFile(QString::fromWCharArray(path)) == ScreenCaptureCore::ErrorCode::Success;
}
