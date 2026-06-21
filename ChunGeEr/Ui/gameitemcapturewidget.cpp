#include "gameitemcapturewidget.h"
#include <QPainter>
#include <QPen>
#include <QMouseEvent>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QDebug>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QScrollArea>
#include <QMenu>
#include <QSettings>
#include <QCoreApplication>
#include "../Ocr/ocrmnager.h"

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
{
    setWindowTitle(QString::fromUtf8("游戏物品截取"));
    resize(1100, 700);

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
    m_dirBtn = new QPushButton(QString::fromUtf8("📁 选择目录"), this);

    m_roiTypeCombo = new QComboBox(this);
    m_roiTypeCombo->addItems({
        QString::fromUtf8("地图名"), QString::fromUtf8("等级"),
        QString::fromUtf8("技能"), QString::fromUtf8("主线任务"),
        QString::fromUtf8("掉线"), QString::fromUtf8("卡住"),
        QString::fromUtf8("设置区域")
    });
    m_roiTypeCombo->setMinimumWidth(90);
    m_roiTypeCombo->setVisible(false);

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
    topLayout->addWidget(m_roiBtn);

    topLayout->addWidget(m_dirBtn);
    topLayout->addStretch();

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
    rootLayout->addLayout(midLayout, 1);

    // ── 信号连接 ──
    connect(m_pauseBtn, &QPushButton::clicked, this, &GameItemCaptureWidget::onPauseToggle);
    connect(m_screenshotBtn, &QPushButton::clicked, this, &GameItemCaptureWidget::onScreenshot);
    connect(m_testBtn, &QPushButton::clicked, this, &GameItemCaptureWidget::onTestMatch);
    connect(m_ocrBtn, &QPushButton::clicked, this, &GameItemCaptureWidget::onOcrRecognize);
    connect(m_dirBtn, &QPushButton::clicked, this, &GameItemCaptureWidget::onSelectSaveDir);
    connect(m_roiBtn, &QPushButton::clicked, this, &GameItemCaptureWidget::onRoiModeToggle);
    // ROI模式框选 → 直接保存
    connect(m_imageLabel, &CaptureImageLabel::selectionChanged, this, [this](const QRect &sel) {
        if (!m_roiMode || !sel.isValid() || sel.width() < 3 || sel.height() < 3) return;
        // ROI类型key映射
        static const QStringList roiKeys = {"Location", "Level", "Skills", "MainQuest", "Disconnect", "Stopped", "SettingsPanel"};
        int idx = m_roiTypeCombo->currentIndex();
        if (idx < 0 || idx >= roiKeys.size()) return;

        // 框选区域是pixmap坐标，转换到原始帧坐标
        QSize pixSize = m_currentPixmap.size();
        double sx = (double)m_currentFrame.cols / pixSize.width();
        double sy = (double)m_currentFrame.rows / pixSize.height();
        int rx = qBound(0, (int)(sel.x() * sx), m_currentFrame.cols - 1);
        int ry = qBound(0, (int)(sel.y() * sy), m_currentFrame.rows - 1);
        int rw = qBound(1, (int)(sel.width() * sx), m_currentFrame.cols - rx);
        int rh = qBound(1, (int)(sel.height() * sy), m_currentFrame.rows - ry);

        saveRoi(roiKeys[idx], QRect(rx, ry, rw, rh));
        m_imageLabel->clearSelection();
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
        m_imageLabel->setCursor(Qt::CrossCursor);
    } else {
        m_pauseBtn->setText(QString::fromUtf8("⏸ 暂停"));
        m_screenshotBtn->setEnabled(false);
        m_ocrBtn->setEnabled(false);
        m_imageLabel->clearSelection();
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

    m_imageLabel->clearSelection();
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

        cv::Mat result, searchMat = frame, tplMat = templ;
        cv::matchTemplate(searchMat, tplMat, result, cv::TM_CCOEFF_NORMED);

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
    } else {
        m_roiBtn->setText(QString::fromUtf8("🎯 ROI模式"));
        m_roiBtn->setStyleSheet("");
        m_roiTypeCombo->setVisible(false);
        m_screenshotBtn->setVisible(true);
        m_nameEdit->setVisible(true);
        m_itemTree->setVisible(true);
        m_imageLabel->clearSelection();
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
