#include "gameslot.h"
#include "gameutils.h"
#include <QDebug>
#include <QPixmap>
#include <QImage>
#include <QScreen>
#include <QGuiApplication>

GameSlot::GameSlot(int index, QObject *parent)
    : QObject(parent), m_index(index)
{
}

void GameSlot::setAccount(const QString &acc, const QString &pwd)
{
    m_account = acc;
    m_password = pwd;
}

// ════════════════════════════════════════════════
// 在所有顶层窗口中找匹配角色名的游戏窗口
// 流程：枚举→截图每个窗口的角色名区域→模板匹配
// ════════════════════════════════════════════════
HWND GameSlot::findMatchingWindow(const cv::Mat &charNameROI)
{
    m_hwnd = nullptr;
    if (m_charName.isEmpty()) return nullptr;

    // 加载角色名模板
    cv::Mat templ = cv::imread(
        (GameUtils::Instance().templateRoot() + "/charnames/" + m_charName + ".png")
        .toLocal8Bit().toStdString());
    if (templ.empty()) return nullptr;

    struct EnumCtx { QString name; cv::Mat templ; cv::Rect roi; HWND best; double bestVal; };
    EnumCtx ctx = {m_charName, templ, cv::Rect(0,0,0,0), nullptr, 0.6};

    // 如果有角色名ROI,设置搜索区域
    // （画面坐标需转为窗口内坐标,这里先不做复杂转换,直接用全窗口搜索）

    // 枚举所有顶层窗口,找窗口类名为游戏窗口的
    ::EnumWindows([](HWND w, LPARAM lp) -> BOOL {
        auto *ctx = reinterpret_cast<EnumCtx *>(lp);
        if (!::IsWindowVisible(w)) return TRUE;

        wchar_t cls[256];
        ::GetClassNameW(w, cls, 256);
        QString className = QString::fromWCharArray(cls);

        // 大唐无双窗口类名（需确认）
        // 常见的可能是 Qt5154QWindowOwnDCIcon 或类似
        if (!className.contains("QWindow")) return TRUE;

        // 截取窗口画面
        RECT rect;
        if (!::GetWindowRect(w, &rect)) return TRUE;
        int ww = rect.right - rect.left;
        int wh = rect.bottom - rect.top;
        if (ww <= 0 || wh <= 0) return TRUE;

        // 用GDI截窗口
        HDC hdcWin = ::GetWindowDC(w);
        HDC hdcMem = ::CreateCompatibleDC(hdcWin);
        HBITMAP hbm = ::CreateCompatibleBitmap(hdcWin, ww, wh);
        HBITMAP hbmOld = (HBITMAP)::SelectObject(hdcMem, hbm);
        ::BitBlt(hdcMem, 0, 0, ww, wh, hdcWin, 0, 0, SRCCOPY);

        BITMAPINFOHEADER bi = {sizeof(BITMAPINFOHEADER), ww, -wh, 1, 32, BI_RGB};
        cv::Mat frame(wh, ww, CV_8UC4);
        ::GetDIBits(hdcMem, hbm, 0, wh, frame.data, (BITMAPINFO *)&bi, DIB_RGB_COLORS);

        ::SelectObject(hdcMem, hbmOld);
        ::DeleteObject(hbm);
        ::DeleteDC(hdcMem);
        ::ReleaseDC(w, hdcWin);

        if (frame.empty()) return TRUE;

        // 裁剪角色名区域做匹配
        cv::Mat search;
        if (ctx->roi.width > 0 && ctx->roi.height > 0) {
            cv::Rect safe = ctx->roi & cv::Rect(0, 0, ww, wh);
            search = frame(safe).clone();
        } else {
            search = frame;
        }

        if (ctx->templ.cols > search.cols || ctx->templ.rows > search.rows) return TRUE;

        cv::Mat result;
        cv::matchTemplate(search, ctx->templ, result, cv::TM_CCOEFF_NORMED);
        double maxVal;
        cv::minMaxLoc(result, nullptr, &maxVal, nullptr, nullptr);

        if (maxVal > ctx->bestVal) {
            ctx->bestVal = maxVal;
            ctx->best = w;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&ctx));

    m_hwnd = ctx.best;
    return m_hwnd;
}

bool GameSlot::bringToFront()
{
    if (!m_hwnd) return false;
    if (::IsIconic(m_hwnd))
        ::ShowWindow(m_hwnd, SW_RESTORE);
    ::SetForegroundWindow(m_hwnd);
    ::BringWindowToTop(m_hwnd);
    return true;
}

bool GameSlot::isForeground() const
{
    return m_hwnd && ::GetForegroundWindow() == m_hwnd;
}

void GameSlot::setROI(int type, const QRect &r)
{
    if (type >= 0 && type < 4) m_rois[type] = r;
}

QRect GameSlot::roi(int type) const
{
    return (type >= 0 && type < 4) ? m_rois[type] : QRect();
}

void GameSlot::setTask(TaskType t, const QString &param)
{
    m_taskType = t;
    m_taskParam = param;
}

QString GameSlot::taskName() const
{
    static const char *names[] = {"", "副本", "主线任务", "冒险", "一条龙"};
    QString s = names[m_taskType];
    if (!m_taskParam.isEmpty())
        s += "/" + m_taskParam;
    return s;
}

void GameSlot::setState(State s)
{
    if (m_state != s) {
        m_state = s;
        emit stateChanged(m_index, s);
    }
}

QString GameSlot::stateText() const
{
    static const char *texts[] = {"空闲", "查找窗口", "运行中", "暂停", "错误"};
    return texts[m_state];
}

void GameSlot::detectAll(const cv::Mat &frame)
{
    auto &gu = GameUtils::Instance();
    gu.setLocationROI(m_rois[0]);
    gu.setLevelROI(m_rois[1]);
    gu.setMainQuestROI(m_rois[2]);
    gu.setSkillsROI(m_rois[3]);

    auto loc = gu.detectLocation(frame);
    if (!loc.name.isEmpty()) m_curMap = loc.name;

    auto lvl = gu.detectLevel(frame);
    if (!lvl.name.isEmpty()) m_curLevel = lvl.name;

    auto mq = gu.detectMainQuest(frame);
    if (!mq.name.isEmpty()) m_curQuest = mq.name;

    m_curSkills = gu.detectSkills(frame);

    emit detected(m_index);
}
