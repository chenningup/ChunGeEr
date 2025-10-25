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
    using namespace std::chrono;

    // 1. Create D3D11 Device
    com_ptr<ID3D11Device> d3d11Device = ScreenCaptureCore::ScreenCapture::CreateD3DDevice();
    IDirect3DDevice direct3DDevice = ScreenCaptureCore::ScreenCapture::CreateDirect3DDeviceFromD3D11Device(d3d11Device);

    // 2. Create capture item for primary monitor
    GraphicsCaptureItem captureItem = ScreenCaptureCore::ScreenCapture::CreateCaptureItemForMonitor();

    // 3. Create frame pool
    auto framePool = Direct3D11CaptureFramePool::Create(
        direct3DDevice,
        DirectXPixelFormat::B8G8R8A8UIntNormalized,
        1,
        captureItem.Size());

    // 4. Create session
    auto session = framePool.CreateCaptureSession(captureItem);

    // 5. Create immediate context
    com_ptr<ID3D11DeviceContext> context;
    d3d11Device->GetImmediateContext(context.put());

    // 6. Prepare triple staging textures + queries
    struct StagingSlot {
        com_ptr<ID3D11Texture2D> tex;
        com_ptr<ID3D11Query> query;
        bool pending = false;
    };
    std::vector<StagingSlot> StagingSlots(3);
    int current = 0;

    D3D11_TEXTURE2D_DESC sharedDesc{};
    bool initialized = false;

    std::condition_variable frameCondition;
    static QDateTime lastTime;

    framePool.FrameArrived([&](auto const& sender, auto const&) {
        if(!lastTime.isValid()) lastTime = QDateTime::currentDateTime();
        else {
            QDateTime cur = QDateTime::currentDateTime();
            if(lastTime.msecsTo(cur) < 30) return;
            lastTime = cur;
        }

        auto frame = sender.TryGetNextFrame();
        if (!frame) return;

        try {
            auto surface = frame.Surface();
            // Get the underlying D3D11 texture using interop
            // Get the underlying D3D11 texture using interop
            com_ptr<ID3D11Texture2D> texture;

            // 注意这里用 ABI::Windows::... 而不是 winrt::Windows::...
            auto dxgiInterfaceAccess = surface.as<
                ::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();

            winrt::check_hresult(dxgiInterfaceAccess->GetInterface(IID_PPV_ARGS(&texture)));



            D3D11_TEXTURE2D_DESC desc;
            texture->GetDesc(&desc);

            // 初始化 staging slots
            if (!initialized) {
                sharedDesc = desc;
                sharedDesc.Usage = D3D11_USAGE_STAGING;
                sharedDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                sharedDesc.BindFlags = 0;
                sharedDesc.MiscFlags = 0;

                for (auto& s : StagingSlots) {
                    winrt::check_hresult(d3d11Device->CreateTexture2D(&sharedDesc, nullptr, s.tex.put()));
                    D3D11_QUERY_DESC qd{};
                    qd.Query = D3D11_QUERY_EVENT;
                    winrt::check_hresult(d3d11Device->CreateQuery(&qd, s.query.put()));
                }
                initialized = true;
            }

            // 当前写入槽
            auto& writeSlot = StagingSlots[current];
            context->CopyResource(writeSlot.tex.get(), texture.get());
            context->End(writeSlot.query.get());
            writeSlot.pending = true;

            // 读取上一个槽
            int readIndex = (current + 1) % StagingSlots.size();
            auto& readSlot = StagingSlots[readIndex];

            if (readSlot.pending) {
                HRESULT hr = context->GetData(readSlot.query.get(), nullptr, 0, 0);
                if (hr == S_OK) {
                    // GPU 拷贝完成，可以 Map
                    D3D11_MAPPED_SUBRESOURCE mapped{};
                    hr = context->Map(readSlot.tex.get(), 0, D3D11_MAP_READ, 0, &mapped);
                    if (SUCCEEDED(hr)) {
                        QDateTime t1 = QDateTime::currentDateTime();

                        uint32_t dataSize = mapped.RowPitch * sharedDesc.Height;
                        auto buffer = std::make_shared<std::vector<uint8_t>>(dataSize);
                        memcpy(buffer->data(), mapped.pData, dataSize);

                        QDateTime t2 = QDateTime::currentDateTime();
                        ScreenData output;
                        output.data = buffer;
                        output.RowPitch = mapped.RowPitch;
                        output.des = sharedDesc;
                        context->Unmap(readSlot.tex.get(), 0);
                        readSlot.pending = false;

                        QDateTime t3 = QDateTime::currentDateTime();
                        //qDebug() << "Memcpy:" << t1.msecsTo(t2) << "Total:" << t1.msecsTo(t3);

                        emit capturedScreen(output);
                    }
                }
            }

            current = (current + 1) % StagingSlots.size();

        } catch (hresult_error const& ex) {
            qWarning() << "Capture error:" << QString::fromWCharArray(ex.message().c_str());
        }
    });

    // 7. Start capture
    session.StartCapture();

    // 8. Message pump
    while (isCapture) {
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        QThread::msleep(30);
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
