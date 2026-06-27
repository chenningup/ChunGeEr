#include "gameitemcapturewidget.h"
#include <QPainter>
#include <QPen>
#include <QMouseEvent>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QDialog>
#include <QDebug>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QScrollArea>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>
#include <QWheelEvent>
#include <QSet>
#include <QMenu>
#include <QSpinBox>
#include <QSettings>
#include <QCoreApplication>
#include <functional>
#include "../Ocr/ocrmnager.h"
#include "../bitmapfontlib.h"
#include "XuLog.h"

// ════════════════════════════════════════════════
// 分类中英文映射
// ════════════════════════════════════════════════
static const QStringList kCatCN = {
    QString::fromUtf8("物品"), QString::fromUtf8("技能"),
    QString::fromUtf8("地点"), QString::fromUtf8("任务"),
    QString::fromUtf8("弹窗"), QString::fromUtf8("角色"),
    QString::fromUtf8("启动"), QString::fromUtf8("等级"),
    QString::fromUtf8("设置"), QString::fromUtf8("图标")
};
static const QStringList kCatEN = {
    "items", "skills", "locations", "quests", "popups", "roles", "login", "levels", "settings", "icons"
};
static QString catCN2EN(const QString &cn) {
    int i = kCatCN.indexOf(cn);
    return i >= 0 ? kCatEN[i] : cn;
}

// ════════════════════════════════════════════════
// CaptureImageLabel 实现
// ════════════════════════════════════════════════

CaptureImageLabel::CaptureImageLabel(QWidget *parent)
    : QLabel(parent), m_selecting(false)
{
    setMouseTracking(true);
    setMinimumSize(320, 240);
    setAlignment(Qt::AlignTop | Qt::AlignLeft);
    setStyleSheet("background-color: #1a1a1a; border: 1px solid #333;");
}

void CaptureImageLabel::setSelectionRect(const QRect &r) { m_selRect = r; update(); }
void CaptureImageLabel::clearSelection() { m_selRect = QRect(); update(); }
bool CaptureImageLabel::hasSelection() const {
    return m_selRect.isValid() && m_selRect.width() > 2 && m_selRect.height() > 2;
}

void CaptureImageLabel::paintEvent(QPaintEvent *event)
{
    QLabel::paintEvent(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    if (m_selRect.isValid() && m_selRect.width() > 0 && m_selRect.height() > 0) {
        QPen pen(QColor(0, 160, 255), 2, Qt::DashLine);
        painter.setPen(pen);
        painter.setBrush(QColor(0, 160, 255, 30));
        painter.drawRect(m_selRect);
    }
}

void CaptureImageLabel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && !pixmap().isNull()) {
        m_startPt = event->pos();
        m_selecting = true;
        m_selRect = QRect(m_startPt, QSize(0, 0));
        update();
    }
}

void CaptureImageLabel::mouseMoveEvent(QMouseEvent *event)
{
    if (m_selecting) {
        QPoint endPt = event->pos();
        if (!pixmap().isNull()) {
            endPt.setX(qBound(0, endPt.x(), pixmap().width() - 1));
            endPt.setY(qBound(0, endPt.y(), pixmap().height() - 1));
        }
        m_selRect = QRect(m_startPt, endPt).normalized();
        update();
    }
}

void CaptureImageLabel::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_selecting) {
        m_selecting = false;
        if (m_selRect.width() < 3 || m_selRect.height() < 3) m_selRect = QRect();
        update();
        emit selectionChanged(m_selRect);
    }
}


// ════════════════════════════════════════════════
// GameItemCaptureWidget 实现
// ════════════════════════════════════════════════

