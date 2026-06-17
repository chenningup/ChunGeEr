#include "accountdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QFileDialog>
#include <QPixmap>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDebug>

AccountDialog::AccountDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QString::fromUtf8("账号管理"));
    setMinimumWidth(500);

    auto *root = new QVBoxLayout(this);

    // ── 游戏路径 ──
    auto *pathBox = new QHBoxLayout();
    pathBox->addWidget(new QLabel(QString::fromUtf8("游戏路径:")));
    m_gamePathLabel = new QLabel(QString::fromUtf8("(自动查找)"));
    m_gamePathLabel->setStyleSheet("color:#888;");
    auto *browseBtn = new QPushButton(QString::fromUtf8("浏览..."));
    connect(browseBtn, &QPushButton::clicked, this, [this]() {
        QDir().mkpath("images/roles");
    QString path = QFileDialog::getOpenFileName(this,
            QString::fromUtf8("选择游戏程序"), "",
            QString::fromUtf8("可执行文件 (*.exe);;所有文件 (*)"));
        if (!path.isEmpty()) m_gamePathLabel->setText(path);
    });
    pathBox->addWidget(m_gamePathLabel, 1);
    pathBox->addWidget(browseBtn);
    root->addLayout(pathBox);

    // ── 角色名图片 ──
    auto *charGroup = new QGroupBox(QString::fromUtf8("角色名截图"));
    auto *charGrid = new QGridLayout(charGroup);
    charGrid->setHorizontalSpacing(8);

    for (int i = 0; i < 3; i++) {
        charGrid->addWidget(new QLabel(QString("窗口%1").arg(i + 1)), i, 0);
        m_charPreviews[i] = new QLabel();
        m_charPreviews[i]->setFixedSize(48, 48);
        m_charPreviews[i]->setStyleSheet("border:1px solid #555; background:#222;");
        m_charPreviews[i]->setAlignment(Qt::AlignCenter);
        m_charPreviews[i]->setText("?");
        charGrid->addWidget(m_charPreviews[i], i, 1);

        auto *pickBtn = new QPushButton(QString::fromUtf8("选择图片..."));
        connect(pickBtn, &QPushButton::clicked, this, [this, i]() { pickCharImage(i); });
        charGrid->addWidget(pickBtn, i, 2);
    }
    root->addWidget(charGroup);

    // ── 账号密码 ──
    auto *acctGroup = new QGroupBox(QString::fromUtf8("账号信息"));
    auto *acctGrid = new QGridLayout(acctGroup);
    acctGrid->setHorizontalSpacing(8);
    acctGrid->addWidget(new QLabel(QString::fromUtf8("账号")), 0, 1);
    acctGrid->addWidget(new QLabel(QString::fromUtf8("密码")), 0, 2);

    for (int i = 0; i < 3; i++) {
        acctGrid->addWidget(new QLabel(QString("窗口%1").arg(i + 1)), i + 1, 0);
        m_accEdits[i] = new QLineEdit();
        m_pwdEdits[i] = new QLineEdit();
        m_pwdEdits[i]->setEchoMode(QLineEdit::Password);
        m_accEdits[i]->setMaximumWidth(120);
        m_pwdEdits[i]->setMaximumWidth(120);
        acctGrid->addWidget(m_accEdits[i], i + 1, 1);
        acctGrid->addWidget(m_pwdEdits[i], i + 1, 2);
    }
    root->addWidget(acctGroup);

    // ── 按钮 ──
    auto *btnBox = new QHBoxLayout();
    btnBox->addStretch();
    auto *saveBtn = new QPushButton(QString::fromUtf8("保存"));
    connect(saveBtn, &QPushButton::clicked, this, &AccountDialog::save);
    auto *closeBtn = new QPushButton(QString::fromUtf8("关闭"));
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnBox->addWidget(saveBtn);
    btnBox->addWidget(closeBtn);
    root->addLayout(btnBox);

    load();
}

