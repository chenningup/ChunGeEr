#include "screencapturemanager.h"
#include <QDateTime>
#include "ScreenCapture.h"
ScreenCaptureManager::ScreenCaptureManager(QObject *parent)
    : QThread{parent},isCapture(false)
{}

ScreenCaptureManager::~ScreenCaptureManager()
{
    stopCapture();
}

void ScreenCaptureManager::init()
{
}

ScreenCaptureManager &ScreenCaptureManager::Instance()
{
    static ScreenCaptureManager mScreenCaptureManager;
    return mScreenCaptureManager;
}
static QDateTime lastTime;
void ScreenCaptureManager::run()
{
    // 1. Create D3D11 Device
    QDateTime time = QDateTime::currentDateTime();
    //qDebug() << "1" << time.currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    com_ptr<ID3D11Device> d3d11Device = ScreenCaptureCore::ScreenCapture::CreateD3DDevice();
    IDirect3DDevice direct3DDevice = ScreenCaptureCore::ScreenCapture::CreateDirect3DDeviceFromD3D11Device(d3d11Device);

    // 2. Create capture item for primary monitor
    GraphicsCaptureItem captureItem = ScreenCaptureCore::ScreenCapture::CreateCaptureItemForMonitor();
    time = QDateTime::currentDateTime();
    //qDebug() << "2" << time.currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    // 3. Create Direct3D11CaptureFramePool
    auto framePool = Direct3D11CaptureFramePool::Create(direct3DDevice,
                                                        DirectXPixelFormat::B8G8R8A8UIntNormalized,
                                                        1,
                                                        captureItem.Size());
    time = QDateTime::currentDateTime();
    //qDebug() << "3" << time.currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    // 4. Create capture session
    auto session = framePool.CreateCaptureSession(captureItem);
    time = QDateTime::currentDateTime();
    //qDebug() << "4" << time.currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    // 5. Configure capture session options
    //session.IsCursorCaptureEnabled(false);
    //session.IsBorderRequired(false);
    std::condition_variable frameCondition;
    time = QDateTime::currentDateTime();
    //qDebug() << "4" << time.currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");

    framePool.FrameArrived([&](auto const &sender, auto const &args) {
        if(!lastTime.isValid())
        {
            lastTime = QDateTime::currentDateTime();
        }
        else
        {
            QDateTime curTime = QDateTime::currentDateTime();
            if(lastTime.msecsTo(curTime) < 30)
            {
                return;
            }
            lastTime = curTime;
        }
        //qDebug() << "5" << time.currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz")<<num;
        //num++;
        auto frame = sender.TryGetNextFrame();
        if (frame) {
            try {
                // Get the Direct3D11 surface
                auto surface = frame.Surface();

                // Get the underlying D3D11 texture using interop
                com_ptr<ID3D11Texture2D> texture;
                auto dxgiInterfaceAccess = surface.as<
                    ::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
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
                winrt::check_hresult(
                    d3d11Device->CreateTexture2D(&desc, nullptr, stagingTexture.put()));

                // Copy to staging texture
                com_ptr<ID3D11DeviceContext> context;
                d3d11Device->GetImmediateContext(context.put());
                context->CopyResource(stagingTexture.get(), texture.get());

                // Map the texture
                D3D11_MAPPED_SUBRESOURCE mappedResource;
                winrt::check_hresult(
                    context->Map(stagingTexture.get(), 0, D3D11_MAP_READ, 0, &mappedResource));

                // Create bitmap data
                uint32_t dataSize = mappedResource.RowPitch * desc.Height;
                std::shared_ptr<std::vector<uint8_t>> buffer
                    = std::make_shared<std::vector<uint8_t>>(dataSize);
                memcpy(buffer->data(), mappedResource.pData, dataSize);
                ScreenData outputBuffer;
                outputBuffer.data = buffer;
                outputBuffer.RowPitch = mappedResource.RowPitch;
                outputBuffer.des = desc;
                emit capturedScreen(outputBuffer);
                context->Unmap(stagingTexture.get(), 0);
                frameCondition.notify_one();

            } catch (hresult_error const &ex) {
                {
                }
                frameCondition.notify_one();
            }
        } else {
        }
    });

    // 7. Start capture
    session.StartCapture();

    while(isCapture){
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        QThread::msleep(50);
    }
    // Cleanup
    session.Close();
    framePool.Close();
}
void ScreenCaptureManager::startCapture()
{
    isCapture = true;
    start();
}

void ScreenCaptureManager::stopCapture()
{
    // Cleanup
   // session.Close();
    //framePool.Close();
    isCapture = false;
}

void ScreenCaptureManager::capTureTimerSlot()
{
    start();
}