GameItemCaptureWidget::GameItemCaptureWidget(QWidget *parent)
    : QWidget(parent)
    , m_paused(false)
    , m_roiMode(false)
    , m_saveDir("D:/coding/8_mProject/10_ChungGeEr/ChunGeEr/images")
    , m_lastCategoryIdx(0)
    , m_loadingList(false)
    , m_bflTrainingMode(false)
    , m_bflTestMode(false)
    , m_bflColorPickMode(false)
    , m_bflColorBtn(nullptr)
    , m_bflColorLabel(nullptr)
{
    setWindowTitle(QString::fromUtf8("游戏物品截取"));
    resize(1100, 700);

    // 默认颜色过滤器：白字，偏色0x30
    BitmapFontLib::Instance().setColorFilter(BflColorFilter());

    // ════════════════════════════════════════
    // 顶部工具栏
    // ════════════════════════════════════════
    auto *topLayout = new QHBoxLayout();

    m_windowCombo = new QComboBox(this);
    m_windowCombo->addItem(QString::fromUtf8("🎮 游戏窗口"));
    m_windowCombo->addItem(QString::fromUtf8("🚀 启动器"));
    m_windowCombo->setMinimumWidth(120);

    m_pauseBtn = new QPushButton(QString::fromUtf8("⏸ 暂停"), this);
    m_screenshotBtn = new QPushButton(QString::fromUtf8("📷 截图"), this);
    m_testBtn = new QPushButton(QString::fromUtf8("🔍 测试匹配"), this);
    m_ocrBtn = new QPushButton(QString::fromUtf8("📝 OCR识别"), this);
    m_trainBflBtn = new QPushButton(QString::fromUtf8("🔤 字库训练"), this);
    m_testBflBtn = new QPushButton(QString::fromUtf8("🔍 字库测试"), this);
    m_loadBflBtn = new QPushButton(QString::fromUtf8("📂 加载字库"), this);
    m_saveBflBtn = new QPushButton(QString::fromUtf8("💾 保存字库"), this);
    m_dirBtn = new QPushButton(QString::fromUtf8("📁 选择目录"), this);

    m_roiTypeCombo = new QComboBox(this);
    m_roiTypeCombo->addItems({
        QString::fromUtf8("地图名"), QString::fromUtf8("地图坐标"),
        QString::fromUtf8("等级"),
        QString::fromUtf8("技能"), QString::fromUtf8("主线任务"),
        QString::fromUtf8("掉线"), QString::fromUtf8("卡住"),
        QString::fromUtf8("设置区域"), QString::fromUtf8("对手头像"),
        QString::fromUtf8("任务对话区域")
    });
    m_roiTypeCombo->setMinimumWidth(90);
    m_roiTypeCombo->setVisible(false);
    m_roiModifyBtn = new QPushButton(QString::fromUtf8("修改"), this);
    m_roiModifyBtn->setVisible(false);
    m_roiModifyBtn->setStyleSheet("background-color: #3498db; color: white;");

    m_roiBtn = new QPushButton(QString::fromUtf8("🎯 ROI模式"), this);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(QString::fromUtf8("输入物品名称（如：回血丹）"));
    m_nameEdit->setMinimumWidth(180);

    m_screenshotBtn->setEnabled(false);
    m_ocrBtn->setEnabled(false);

    topLayout->addWidget(m_windowCombo);
    topLayout->addWidget(m_pauseBtn);
    topLayout->addWidget(m_screenshotBtn);
    topLayout->addWidget(m_testBtn);
    topLayout->addWidget(m_ocrBtn);
    topLayout->addWidget(m_nameEdit);
    topLayout->addWidget(m_roiTypeCombo);
    topLayout->addWidget(m_roiModifyBtn);
    topLayout->addWidget(m_roiBtn);

    topLayout->addWidget(m_dirBtn);

    topLayout->addStretch();

    // 第二行：点阵字库
    auto *bflLayout = new QHBoxLayout();

    m_bflStatusLabel = new QLabel(QString::fromUtf8("字库: 未加载"), this);
    m_bflStatusLabel->setStyleSheet("color: #888; padding: 0 6px;");
    bflLayout->addWidget(m_bflStatusLabel);
    bflLayout->addWidget(m_loadBflBtn);
    bflLayout->addWidget(m_saveBflBtn);
    bflLayout->addWidget(m_trainBflBtn);
    bflLayout->addWidget(m_testBflBtn);

    // 颜色过滤器
    m_bflColorLabel = new QLabel(QString::fromUtf8("未取色"), this);
    m_bflColorLabel->setStyleSheet("background: #EEE; color: #888; border: 1px solid #999; padding: 0 8px; border-radius: 3px;");
    m_bflColorLabel->setToolTip(QString::fromUtf8("点击🎨取色后在此显示，右键重置"));
    m_bflColorLabel->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_bflColorLabel, &QLabel::customContextMenuRequested, this, [this]() {
        auto &lib = BitmapFontLib::Instance();
        if (lib.colorFilter().isEmpty()) return;
        QMenu menu;
        QAction *clearAction = menu.addAction(QString::fromUtf8("🗑 重置取色"));
        QAction *chosen = menu.exec(QCursor::pos());
        if (chosen == clearAction) {
            BflColorFilter filter;
            lib.setColorFilter(filter);
            m_bflColorLabel->setText(QString::fromUtf8("未取色"));
            m_bflColorLabel->setStyleSheet("background: #EEE; color: #888; border: 1px solid #999; padding: 0 8px; border-radius: 3px;");
            m_bflColorLabel->setToolTip(QString::fromUtf8("点击🎨取色后在此显示，右键重置"));
            infof("[BFL] 取色已重置");
        }
    });
    bflLayout->addWidget(m_bflColorLabel);

    m_bflColorBtn = new QPushButton(QString::fromUtf8("🎨 取色"), this);
    m_bflColorBtn->setToolTip(QString::fromUtf8("点击后在截图上点选文字颜色"));
    m_bflColorBtn->setFixedWidth(80);
    bflLayout->addWidget(m_bflColorBtn);

    m_trainBflBtn->setEnabled(false);
    m_testBflBtn->setEnabled(false);
    m_bflColorBtn->setEnabled(false);

    bflLayout->addStretch();

    // ════════════════════════════════════════
    // 中间区域：左侧分类树 + 右侧游戏画面
    // ════════════════════════════════════════
    auto *midLayout = new QHBoxLayout();

    m_itemTree = new QTreeWidget(this);
    m_itemTree->setMinimumWidth(160);
    m_itemTree->setMaximumWidth(240);
    m_itemTree->setIconSize(QSize(48, 48));
    m_itemTree->setHeaderHidden(true);
    m_itemTree->setIndentation(12);
    m_itemTree->setSelectionMode(QAbstractItemView::MultiSelection);
    m_itemTree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_itemTree->setStyleSheet(
        "QTreeWidget { background-color: #222; border: 1px solid #333; color: #ccc; }"
        "QTreeWidget::item:selected { background-color: #3a3a5a; }");

    auto *scrollArea = new QScrollArea(this);
    m_imageLabel = new CaptureImageLabel();
    m_imageLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    scrollArea->setWidget(m_imageLabel);
    scrollArea->setWidgetResizable(false);
    scrollArea->setStyleSheet("QScrollArea { background-color: #111; border: none; }");

    midLayout->addWidget(m_itemTree);
    midLayout->addWidget(scrollArea, 1);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->addLayout(topLayout);
    rootLayout->addLayout(bflLayout);
    rootLayout->addLayout(midLayout, 1);

    // ── 信号连接 ──
    connect(m_pauseBtn, &QPushButton::clicked, this, &GameItemCaptureWidget::onPauseToggle);
    connect(m_screenshotBtn, &QPushButton::clicked, this, &GameItemCaptureWidget::onScreenshot);
    connect(m_testBtn, &QPushButton::clicked, this, &GameItemCaptureWidget::onTestMatch);
    connect(m_ocrBtn, &QPushButton::clicked, this, &GameItemCaptureWidget::onOcrRecognize);
    connect(m_dirBtn, &QPushButton::clicked, this, &GameItemCaptureWidget::onSelectSaveDir);
    connect(m_roiBtn, &QPushButton::clicked, this, &GameItemCaptureWidget::onRoiModeToggle);
    connect(m_loadBflBtn, &QPushButton::clicked, this, &GameItemCaptureWidget::onFontLibLoad);
    // 修改按钮：清除当前选区重新画
    connect(m_roiModifyBtn, &QPushButton::clicked, this, [this]() {
        if (m_imageLabel) { m_imageLabel->clearSelection(); }
        m_roiModifyBtn->setVisible(false);
    });
    // ROI类型切换时加载已有配置
    connect(m_roiTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (m_roiMode) loadRoiForCurrentSelection();
    });
    connect(m_saveBflBtn, &QPushButton::clicked, this, &GameItemCaptureWidget::onFontLibSave);
    connect(m_trainBflBtn, &QPushButton::clicked, this, &GameItemCaptureWidget::onFontLibTrain);
    connect(m_testBflBtn, &QPushButton::clicked, this, &GameItemCaptureWidget::onFontLibTest);
    connect(m_bflColorBtn, &QPushButton::clicked, this, [this]() {
        infof("进入取色模式: 请在截图上点击文字颜色");
        if (!m_paused) {
            m_paused = true;
            m_captureTimer->stop();
            m_pauseBtn->setText(QString::fromUtf8("▶ 继续"));
        }
        m_bflColorPickMode = true;
        m_imageLabel->setCursor(Qt::CrossCursor);
    });
    // 自动加载默认字库
    {
        QString defaultBfl = QDir::cleanPath(m_saveDir + QString::fromUtf8("/../datang_font.bfl"));
        if (QFileInfo::exists(defaultBfl) && BitmapFontLib::Instance().load(defaultBfl)) {
            m_bflPath = defaultBfl;
            updateBflStatus();
        }
    }
    // 统一框选：字库训练/测试优先于ROI
    connect(m_imageLabel, &CaptureImageLabel::selectionChanged, this, [this](const QRect &sel) {
        // 取色模式优先（单点点击，rect可能很小）
        if (m_bflColorPickMode) {
            m_bflColorPickMode = false;
            m_imageLabel->setCursor(Qt::ArrowCursor);
            if (sel.isValid()) {
                handleBflColorPick(sel);
            }
            return;
        }

        if (!sel.isValid() || sel.width() < 3 || sel.height() < 3) return;

        if (m_bflTrainingMode) {
            m_bflTrainingMode = false;
            handleBflTrainSelection(sel);
            return;
        }
        if (m_bflTestMode) {
            m_bflTestMode = false;
            handleBflTestSelection(sel);
            return;
        }

        if (!m_roiMode) return;
        static const QStringList roiKeys = {"Location", "MapCoord", "Level", "Skills", "MainQuest", "Disconnect", "Stopped", "SettingsPanel", "TargetAvatar", "DialogBtn"};
        int idx = m_roiTypeCombo->currentIndex();
        if (idx < 0 || idx >= roiKeys.size()) return;

        QSize pixSize = m_currentPixmap.size();
        double sx = (double)m_currentFrame.cols / pixSize.width();
        double sy = (double)m_currentFrame.rows / pixSize.height();
        int rx = qBound(0, (int)(sel.x() * sx), m_currentFrame.cols - 1);
        int ry = qBound(0, (int)(sel.y() * sy), m_currentFrame.rows - 1);

        // 将全窗口坐标转为客户区坐标（减去标题栏高度）
        HWND _hwnd = findGameWindow();
        if (_hwnd) {
            RECT _wr; POINT _pt = {0,0};
            GetWindowRect(_hwnd, &_wr);
            ClientToScreen(_hwnd, &_pt);
            int _barH = _pt.y - _wr.top;
            if (_barH > 0) ry = qMax(0, ry - _barH);
        }

        int rw = qBound(1, (int)(sel.width() * sx), m_currentFrame.cols - rx);
        int rh = qBound(1, (int)(sel.height() * sy), m_currentFrame.rows - ry);

        saveRoi(roiKeys[idx], QRect(rx, ry, rw, rh));
        m_roiModifyBtn->setVisible(true);
        if (m_imageLabel) { m_imageLabel->setSelectionRect(sel); }
    });
    connect(m_itemTree, &QTreeWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        QTreeWidgetItem *item = m_itemTree->itemAt(pos);
        if (!item || item->data(0, Qt::UserRole).toString() == "__CATEGORY__") return;
        QString filePath = item->data(0, Qt::UserRole).toString();
        if (filePath.isEmpty()) return;
        QMenu menu;
        QAction *delAction = menu.addAction(QString::fromUtf8("\U0001f5d1 \u5220\u9664"));
        QAction *chosen = menu.exec(m_itemTree->viewport()->mapToGlobal(pos));
        if (chosen == delAction) {
            if (QFile::remove(filePath)) {
                m_templateCache.remove(filePath);
                delete item;
                rebuildActivePaths();
            } else {
                QMessageBox::warning(this, QString::fromUtf8("\u9519\u8bef"),
                    QString::fromUtf8("\u5220\u9664\u5931\u8d25: ") + filePath);
            }
        }
    });
    connect(m_itemTree, &QTreeWidget::itemSelectionChanged, this, [this]() {
        if (!m_loadingList) rebuildActivePaths();
    });

    m_captureTimer = new QTimer(this);
    connect(m_captureTimer, &QTimer::timeout, this, &GameItemCaptureWidget::onCaptureTick);
    m_captureTimer->start(100);

    loadImagesFromDir();
}

GameItemCaptureWidget::~GameItemCaptureWidget()
{
    m_captureTimer->stop();
}

// ════════════════════════════════════════════════
// 找目标窗口：根据下拉框选择启动器或游戏窗口
// ════════════════════════════════════════════════
HWND GameItemCaptureWidget::findGameWindow()
{
    bool wantLauncher = (m_windowCombo && m_windowCombo->currentIndex() == 1);

    if (wantLauncher) {
        // 启动器：Qt5152QWindowIcon 类，标题含"大唐无双"
        HWND hwnd = FindWindowW(L"Qt5152QWindowIcon", nullptr);
        if (hwnd) {
            wchar_t title[256];
            GetWindowTextW(hwnd, title, 256);
            if (wcsstr(title, L"\u5927\u5510") || wcsstr(title, L"\u65e0\u53cc")) return hwnd;
        }
        return hwnd;
    }

    // 游戏窗口：标题精确/模糊匹配"大唐无双"
    HWND hwnd = FindWindowW(nullptr, L"\u5927\u5510\u65e0\u53cc\u516c\u6d4b - \u4e03\u4fa0\u4e94\u4e49 (4.0.58:1041281  1.0.5:1039767)");
    if (hwnd) return hwnd;
    HWND h = FindWindowW(nullptr, nullptr);
    while (h) {
        wchar_t title[256];
        GetWindowTextW(h, title, 256);
        if (wcsstr(title, L"\u5927\u5510\u65e0\u53cc")) return h;
        h = GetWindow(h, GW_HWNDNEXT);
    }
    return nullptr;
}