void AccountDialog::load()
{
    QSettings settings("config.ini", QSettings::IniFormat);

    // 游戏路径
    QString gamePath = settings.value("Accounts/GamePath").toString();
    if (gamePath.isEmpty()) gamePath = findGamePath();
    m_gamePathLabel->setText(gamePath.isEmpty() ? QString::fromUtf8("(未找到)") : gamePath);
    QDir().mkpath("images/roles");

    // 账号
    for (int i = 0; i < 3; i++) {
        QString prefix = QString("Accounts/Slot%1/").arg(i);
        m_charPaths[i] = settings.value(prefix + "CharImage").toString();
        if (m_charPaths[i].isEmpty()) {
            QString defaultPath = QDir("images/roles").absoluteFilePath(
                settings.value(prefix + "CharName").toString() + ".png");
            if (QFileInfo::exists(defaultPath)) m_charPaths[i] = defaultPath;
        }
        m_accEdits[i]->setText(settings.value(prefix + "Account").toString());
        m_pwdEdits[i]->setText(settings.value(prefix + "Password").toString());

        // 预览
        QString defRole = QDir("images/roles").absoluteFilePath(m_accEdits[i]->text() + ".png");
    if (!QFileInfo::exists(m_charPaths[i]) && QFileInfo::exists(defRole))
        m_charPaths[i] = defRole;
    if (!m_charPaths[i].isEmpty() && QFileInfo::exists(m_charPaths[i])) {
            QPixmap pix(m_charPaths[i]);
            if (!pix.isNull())
                m_charPreviews[i]->setPixmap(pix.scaled(44, 44, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            else
                m_charPreviews[i]->setText("?");
        }
    }
}

void AccountDialog::save()
{
    QSettings settings("config.ini", QSettings::IniFormat);

    QString gamePath = m_gamePathLabel->text();
    if (gamePath != QString::fromUtf8("(未找到)") && gamePath != QString::fromUtf8("(自动查找)"))
        settings.setValue("Accounts/GamePath", gamePath);

    for (int i = 0; i < 3; i++) {
        QString prefix = QString("Accounts/Slot%1/").arg(i);
        settings.setValue(prefix + "CharImage", m_charPaths[i]);
        settings.setValue(prefix + "Account", m_accEdits[i]->text());
        settings.setValue(prefix + "Password", m_pwdEdits[i]->text());
    }
    emit accountsChanged();
    accept();
}

QString AccountDialog::findGamePath()
{
    // 从桌面快捷方式找游戏
    QStringList desktopDirs = {
        QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
        QDir::homePath() + "/Desktop",
        "C:/Users/Public/Desktop"
    };

    QStringList keywords = {"大唐", "无双", "dtws", "datang"};

    for (const QString &desktop : desktopDirs) {
        QDir dir(desktop);
        if (!dir.exists()) continue;

        for (const QFileInfo &fi : dir.entryInfoList({"*.lnk"}, QDir::Files)) {
            QString name = fi.baseName().toLower();
            for (const QString &kw : keywords) {
                if (name.contains(kw)) {
                    // 解析 .lnk 获取目标路径
                    // 简化：尝试常见安装路径
                    QStringList guessPaths = {
                        "C:/Program Files (x86)/dtws/dtws.exe",
                        "C:/Program Files/大唐无双/dtws.exe",
                        "D:/大唐无双/dtws.exe",
                    };
                    for (const QString &gp : guessPaths) {
                        if (QFileInfo::exists(gp)) return gp;
                    }
                    return fi.absoluteFilePath(); // 返回lnk路径作为fallback
                }
            }
        }
    }
    return "";
}

void AccountDialog::pickCharImage(int slotIdx)
{
    QDir().mkpath("images/roles");
    QString path = QFileDialog::getOpenFileName(this,
        QString::fromUtf8("选择角色名截图"),
        m_charPaths[slotIdx],
        QString::fromUtf8("图片 (*.png *.jpg *.bmp);;所有文件 (*)"));
    if (path.isEmpty()) return;

    m_charPaths[slotIdx] = path;
    QPixmap pix(path);
    if (!pix.isNull())
        m_charPreviews[slotIdx]->setPixmap(pix.scaled(44, 44, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}