// ════════════════════════════════════════════════
// GDI 截取游戏窗口 → cv::Mat
// ════════════════════════════════════════════════
cv::Mat GameItemCaptureWidget::captureGameWindow()
{
    HWND hwnd = findGameWindow();
    if (!hwnd) return cv::Mat();

    RECT rect;
    if (!GetWindowRect(hwnd, &rect)) return cv::Mat();
    int w = rect.right - rect.left;
    int h = rect.bottom - rect.top;
    if (w <= 0 || h <= 0) return cv::Mat();

    HDC hdcScreen = GetDC(nullptr);
    if (!hdcScreen) return cv::Mat();

    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hbm = CreateCompatibleBitmap(hdcScreen, w, h);
    HGDIOBJ old = SelectObject(hdcMem, hbm);
    BitBlt(hdcMem, 0, 0, w, h, hdcScreen, rect.left, rect.top, SRCCOPY);

    cv::Mat img(h, w, CV_8UC4);
    BITMAPINFOHEADER bi = {};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = w;
    bi.biHeight = -h;
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;
    GetDIBits(hdcMem, hbm, 0, h, img.data, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

    cv::Mat bgr;
    cv::cvtColor(img, bgr, cv::COLOR_BGRA2BGR);

    SelectObject(hdcMem, old);
    DeleteObject(hbm);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
    return bgr;
}

// ════════════════════════════════════════════════
// cv::Mat → QImage
// ════════════════════════════════════════════════
QImage GameItemCaptureWidget::matToQImage(const cv::Mat &mat)
{
    if (mat.empty()) return QImage();
    if (mat.type() == CV_8UC3) {
        return QImage(mat.data, mat.cols, mat.rows, (int)mat.step, QImage::Format_BGR888).copy();
    }
    if (mat.type() == CV_8UC4) {
        return QImage(mat.data, mat.cols, mat.rows, (int)mat.step, QImage::Format_ARGB32).copy();
    }
    if (mat.type() == CV_8UC1) {
        // 单通道灰度图 → 转 BGR 再转 QImage，保证二值化预览可见
        cv::Mat bgr;
        cv::cvtColor(mat, bgr, cv::COLOR_GRAY2BGR);
        return QImage(bgr.data, bgr.cols, bgr.rows, (int)bgr.step, QImage::Format_BGR888).copy();
    }
    return QImage();
}

// ════════════════════════════════════════════════
// 定时截取 → 刷新显示（选中图片则实时匹配，用缓存）
// ════════════════════════════════════════════════
void GameItemCaptureWidget::onCaptureTick()
{
    if (m_paused) return;

    m_currentFrame = captureGameWindow();
    if (m_currentFrame.empty()) {
        m_imageLabel->setText(QString::fromUtf8("⚠ 未找到游戏窗口"));
        m_imageLabel->setStyleSheet("background-color: #1a1a1a; color: #f55; font-size: 16px; qproperty-alignment: AlignCenter;");
        return;
    }

    cv::Mat displayFrame = m_currentFrame.clone();

    // ── 实时模板匹配（用缓存，不读磁盘）──
    if (!m_selectedPaths.isEmpty()) {
        for (const QString &path : m_selectedPaths) {
            auto it = m_templateCache.find(path);
            cv::Mat templ;
            if (it != m_templateCache.end()) {
                templ = it.value();
            } else {
                templ = cv::imread(path.toLocal8Bit().toStdString());
                if (!templ.empty()) m_templateCache[path] = templ;
            }
            if (templ.empty()) continue;
            if (templ.cols > displayFrame.cols || templ.rows > displayFrame.rows) continue;

            cv::Mat result;
            cv::matchTemplate(displayFrame, templ, result, cv::TM_CCOEFF_NORMED);

            double maxVal;
            cv::Point maxLoc;
            cv::minMaxLoc(result, nullptr, &maxVal, nullptr, &maxLoc);

            if (maxVal < 0.5) continue;

            QFileInfo fi(path);
            cv::rectangle(displayFrame, maxLoc,
                          cv::Point(maxLoc.x + templ.cols, maxLoc.y + templ.rows),
                          cv::Scalar(0, 255, 0), 2);
            cv::putText(displayFrame,
                        std::to_string(maxVal).substr(0, 4),
                        cv::Point(maxLoc.x, maxLoc.y - 5),
                        cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(0, 255, 0), 1);
        }
    }

    QImage qimg = matToQImage(displayFrame);
    m_currentPixmap = QPixmap::fromImage(qimg);
    m_imageLabel->setPixmap(m_currentPixmap);
    m_imageLabel->resize(m_currentPixmap.size());
    m_imageLabel->setStyleSheet("background-color: #1a1a1a; border: 1px solid #333;");
}

// ════════════════════════════════════════════════
// 暂停 / 继续
// ════════════════════════════════════════════════
void GameItemCaptureWidget::onPauseToggle()
{
    m_paused = !m_paused;
    if (m_paused) {
        m_pauseBtn->setText(QString::fromUtf8("▶ 继续"));
        m_screenshotBtn->setEnabled(true);
        m_ocrBtn->setEnabled(true);
        m_trainBflBtn->setEnabled(true);
        m_testBflBtn->setEnabled(BitmapFontLib::Instance().charCount() > 0);
        m_imageLabel->setCursor(Qt::CrossCursor);
    } else {
        m_pauseBtn->setText(QString::fromUtf8("⏸ 暂停"));
        m_screenshotBtn->setEnabled(false);
        m_ocrBtn->setEnabled(false);
        m_trainBflBtn->setEnabled(false);
        m_testBflBtn->setEnabled(false);
        if (m_imageLabel) { m_imageLabel->clearSelection(); }
        m_imageLabel->setCursor(Qt::ArrowCursor);
    }
}

// ════════════════════════════════════════════════
// 截图：保存框选区域
// ════════════════════════════════════════════════
void GameItemCaptureWidget::onScreenshot()
{
    if (!m_paused || m_currentFrame.empty()) {
        QMessageBox::warning(this, QString::fromUtf8("提示"),
                             QString::fromUtf8("请先暂停画面，再框选区域！"));
        return;
    }

    QRect sel = m_imageLabel->selectionRect();
    if (!m_imageLabel->hasSelection()) {
        QMessageBox::warning(this, QString::fromUtf8("提示"),
                             QString::fromUtf8("请先在画面上拖拽框选要截取的区域！"));
        return;
    }

    // 名称
    QString itemName = m_nameEdit->text().trimmed();
    if (itemName.isEmpty()) {
        bool ok;
        itemName = QInputDialog::getText(this,
            QString::fromUtf8("物品名称"),
            QString::fromUtf8("请输入物品名称："),
            QLineEdit::Normal, "", &ok);
        if (!ok || itemName.isEmpty()) return;
        m_nameEdit->setText(itemName);
    }

    // 选择分类
    bool ok;
    QString categoryCN = QInputDialog::getItem(this,
        QString::fromUtf8("保存分类"),
        QString::fromUtf8("保存到哪个分类？"),
        kCatCN, m_lastCategoryIdx, false, &ok);
    if (!ok) return;
    m_lastCategoryIdx = kCatCN.indexOf(categoryCN);
    QString categoryEN = catCN2EN(categoryCN);

    // 裁剪（pixmap 1:1 显示，坐标直接对应）
    QSize pixSize = m_currentPixmap.size();
    double sx = (double)m_currentFrame.cols / pixSize.width();
    double sy = (double)m_currentFrame.rows / pixSize.height();
    int rx = qBound(0, (int)(sel.x() * sx), m_currentFrame.cols - 1);
    int ry = qBound(0, (int)(sel.y() * sy), m_currentFrame.rows - 1);
    int rw = qBound(1, (int)(sel.width() * sx), m_currentFrame.cols - rx);
    int rh = qBound(1, (int)(sel.height() * sy), m_currentFrame.rows - ry);

    cv::Rect roi(rx, ry, rw, rh);
    cv::Mat cropMat = m_currentFrame(roi).clone();

    // 保存到分类子目录
    QString savePath = m_saveDir + "/" + categoryEN;
    QDir().mkpath(savePath);
    QString filePath = savePath + "/" + itemName + ".png";
    if (QFile::exists(filePath)) {
        filePath = savePath + "/" + itemName + "_" +
                   QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".png";
    }
    cv::imwrite(filePath.toLocal8Bit().toStdString(), cropMat);

    QPixmap thumb = QPixmap::fromImage(matToQImage(cropMat));
    qDebug() << "[ItemCap] 保存截图:" << filePath << "尺寸:" << rw << "x" << rh;

    addItemToList(itemName, thumb, filePath);

        if (m_imageLabel) { m_imageLabel->clearSelection(); }
    m_nameEdit->clear();
}

// ════════════════════════════════════════════════
// 测试：cv::matchTemplate 在所有选中图上
// ════════════════════════════════════════════════
void GameItemCaptureWidget::onTestMatch()
{
    rebuildActivePaths();
    if (m_selectedPaths.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("提示"),
                             QString::fromUtf8("请先在左侧列表选中图片！"));
        return;
    }

    // 用当前帧（暂停或实时）做匹配，不覆盖 m_currentPixmap
    cv::Mat frame = m_currentFrame.empty() ? captureGameWindow() : m_currentFrame.clone();
    if (frame.empty()) {
        QMessageBox::warning(this, QString::fromUtf8("错误"),
                             QString::fromUtf8("无法截取游戏画面！"));
        return;
    }

    QStringList results;
    cv::Mat displayFrame = frame.clone();

    for (const QString &path : m_selectedPaths) {
        QFileInfo fi(path);
        cv::Mat templ = cv::imread(path.toLocal8Bit().toStdString());
        if (templ.empty()) {
            results.append(QString("[%1] ❌ 无法读取").arg(fi.baseName()));
            continue;
        }
        if (templ.cols > frame.cols || templ.rows > frame.rows) {
            results.append(QString("[%1] ❌ 模板比画面大").arg(fi.baseName()));
            continue;
        }

        cv::Mat result;
        cv::matchTemplate(frame, templ, result, cv::TM_CCOEFF_NORMED);

        double maxVal;
        cv::Point maxLoc;
        cv::minMaxLoc(result, nullptr, &maxVal, nullptr, &maxLoc);

        QString emoji = maxVal > 0.8 ? QString::fromUtf8("✅") :
                         maxVal > 0.5 ? QString::fromUtf8("⚠") : QString::fromUtf8("❌");
        results.append(QString("%1 [%2] 置信度:%3 位置:(%4,%5)")
                           .arg(emoji, fi.baseName())
                           .arg(maxVal, 0, 'f', 4)
                           .arg(maxLoc.x).arg(maxLoc.y));

        cv::rectangle(displayFrame,
                      maxLoc,
                      cv::Point(maxLoc.x + templ.cols, maxLoc.y + templ.rows),
                      cv::Scalar(0, 255, 0), 2);
        cv::putText(displayFrame,
                    std::to_string(maxVal).substr(0, 4),
                    cv::Point(maxLoc.x, maxLoc.y - 5),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);
    }

    // 显示带标记的画面（copy 不影响后续截图坐标）
    QImage qimg = matToQImage(displayFrame);
    QPixmap resultPixmap = QPixmap::fromImage(qimg);
    m_imageLabel->setPixmap(resultPixmap);
    m_imageLabel->resize(resultPixmap.size());

    QMessageBox::information(this,
        QString::fromUtf8("多模板匹配 (%1张)").arg(m_selectedPaths.size()),
        results.join("\n"));
}

// ════════════════════════════════════════════════
// OCR识别：对框选区域运行OCR
// ════════════════════════════════════════════════
void GameItemCaptureWidget::onOcrRecognize()
{
    if (!m_paused || m_currentFrame.empty()) {
        QMessageBox::warning(this, QString::fromUtf8("提示"),
                             QString::fromUtf8("请先暂停画面！"));
        return;
    }

    QRect sel = m_imageLabel->selectionRect();
    if (!m_imageLabel->hasSelection()) {
        QMessageBox::warning(this, QString::fromUtf8("提示"),
                             QString::fromUtf8("请先在画面上拖拽框选要识别的文字区域！"));
        return;
    }

    QSize pixSize = m_currentPixmap.size();
    double sx = (double)m_currentFrame.cols / pixSize.width();
    double sy = (double)m_currentFrame.rows / pixSize.height();

    int rx = qBound(0, (int)(sel.x() * sx), m_currentFrame.cols - 1);
    int ry = qBound(0, (int)(sel.y() * sy), m_currentFrame.rows - 1);
    int rw = qBound(1, (int)(sel.width() * sx), m_currentFrame.cols - rx);
    int rh = qBound(1, (int)(sel.height() * sy), m_currentFrame.rows - ry);

    cv::Rect roi(rx, ry, rw, rh);
    cv::Mat crop = m_currentFrame(roi).clone();

    if (!OcrMnager::Instance().isReady()) {
        QMessageBox::warning(this, QString::fromUtf8("错误"),
                             QString::fromUtf8("OCR引擎未就绪！"));
        return;
    }

    QString text = OcrMnager::Instance().identify(crop);
    QString engName = OcrMnager::Instance().engineName();

    if (text.trimmed().isEmpty()) {
        QMessageBox::information(this,
            QString::fromUtf8("OCR识别 (%1)").arg(engName),
            QString::fromUtf8("未识别到文字。\n\n提示：可尝试切换OCR引擎（主界面下拉框）。"));
    } else {
        QMessageBox::information(this,
            QString::fromUtf8("OCR识别 (%1)").arg(engName),
            QString::fromUtf8("识别结果：\n\n%1").arg(text));
    }
}

// ════════════════════════════════════════════════
// 选择保存根目录
// ════════════════════════════════════════════════
void GameItemCaptureWidget::onSelectSaveDir()
{
    QString dir = QFileDialog::getExistingDirectory(this,
        QString::fromUtf8("选择图片根目录"), m_saveDir);
    if (!dir.isEmpty()) {
        m_saveDir = dir;
        m_dirBtn->setToolTip(m_saveDir);
        m_itemTree->clear();
        m_selectedPaths.clear();
        m_templateCache.clear();
        loadImagesFromDir();
    }
}

// ════════════════════════════════════════════════
// 扫描所有分类子目录，加载到树形列表
// ════════════════════════════════════════════════
void GameItemCaptureWidget::loadImagesFromDir()
{
    m_loadingList = true;
    m_itemTree->clear();
    QStringList filters = {"*.png", "*.jpg", "*.jpeg", "*.bmp"};

    for (int i = 0; i < kCatEN.size(); i++) {
        auto *catItem = new QTreeWidgetItem();
        catItem->setText(0, kCatCN[i] + QString(" (%1)").arg(0));
        catItem->setFlags(catItem->flags() & ~Qt::ItemIsSelectable);
        catItem->setData(0, Qt::UserRole, "__CATEGORY__");
        m_itemTree->addTopLevelItem(catItem);
        catItem->setExpanded(true);

        QString dir = m_saveDir + "/" + kCatEN[i];
        QDir d(dir);
        QFileInfoList files = d.entryInfoList(filters, QDir::Files, QDir::Name);

        for (const QFileInfo &fi : files) {
            QPixmap pix(fi.absoluteFilePath());
            if (pix.isNull()) continue;
            auto *imgItem = new QTreeWidgetItem();
            imgItem->setText(0, fi.baseName());
            imgItem->setIcon(0, QIcon(pix.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
            imgItem->setData(0, Qt::UserRole, fi.absoluteFilePath());
            imgItem->setToolTip(0, fi.absoluteFilePath());
            catItem->addChild(imgItem);
        }
        if (catItem->childCount() == 0) {
            auto *empty = new QTreeWidgetItem();
            empty->setText(0, QString::fromUtf8("(空)"));
            empty->setFlags(empty->flags() & ~Qt::ItemIsSelectable);
            catItem->addChild(empty);
        }
        catItem->setText(0, kCatCN[i] + QString(" (%1)").arg(catItem->childCount()));
    }
    m_loadingList = false;
}

// ════════════════════════════════════════════════
// 重建选中路径 + 刷新模板缓存
// ════════════════════════════════════════════════
void GameItemCaptureWidget::rebuildActivePaths()
{
    QStringList oldPaths = m_selectedPaths;
    m_selectedPaths.clear();

    for (auto *item : m_itemTree->selectedItems()) {
        QString path = item->data(0, Qt::UserRole).toString();
        if (path.isEmpty() || path == "__CATEGORY__") continue;
        m_selectedPaths.append(path);
    }

    // 选中集合变了才重建缓存
    QSet<QString> oldSet(oldPaths.begin(), oldPaths.end());
    QSet<QString> newSet(m_selectedPaths.begin(), m_selectedPaths.end());
    if (oldSet != newSet) {
        // 删掉不再选中的模板缓存
        for (auto it = m_templateCache.begin(); it != m_templateCache.end(); ) {
            if (!newSet.contains(it.key())) it = m_templateCache.erase(it);
            else ++it;
        }
        // 预加载新模板
        for (const QString &path : m_selectedPaths) {
            if (!m_templateCache.contains(path)) {
                cv::Mat m = cv::imread(path.toLocal8Bit().toStdString());
                if (!m.empty()) m_templateCache[path] = m;
            }
        }
    }
}

// ════════════════════════════════════════════════
// 向树中添加截图
// ════════════════════════════════════════════════
void GameItemCaptureWidget::addItemToList(const QString &name,
                                          const QPixmap &pixmap,
                                          const QString &filePath)
{
    QFileInfo fi(filePath);
    QString catEN = fi.dir().dirName();
    int catIdx = kCatEN.indexOf(catEN);
    if (catIdx < 0) return;

    QTreeWidgetItem *parent = nullptr;
    for (int i = 0; i < m_itemTree->topLevelItemCount(); i++) {
        if (m_itemTree->topLevelItem(i)->text(0).startsWith(kCatCN[catIdx])) {
            parent = m_itemTree->topLevelItem(i);
            break;
        }
    }
    if (!parent) return;

    // 清除"(空)"占位
    if (parent->childCount() == 1 && parent->child(0)->text(0) == QString::fromUtf8("(空)")) {
        delete parent->takeChild(0);
    }

    auto *imgItem = new QTreeWidgetItem();
    imgItem->setText(0, name);
    imgItem->setIcon(0, QIcon(pixmap.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    imgItem->setData(0, Qt::UserRole, filePath);
    imgItem->setToolTip(0, filePath);
    parent->insertChild(0, imgItem);

    // 更新分类计数
    parent->setText(0, kCatCN[catIdx] + QString(" (%1)").arg(parent->childCount()));
}

// ════════════════════════════════════════════════
// ROI模式切换
// ════════════════════════════════════════════════
void GameItemCaptureWidget::onRoiModeToggle()
{
    m_roiMode = !m_roiMode;
    if (m_roiMode) {
        m_roiBtn->setText(QString::fromUtf8("🎯 退出ROI"));
        m_roiBtn->setStyleSheet("background-color: #e67e22; color: white;");
        m_roiTypeCombo->setVisible(true);
        m_screenshotBtn->setVisible(false);
        m_nameEdit->setVisible(false);
        m_itemTree->setVisible(false);
        // 自动暂停
        if (!m_paused) onPauseToggle();
        loadRoiForCurrentSelection();
    } else {
        m_roiBtn->setText(QString::fromUtf8("🎯 ROI模式"));
        m_roiBtn->setStyleSheet("");
        m_roiTypeCombo->setVisible(false);
        m_screenshotBtn->setVisible(true);
        m_nameEdit->setVisible(true);
        m_itemTree->setVisible(true);
        m_roiModifyBtn->setVisible(false);
        if (m_imageLabel) { m_imageLabel->clearSelection(); }
    }
}

// ════════════════════════════════════════════════
// 保存ROI到config.ini（窗口相对坐标）
// ════════════════════════════════════════════════
void GameItemCaptureWidget::saveRoi(const QString &roiKey, const QRect &rect)
{
    // ROI是窗口相对坐标，存到config.ini
    QString configPath = QCoreApplication::applicationDirPath() + "/config.ini";
    QSettings settings(configPath, QSettings::IniFormat);
    settings.beginGroup("ROIs");
    settings.setValue(roiKey, QString("%1,%2,%3,%4")
        .arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height()));
    settings.endGroup();
    settings.sync();

    QMessageBox::information(this,
        QString::fromUtf8("ROI已保存"),
        QString::fromUtf8("%1: (%2,%3) %4x%5")
            .arg(roiKey).arg(rect.x()).arg(rect.y())
            .arg(rect.width()).arg(rect.height()));
}

// ════════════════════════════════════════════════
// 加载并显示当前ROI类型已有配置
// ════════════════════════════════════════════════
void GameItemCaptureWidget::loadRoiForCurrentSelection()
{
    if (!m_roiMode || m_currentFrame.empty() || m_currentPixmap.isNull()) {
        m_roiModifyBtn->setVisible(false);
        return;
    }

    int idx = m_roiTypeCombo->currentIndex();
    static const QStringList roiKeys = {"Location", "MapCoord", "Level", "Skills", "MainQuest", "Disconnect", "Stopped", "SettingsPanel", "TargetAvatar", "DialogBtn"};
    if (idx < 0 || idx >= roiKeys.size()) {
        m_roiModifyBtn->setVisible(false);
        return;
    }

    QString configPath = QCoreApplication::applicationDirPath() + "/config.ini";
    QSettings s(configPath, QSettings::IniFormat);
    QString key = roiKeys[idx];
    if (!s.contains("ROIs/" + key)) {
        if (m_imageLabel) { m_imageLabel->clearSelection(); }
        m_roiModifyBtn->setVisible(false);
        return;
    }

    QStringList parts = s.value("ROIs/" + key).toString().split(",");
    if (parts.size() != 4) {
        if (m_imageLabel) { m_imageLabel->clearSelection(); }
        m_roiModifyBtn->setVisible(false);
        return;
    }

    // ROI存的是客户区相对坐标，显示时需转回窗口坐标（加回标题栏）
    QRect clientRect(parts[0].toInt(), parts[1].toInt(),
                     parts[2].toInt(), parts[3].toInt());
    int barH = 0;
    HWND _hwnd = findGameWindow();
    if (_hwnd) {
        RECT _wr; POINT _pt = {0,0};
        GetWindowRect(_hwnd, &_wr);
        ClientToScreen(_hwnd, &_pt);
        barH = _pt.y - _wr.top;
    }
    QRect windowRect(clientRect.x(), clientRect.y() + barH,
                      clientRect.width(), clientRect.height());
    QSize pixSize = m_currentPixmap.size();
    double sx = (double)pixSize.width() / m_currentFrame.cols;
    double sy = (double)pixSize.height() / m_currentFrame.rows;

    QRect labelRect(
        qRound(windowRect.x() * sx),
        qRound(windowRect.y() * sy),
        qRound(windowRect.width() * sx),
        qRound(windowRect.height() * sy)
    );

    infof("[ROI] 加载已有配置: {}={},{},{},{}  barH={}  label({},{},{},{})",
          key.toStdString(), clientRect.x(), clientRect.y(),
          clientRect.width(), clientRect.height(), barH,
          labelRect.x(), labelRect.y(),
          labelRect.width(), labelRect.height());

    if (m_imageLabel) { m_imageLabel->setSelectionRect(labelRect); }
    m_roiModifyBtn->setVisible(true);
}

// ════════════════════════════════════════════════
// 点阵字库 - 刷新状态标签
// ════════════════════════════════════════════════
void GameItemCaptureWidget::updateBflStatus()
{
    auto &lib = BitmapFontLib::Instance();
    QString fname = m_bflPath.isEmpty()
        ? QString::fromUtf8("未加载") : QFileInfo(m_bflPath).fileName();
    m_bflStatusLabel->setText(
        QString::fromUtf8("字库: %1 | %2\u5b57").arg(fname).arg(lib.charCount()));
    m_bflStatusLabel->setStyleSheet(
        lib.charCount() > 0 ? "color: #5f5; padding: 0 6px;" : "color: #888; padding: 0 6px;");
    bool ready = lib.charCount() > 0;
    m_trainBflBtn->setEnabled(true);
    m_testBflBtn->setEnabled(ready);
    if (m_bflColorBtn) m_bflColorBtn->setEnabled(true);
}

// ════════════════════════════════════════════════
// 加载字库
// ════════════════════════════════════════════════
void GameItemCaptureWidget::onFontLibLoad()
{
    QString path = QFileDialog::getOpenFileName(this,
        QString::fromUtf8("\u52a0\u8f7d\u5b57\u5e93"),
        m_saveDir, QString::fromUtf8("BitmapFontLib (*.bfl)"));
    if (path.isEmpty()) return;

    auto &lib = BitmapFontLib::Instance();
    if (lib.load(path)) {
        m_bflPath = path;
        updateBflStatus();
        QMessageBox::information(this, QString::fromUtf8("\u52a0\u8f7d\u6210\u529f"),
            QString::fromUtf8("\u5df2\u52a0\u8f7d %1 \u4e2a\u5b57\u7b26")
                .arg(lib.charCount()));
    } else {
        QMessageBox::warning(this, QString::fromUtf8("\u52a0\u8f7d\u5931\u8d25"),
            QString::fromUtf8("\u65e0\u6cd5\u8bfb\u53d6\u5b57\u5e93\u6587\u4ef6\uff01"));
    }
}

// ════════════════════════════════════════════════
// 保存字库
// ════════════════════════════════════════════════
void GameItemCaptureWidget::onFontLibSave()
{
    auto &lib = BitmapFontLib::Instance();
    if (lib.charCount() == 0) {
        QMessageBox::warning(this, QString::fromUtf8("\u63d0\u793a"),
            QString::fromUtf8("\u5b57\u5e93\u4e3a\u7a7a\uff0c\u8bf7\u5148\u8bad\u7ec3\uff01"));
        return;
    }
    QString defaultName = m_bflPath.isEmpty()
        ? QDir::cleanPath(m_saveDir + QString::fromUtf8("/../datang_font.bfl")) : m_bflPath;
    QString path = QFileDialog::getSaveFileName(this,
        QString::fromUtf8("\u4fdd\u5b58\u5b57\u5e93"),
        defaultName, QString::fromUtf8("BitmapFontLib (*.bfl)"));
    if (path.isEmpty()) return;

    if (lib.save(path)) {
        m_bflPath = path;
        updateBflStatus();

        // 同时拷贝到源码目录（防止Release编译时被CMake全覆盖）
        QString srcDir = QDir::cleanPath(QCoreApplication::applicationDirPath() + "/../..");
        QString srcPath = srcDir + "/datang_font.bfl";
        if (QFile::copy(path, srcPath)) {
            qDebug() << "[BFL] 已拷贝到源码目录:" << srcPath;
        } else {
            // 目标已存在则覆盖
            QFile::remove(srcPath);
            QFile::copy(path, srcPath);
            qDebug() << "[BFL] 已覆盖源码目录:" << srcPath;
        }
    }
}

// ════════════════════════════════════════════════
// 进入训练模式
// ════════════════════════════════════════════════
void GameItemCaptureWidget::onFontLibTrain()
{
    if (BitmapFontLib::Instance().charCount() == 0) {
        QMessageBox::information(this, QString::fromUtf8("\u63d0\u793a"),
            QString::fromUtf8("\u5b57\u5e93\u4e3a\u7a7a\uff0c\u7b2c\u4e00\u6b21\u8bad\u7ec3\u4f1a\u81ea\u52a8\u5efa\u5e93"));
    }
    if (!m_paused) onPauseToggle();
    m_bflTrainingMode = true;
    m_imageLabel->setCursor(Qt::CrossCursor);
    QMessageBox::information(this, QString::fromUtf8("\u5b57\u5e93\u8bad\u7ec3"),
        QString::fromUtf8("\u8bf7\u5728\u753b\u9762\u4e0a\u6846\u9009\u4e00\u884c\u6587\u5b57\u533a\u57df\n"
                          "\u6846\u9009\u540e\u4f1a\u81ea\u52a8\u5207\u5b57\uff0c\u7136\u540e\u8f93\u5165\u5bf9\u5e94\u6587\u5b57\u5373\u53ef\u8bad\u7ec3"));
}

// ════════════════════════════════════════════════
// 进入测试模式
// ════════════════════════════════════════════════
void GameItemCaptureWidget::onFontLibTest()
{
    if (!m_paused) onPauseToggle();
    m_bflTestMode = true;
    m_imageLabel->setCursor(Qt::CrossCursor);
    QMessageBox::information(this, QString::fromUtf8("\u5b57\u5e93\u6d4b\u8bd5"),
        QString::fromUtf8("\u8bf7\u5728\u753b\u9762\u4e0a\u6846\u9009\u4e00\u884c\u6587\u5b57\u533a\u57df\n"
                          "\u6846\u9009\u540e\u5c06\u81ea\u52a8\u8bc6\u522b\u5e76\u663e\u793a\u7ed3\u679c"));
}

// ════════════════════════════════════════════════
// 训练：框选 → binarize → 切字 → 标注 → addSample
// ════════════════════════════════════════════════
void GameItemCaptureWidget::handleBflTrainSelection(const QRect &sel)
{
    if (m_currentFrame.empty()) return;

    // pixmap → frame 坐标
    QSize pixSize = m_currentPixmap.size();
    double fsx = (double)m_currentFrame.cols / pixSize.width();
    double fsy = (double)m_currentFrame.rows / pixSize.height();
    int frx = qBound(0, (int)(sel.x() * fsx), m_currentFrame.cols - 1);
    int fry = qBound(0, (int)(sel.y() * fsy), m_currentFrame.rows - 1);
    int frw = qBound(1, (int)(sel.width() * fsx), m_currentFrame.cols - frx);
    int frh = qBound(1, (int)(sel.height() * fsy), m_currentFrame.rows - fry);

    cv::Mat roi = m_currentFrame(cv::Rect(frx, fry, frw, frh)).clone();
    infof("[BFL] train sel: pixSize={}x{} scale=({:.3f},{:.3f}) roi=({},{},{},{})",
        pixSize.width(), pixSize.height(), fsx, fsy, frx, fry, frw, frh);

    // ═══ 步骤1: 交互取色（可缩放+像素蒙层+颜色列表） ═══
    QDialog colorDlg(this);
    colorDlg.setWindowTitle(QString::fromUtf8("取色 - 滚轮缩放 / 点击像素取色"));
    colorDlg.resize(960, 580);
    colorDlg.setMinimumSize(800, 450);

    auto *outerLayout = new QVBoxLayout(&colorDlg);
    outerLayout->setContentsMargins(8, 8, 8, 8);

    // ── 主区域: 左颜色列表 | 右可缩放图+二值化预览 ──
    auto *mainLayout = new QHBoxLayout();

    // === 左: 颜色列表面板 ===
    auto *leftPanel = new QVBoxLayout();
    leftPanel->setSpacing(4);

    auto *leftTitle = new QLabel(QString::fromUtf8("已选颜色"), &colorDlg);
    leftTitle->setStyleSheet("font-weight: bold; color: #aaa; font-size: 12px;");
    leftPanel->addWidget(leftTitle);

    // 滚动区域装颜色行
    QScrollArea *colorScroll = new QScrollArea(&colorDlg);
    colorScroll->setWidgetResizable(true);
    colorScroll->setFixedWidth(220);
    colorScroll->setStyleSheet("QScrollArea { background: #1a1a1a; border: 1px solid #444; }");
    QWidget *colorListWidget = new QWidget(&colorDlg);
    colorListWidget->setStyleSheet("background: #1a1a1a;");
    QVBoxLayout *colorListLayout = new QVBoxLayout(colorListWidget);
    colorListLayout->setSpacing(2);
    colorListLayout->setContentsMargins(4, 4, 4, 4);
    colorListLayout->addStretch();
    colorScroll->setWidget(colorListWidget);
    leftPanel->addWidget(colorScroll, 1);

    // 取色计数
    auto *pixelCountLabel = new QLabel(QString::fromUtf8("0 个像素"), &colorDlg);
    pixelCountLabel->setStyleSheet("color: #666; font-size: 11px;");
    leftPanel->addWidget(pixelCountLabel);

    // 重置按钮
    auto *resetBtn = new QPushButton(QString::fromUtf8("🗑 全部重置"), &colorDlg);
    resetBtn->setFixedHeight(28);
    leftPanel->addWidget(resetBtn);

    mainLayout->addLayout(leftPanel);

    // === 右: 可缩放原图 + 二值化预览 ===
    auto *rightPanel = new QVBoxLayout();
    rightPanel->setSpacing(6);

    auto *imgTitle = new QLabel(QString::fromUtf8("👆 滚轮缩放 | 点击像素取色 | 再点取消"), &colorDlg);
    imgTitle->setStyleSheet("font-weight: bold; color: #ccc; font-size: 12px;");
    rightPanel->addWidget(imgTitle);

    // 偏色范围调节
    auto *biasLayout = new QHBoxLayout();
    auto *biasLabel = new QLabel(QString::fromUtf8("偏色范围: ±"), &colorDlg);
    biasLabel->setStyleSheet("color: #ccc; font-size: 12px;");
    auto *biasSpin = new QSpinBox(&colorDlg);
    biasSpin->setRange(1, 128);
    biasSpin->setValue(48);
    biasSpin->setFixedWidth(60);
    biasSpin->setStyleSheet("color: #ddd; background: #333; border: 1px solid #555;");
    biasLayout->addWidget(biasLabel);
    biasLayout->addWidget(biasSpin);
    biasLayout->addStretch();
    rightPanel->addLayout(biasLayout);

    // QGraphicsView 可缩放
    QGraphicsScene *scene = new QGraphicsScene(&colorDlg);
    QGraphicsView *view = new QGraphicsView(scene, &colorDlg);
    view->setDragMode(QGraphicsView::ScrollHandDrag);
    view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    view->setRenderHint(QPainter::Antialiasing, false);
    view->setRenderHint(QPainter::SmoothPixmapTransform, false);  // 放大时保持像素锐利
    view->setStyleSheet("background: #000; border: 2px solid #555;");
    view->setMouseTracking(true);

    QImage origImg = matToQImage(roi);
    QPixmap origPix = QPixmap::fromImage(origImg);
    QGraphicsPixmapItem *pixItem = scene->addPixmap(origPix);
    view->fitInView(pixItem, Qt::KeepAspectRatio);
    rightPanel->addWidget(view, 1);

    // 二值化预览
    auto *binTitle = new QLabel(QString::fromUtf8("二值化预览"), &colorDlg);
    binTitle->setStyleSheet("color: #aaa; font-size: 11px;");
    rightPanel->addWidget(binTitle);

    QLabel *binLabel = new QLabel(&colorDlg);
    binLabel->setAlignment(Qt::AlignCenter);
    binLabel->setStyleSheet("background: #111; border: 1px solid #444;");
    binLabel->setMinimumHeight(80);
    binLabel->setScaledContents(false);
    rightPanel->addWidget(binLabel);

    mainLayout->addLayout(rightPanel, 1);
    outerLayout->addLayout(mainLayout, 1);

    // ── 底部按钮 ──
    auto *btnLayout = new QHBoxLayout();
    auto *okBtn = new QPushButton(QString::fromUtf8("✅ 确认"), &colorDlg);
    auto *cancelBtn = new QPushButton(QString::fromUtf8("取消"), &colorDlg);
    btnLayout->addStretch();
    btnLayout->addWidget(okBtn);
    btnLayout->addWidget(cancelBtn);
    outerLayout->addLayout(btnLayout);

    // ════════════════════════════════════════
    // 状态管理 — 不继承全局颜色，从零开始
    // 全局 filter 由工具栏"取色"按钮管理，可能已累积上万种颜色
    // （±48偏色×几千种=全图命中→二值化全白）
    // ════════════════════════════════════════
    auto &lib = BitmapFontLib::Instance();
    BflColorFilter tempFilter;  // 空——重新取色
    QSet<QPoint> selectedPixels;
    QVector<QGraphicsItem*> overlayItems;

    // 更新全部: 颜色列表、二值化预览、蒙层
    std::function<void()> updateAll;
    updateAll = [&]() {
        // tempFilter 由点击事件直接管理，此处只读

        // ── 更新颜色列表（每行：色块 + 颜色+偏色范围 + ✕） ──
        while (colorListLayout->count() > 1) {
            QLayoutItem *item = colorListLayout->takeAt(0);
            if (item->widget()) {
                delete item->widget();
            } else if (item->layout()) {
                // 先取出子 widget 用 deleteLater 延迟删除（避免信号回调中删除发送者）
                QLayout *subLayout = item->layout();
                while (subLayout->count() > 0) {
                    QLayoutItem *subItem = subLayout->takeAt(0);
                    if (subItem->widget()) subItem->widget()->deleteLater();
                    delete subItem;
                }
            }
            delete item;
        }

        infof("[BFL] updateAll: rebuilding color list, points.size()={}", tempFilter.points.size());
        for (int ci = 0; ci < tempFilter.points.size(); ci++) {
            const auto &pt = tempFilter.points[ci];
            QColor c = pt.color;
            infof("[BFL] updateAll: row[{}] RGB({},{},{})", ci, c.red(), c.green(), c.blue());
            QColor bias = pt.bias;
            QString hex = QString("#%1%2%3")
                .arg(c.red(), 2, 16, QChar('0'))
                .arg(c.green(), 2, 16, QChar('0'))
                .arg(c.blue(), 2, 16, QChar('0'));

            auto *row = new QHBoxLayout();
            row->setSpacing(4);

            auto *swatch = new QLabel(&colorDlg);
            swatch->setFixedSize(18, 18);
            swatch->setStyleSheet(QString("background: %1; border: 1px solid #555;").arg(hex));
            row->addWidget(swatch);

            auto *info = new QLabel(
                QString("%1 ±%2").arg(hex).arg(bias.red()), &colorDlg);
            info->setStyleSheet("color: #ccc; font-size: 11px;");
            row->addWidget(info, 1);

            auto *delBtn = new QPushButton(QString::fromUtf8("✕"), &colorDlg);
            delBtn->setFixedSize(20, 20);
            delBtn->setStyleSheet("QPushButton { color: #f66; background: transparent; border: none; font-size: 12px; } QPushButton:hover { color: #f00; }");
            // 用颜色值做唯一标识，避免下标过期
            QColor capturedColor = pt.color;
            connect(delBtn, &QPushButton::clicked, [&, capturedColor, delBtn]() {
                // 检查颜色是否还存在（可能已被删除，旧按钮残留事件）
                bool stillExists = false;
                for (int ci2 = 0; ci2 < tempFilter.points.size(); ci2++) {
                    if (tempFilter.points[ci2].color == capturedColor) {
                        stillExists = true;
                        break;
                    }
                }
                if (!stillExists) return;

                infof("[BFL] delBtn: deleting color RGB({},{},{})", capturedColor.red(), capturedColor.green(), capturedColor.blue());
                // 按颜色值查找并删除
                for (int ci2 = 0; ci2 < tempFilter.points.size(); ci2++) {
                    if (tempFilter.points[ci2].color == capturedColor) {
                        tempFilter.points.removeAt(ci2);
                        break;
                    }
                }
                // 不能在 clicked 回调里直接 updateAll（会 delete delBtn 自身导致崩溃）
                // 延迟到下一个事件循环再重建
                QTimer::singleShot(0, [&]() {
                    // 基于剩余颜色重扫蒙层
                    selectedPixels.clear();
                    for (int ci2 = 0; ci2 < tempFilter.points.size(); ci2++) {
                        const auto &pt2 = tempFilter.points[ci2];
                        int cr2 = pt2.color.red(), cg2 = pt2.color.green(), cb2 = pt2.color.blue();
                        int br2 = pt2.bias.red();
                        for (int y = 0; y < roi.rows; y++) {
                            for (int x = 0; x < roi.cols; x++) {
                                int r, g, b;
                                if (roi.channels() == 4) {
                                    cv::Vec4b bg = roi.at<cv::Vec4b>(y, x);
                                    r = bg[2]; g = bg[1]; b = bg[0];
                                } else if (roi.channels() == 3) {
                                    cv::Vec3b bg = roi.at<cv::Vec3b>(y, x);
                                    r = bg[2]; g = bg[1]; b = bg[0];
                                } else {
                                    r = g = b = roi.at<uint8_t>(y, x);
                                }
                                if (abs(r - cr2) <= br2 && abs(g - cg2) <= br2 && abs(b - cb2) <= br2) {
                                    selectedPixels.insert(QPoint(x, y));
                                }
                            }
                        }
                    }
                    updateAll();
                });
            });
            row->addWidget(delBtn);

            colorListLayout->insertLayout(colorListLayout->count() - 1, row);
        }

        pixelCountLabel->setText(QString::fromUtf8("%1 个像素").arg(selectedPixels.size()));

        // ── 更新二值化预览 ──
        {
            BflColorFilter savedFilter = lib.colorFilter();
            lib.setColorFilter(tempFilter);
            cv::Mat bin = lib.binarize(roi);
            lib.setColorFilter(savedFilter);

            infof("[BFL] preview: points={} selected={} bin={}x{} empty={}",
                  tempFilter.points.size(), selectedPixels.size(), bin.cols, bin.rows, bin.empty());

            if (!bin.empty()) {
                QImage binImg = matToQImage(bin);
                infof("[BFL] preview: QImage={}x{} fmt={}", binImg.width(), binImg.height(), (int)binImg.format());
                int targetH = qMin(120, qMax(40, roi.rows * 6));
                QPixmap scaled = QPixmap::fromImage(binImg).scaledToHeight(targetH, Qt::FastTransformation);
                infof("[BFL] preview: scaled={}x{} isNull={}", scaled.width(), scaled.height(), scaled.isNull());
                binLabel->setPixmap(scaled);
                binLabel->setMinimumHeight(targetH);
                binLabel->setMinimumWidth(scaled.width());
            } else {
                infof("[BFL] bin preview: EMPTY binarize result");
                binLabel->clear();
            }
        }

        // ── 更新蒙层（遮罩图替代逐个rect） ──
        for (auto *item : overlayItems) scene->removeItem(item);
        qDeleteAll(overlayItems);
        overlayItems.clear();
        if (!selectedPixels.isEmpty()) {
            // 创建半透明遮罩图像
            QImage overlay(roi.cols, roi.rows, QImage::Format_ARGB32_Premultiplied);
            overlay.fill(Qt::transparent);
            QColor markerColor(255, 80, 80, 120);
            for (const QPoint &p : selectedPixels) {
                overlay.setPixelColor(p.x(), p.y(), markerColor);
            }
            QPixmap overlayPix = QPixmap::fromImage(overlay);
            auto *ovItem = scene->addPixmap(overlayPix);
            ovItem->setZValue(1);
            overlayItems.append(ovItem);
        }
    };

    // ── 事件过滤器: 滚轮缩放 + 鼠标点击取色 ──
    class ViewEventFilter : public QObject {
    public:
        QGraphicsView *view;
        std::function<void(int, int)> onClick;  // (imgX, imgY)

        ViewEventFilter(QGraphicsView *v, std::function<void(int,int)> cb)
            : view(v), onClick(cb) {}

        bool eventFilter(QObject *obj, QEvent *event) override {
            if (event->type() == QEvent::Wheel) {
                QWheelEvent *we = static_cast<QWheelEvent*>(event);
                double factor = we->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
                view->scale(factor, factor);
                return true;
            }
            if (event->type() == QEvent::MouseButtonPress) {
                QMouseEvent *me = static_cast<QMouseEvent*>(event);
                if (me->button() == Qt::LeftButton) {
                    QPointF sp = view->mapToScene(me->pos());
                    int px = qBound(0, (int)sp.x(), (int)(view->scene()->width() - 1));
                    int py = qBound(0, (int)sp.y(), (int)(view->scene()->height() - 1));
                    onClick(px, py);
                    return true;
                }
            }
            return false;
        }
    };

    ViewEventFilter viewFilter(view, [&](int px, int py) {
        // 获取点击像素颜色
        QColor clickedColor;
        if (roi.channels() == 4) {
            cv::Vec4b bgra = roi.at<cv::Vec4b>(py, px);
            clickedColor = QColor(bgra[2], bgra[1], bgra[0]);
        } else if (roi.channels() == 3) {
            cv::Vec3b bgr = roi.at<cv::Vec3b>(py, px);
            clickedColor = QColor(bgr[2], bgr[1], bgr[0]);
        } else {
            uint8_t gray = roi.at<uint8_t>(py, px);
            clickedColor = QColor(gray, gray, gray);
        }

        // 颜色范围：从 SpinBox 读取偏色
        int bias = biasSpin->value();
        int cr = clickedColor.red(), cg = clickedColor.green(), cb = clickedColor.blue();

        // 已蒙层的像素点击无反应，取消蒙层只能通过左侧颜色列表✕按钮
        QPoint clickedPt(px, py);
        if (selectedPixels.contains(clickedPt)) {
            return;
        }

        {
            // 扫描整张ROI，颜色范围内的像素全选上
            for (int y = 0; y < roi.rows; y++) {
                for (int x = 0; x < roi.cols; x++) {
                    int r, g, b;
                    if (roi.channels() == 4) {
                        cv::Vec4b bg = roi.at<cv::Vec4b>(y, x);
                        r = bg[2]; g = bg[1]; b = bg[0];
                    } else if (roi.channels() == 3) {
                        cv::Vec3b bg = roi.at<cv::Vec3b>(y, x);
                        r = bg[2]; g = bg[1]; b = bg[0];
                    } else {
                        r = g = b = roi.at<uint8_t>(y, x);
                    }
                    if (abs(r - cr) <= bias && abs(g - cg) <= bias && abs(b - cb) <= bias) {
                        selectedPixels.insert(QPoint(x, y));
                    }
                }
            }
            tempFilter.add(clickedColor, QColor(bias, bias, bias));
        }
        infof("[BFL] onClick: before updateAll, points={}", tempFilter.points.size());
        updateAll();
        infof("[BFL] onClick: after updateAll");
    });
    view->viewport()->installEventFilter(&viewFilter);

    // ── 初始显示 ──
    updateAll();

    // ── 重置按钮 ──
    connect(resetBtn, &QPushButton::clicked, [&]() {
        selectedPixels.clear();
        updateAll();
    });

    connect(okBtn, &QPushButton::clicked, &colorDlg, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, &colorDlg, &QDialog::reject);

    if (colorDlg.exec() != QDialog::Accepted) {
        if (m_imageLabel) { m_imageLabel->clearSelection(); }
        return;
    }

    // 确认: 清空旧颜色，只保留本次训练的临时颜色
    lib.colorFilterRef().clear();
    for (const auto &pt : tempFilter.points) {
        lib.colorFilterRef().add(pt.color, pt.bias);
    }

    // ═══ 步骤2: 最终二值化（整行作为一个整体训练） ═══
    cv::Mat binary = lib.binarize(roi);

    if (binary.empty() || cv::countNonZero(binary) == 0) {
        QMessageBox::warning(this, QString::fromUtf8("二值化失败"),
            QString::fromUtf8("未检测到前景像素！请确认:\n"
                              "1. 已取色(至少点一个文字像素)\n"
                              "2. 框选的确实是文字区域"));
        if (m_imageLabel) { m_imageLabel->clearSelection(); }
        return;
    }

    // 裁掉上下空白行
    {
        cv::Mat rowSums;
        cv::reduce(binary, rowSums, 1, cv::REDUCE_SUM, CV_32S);
        int top = 0, bottom = binary.rows - 1;
        for (; top < binary.rows && rowSums.at<int>(top) == 0; top++);
        for (; bottom >= 0 && rowSums.at<int>(bottom) == 0; bottom--);
        if (top <= bottom)
            binary = binary(cv::Rect(0, top, binary.cols, bottom - top + 1)).clone();
    }

    // 裁掉左右空白列
    {
        cv::Mat colSums;
        cv::reduce(binary, colSums, 0, cv::REDUCE_SUM, CV_32S);
        int left = 0, right = binary.cols - 1;
        for (; left < binary.cols && colSums.at<int>(left) == 0; left++);
        for (; right >= 0 && colSums.at<int>(right) == 0; right--);
        if (left <= right)
            binary = binary(cv::Rect(left, 0, right - left + 1, binary.rows)).clone();
    }

    // ═══ 步骤3: 预览 + 输入文字 ═══
    QDialog dlg(this);
    dlg.setWindowTitle(QString::fromUtf8("字库训练"));
    auto *dl = new QVBoxLayout(&dlg);

    auto *pl = new QLabel(&dlg);
    pl->setPixmap(QPixmap::fromImage(matToQImage(roi)));
    pl->setAlignment(Qt::AlignCenter);
    pl->setStyleSheet("background-color: #000;");
    dl->addWidget(pl);

    auto *bi = new QLabel(&dlg);
    bi->setPixmap(QPixmap::fromImage(matToQImage(binary)));
    bi->setAlignment(Qt::AlignCenter);
    bi->setStyleSheet("background-color: #000;");
    dl->addWidget(bi);

    auto *te = new QLineEdit(&dlg);
    te->setPlaceholderText(QString::fromUtf8("输入这行文字（如：前往洛阳）"));
    dl->addWidget(te);

    auto *bl = new QHBoxLayout();
    auto *ok = new QPushButton(QString::fromUtf8("训练"), &dlg);
    auto *cancel = new QPushButton(QString::fromUtf8("取消"), &dlg);
    bl->addStretch();
    bl->addWidget(ok);
    bl->addWidget(cancel);
    dl->addLayout(bl);

    connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) {
        if (m_imageLabel) { m_imageLabel->clearSelection(); }
        return;
    }

    QString text = te->text().trimmed();
        if (m_imageLabel) { if (text.isEmpty()) { m_imageLabel->clearSelection(); return; } }

    // 整行作为一个整体训练，charName = 用户输入的整行文字
    lib.addChar(text.toUtf8().toStdString(), binary);

    updateBflStatus();
    QMessageBox::information(this, QString::fromUtf8("训练完成"),
        QString::fromUtf8("已训练整行: \"%1\" (%2x%3)").arg(text).arg(binary.cols).arg(binary.rows));

        if (m_imageLabel) { m_imageLabel->clearSelection(); }
}

// 测试：框选 → binarize → 切字 → recognize → 显示
// ════════════════════════════════════════════════
void GameItemCaptureWidget::handleBflTestSelection(const QRect &sel)
{
    if (m_currentFrame.empty()) return;

    QSize pixSize = m_currentPixmap.size();
    double fsx = (double)m_currentFrame.cols / pixSize.width();
    double fsy = (double)m_currentFrame.rows / pixSize.height();
    int frx = qBound(0, (int)(sel.x() * fsx), m_currentFrame.cols - 1);
    int fry = qBound(0, (int)(sel.y() * fsy), m_currentFrame.rows - 1);
    int frw = qBound(1, (int)(sel.width() * fsx), m_currentFrame.cols - frx);
    int frh = qBound(1, (int)(sel.height() * fsy), m_currentFrame.rows - fry);

    cv::Mat roi = m_currentFrame(cv::Rect(frx, fry, frw, frh)).clone();
    infof("[BFL-Test] ROI: {}x{} at ({},{})", frw, frh, frx, fry);

    auto &lib = BitmapFontLib::Instance();
    infof("[BFL-Test] colorFilter points={}, glyphs={}",
          lib.colorFilter().points.size(), lib.charCount());

    // findString 现在接收彩色图，每个字形用自己的colorFilter做binarize
    auto results = lib.findString(roi, 0.85);
    infof("[BFL-Test] findString(0.85): {} matches", results.size());
    for (size_t i = 0; i < results.size() && i < 10; i++) {
        infof("[BFL-Test]   match[{}]: char='{}' sim={:.3f} at ({},{}) {}x{}",
              i, results[i].charName, results[i].similarity,
              results[i].x, results[i].y, results[i].width, results[i].height);
    }
    // 如果0.85没结果，试试更低阈值
    if (results.empty()) {
        auto resultsLoose = lib.findString(roi, 0.5);
        infof("[BFL-Test] findString(0.5): {} matches (loose)", resultsLoose.size());
        for (size_t i = 0; i < resultsLoose.size() && i < 10; i++) {
            infof("[BFL-Test]   loose[{}]: char='{}' sim={:.3f} at ({},{}) {}x{}",
                  i, resultsLoose[i].charName, resultsLoose[i].similarity,
                  resultsLoose[i].x, resultsLoose[i].y, resultsLoose[i].width, resultsLoose[i].height);
        }
        results = resultsLoose;
    }

    if (results.empty()) {
        QMessageBox::information(this, QString::fromUtf8("\u8bc6\u522b\u7ed3\u679c"),
            QString::fromUtf8("\u672a\u8bc6\u522b\u5230\u5b57\u7b26"));
        if (m_imageLabel) { m_imageLabel->clearSelection(); }
        return;
    }

    // 在roi图上画识别结果
    cv::Mat display = roi.clone();
    QStringList detailLines;
    QString text;

    for (const auto &r : results) {
        cv::Scalar color = r.similarity > 0.9
            ? cv::Scalar(0, 255, 0)
            : (r.similarity > 0.8
                ? cv::Scalar(0, 200, 255) : cv::Scalar(0, 0, 255));

        cv::Rect glyphRect(r.x, r.y, r.width, r.height);
        cv::rectangle(display, glyphRect, color, 2);

        QString ch = QString::fromStdString(r.charName);
        text += ch;
        detailLines.append(
            QString("%1 (%2,%3) %4%")
                .arg(ch).arg(r.x).arg(r.y)
                .arg((int)(r.similarity * 100)));
    }

    // 显示识别画面
    QImage resultImg = matToQImage(display);
    QPixmap resultPix = QPixmap::fromImage(resultImg);
    m_imageLabel->setPixmap(resultPix);
    m_imageLabel->resize(resultPix.size());

    QMessageBox::information(this,
        QString::fromUtf8("\u5b57\u5e93\u8bc6\u522b\u7ed3\u679c"),
        QString::fromUtf8("\u8bc6\u522b\u6587\u5b57: %1\n\n\u8be6\u60c5:\n%2")
            .arg(text, detailLines.join("\n")));

        if (m_imageLabel) { m_imageLabel->clearSelection(); }
}

void GameItemCaptureWidget::handleBflColorPick(const QRect &sel)
{
    if (m_currentFrame.empty()) return;

    // 选区的中心点作为取色点
    QSize pixSize = m_currentPixmap.size();
    double fsx = (double)m_currentFrame.cols / pixSize.width();
    double fsy = (double)m_currentFrame.rows / pixSize.height();
    int cx = qBound(0, (int)(sel.center().x() * fsx), m_currentFrame.cols - 1);
    int cy = qBound(0, (int)(sel.center().y() * fsy), m_currentFrame.rows - 1);

    // 取像素颜色（BGR→RGB）
    auto &lib = BitmapFontLib::Instance();
    BflColorFilter filter = lib.colorFilter();

    QColor pickedColor;
    if (m_currentFrame.channels() >= 3) {
        cv::Vec3b bgr = m_currentFrame.at<cv::Vec3b>(cy, cx);
        pickedColor = QColor(bgr[2], bgr[1], bgr[0]);
    } else {
        uint8_t gray = m_currentFrame.at<uint8_t>(cy, cx);
        pickedColor = QColor(gray, gray, gray);
    }

    // 追加颜色（不替换之前的）
    filter.add(pickedColor);
    lib.setColorFilter(filter);

    // 更新颜色标签 — 显示多色色块
    int n = filter.points.size();
    QString tip = QString::fromUtf8("已取 %1 种颜色:").arg(n);
    for (int i = 0; i < n; i++) {
        const auto &pt = filter.points[i];
        QString h = QString("#%1%2%3")
            .arg(pt.color.red(), 2, 16, QChar('0'))
            .arg(pt.color.green(), 2, 16, QChar('0'))
            .arg(pt.color.blue(), 2, 16, QChar('0'));
        tip += QString("\n  %1 (%2)").arg(h).arg(i + 1);
    }
    m_bflColorLabel->setToolTip(tip);

    // 用最后取的颜色做标签底色

    // 用最后取的颜色做标签底色
    QString lastHex = QString("#%1%2%3")
        .arg(pickedColor.red(), 2, 16, QChar('0'))
        .arg(pickedColor.green(), 2, 16, QChar('0'))
        .arg(pickedColor.blue(), 2, 16, QChar('0'));
    m_bflColorLabel->setText(QString::fromUtf8("🎨 ×%1").arg(n));
    m_bflColorLabel->setStyleSheet(
        QString("background: %1; color: %2; border: 1px solid #999; padding: 0 8px; border-radius: 3px; font-weight: bold;")
            .arg(lastHex)
            .arg(pickedColor.lightness() > 128 ? "#000" : "#FFF"));

    infof("[BFL] 取色 #%d: ({},{}) → %s (共%d种)",
          n, cx, cy, lastHex.toStdString().c_str(), n);

        if (m_imageLabel) { m_imageLabel->clearSelection(); }
}
