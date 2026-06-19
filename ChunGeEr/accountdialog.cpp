#include "accountdialog.h"
#include <QCoreApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QFileDialog>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDebug>

AccountDialog::AccountDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QString::fromUtf8("账号管理"));
    setMinimumWidth(560);

    auto *root = new QVBoxLayout(this);

    // ── 游戏路径 ──
    auto *pathBox = new QHBoxLayout();
    pathBox->addWidget(new QLabel(QString::fromUtf8("游戏路径:")));
    m_gamePathLabel = new QLabel(QString::fromUtf8("(自动查找)"));
    m_gamePathLabel->setStyleSheet("color:#888;");
    auto *browseBtn = new QPushButton(QString::fromUtf8("浏览..."));
    connect(browseBtn, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getOpenFileName(this,
            QString::fromUtf8("选择游戏程序"), "",
            QString::fromUtf8("可执行文件 (*.exe);;所有文件 (*)"));
        if (!path.isEmpty()) m_gamePathLabel->setText(path);
    });
    pathBox->addWidget(m_gamePathLabel, 1);
    pathBox->addWidget(browseBtn);
    root->addLayout(pathBox);

    // ── 账号密码 / 角色名 / 门派 ──
    auto *acctGroup = new QGroupBox(QString::fromUtf8("账号信息"));
    auto *acctGrid = new QGridLayout(acctGroup);
    acctGrid->setHorizontalSpacing(8);
    acctGrid->addWidget(new QLabel(QString::fromUtf8("账号")), 0, 1);
    acctGrid->addWidget(new QLabel(QString::fromUtf8("密码")), 0, 2);
    acctGrid->addWidget(new QLabel(QString::fromUtf8("角色名")), 0, 3);
    acctGrid->addWidget(new QLabel(QString::fromUtf8("门派")), 0, 4);

    QStringList factions = {"", QString::fromUtf8("少林"), QString::fromUtf8("天煞"),
        QString::fromUtf8("蜀山"), QString::fromUtf8("寒冰"), QString::fromUtf8("无名"),
        QString::fromUtf8("侠隐"), QString::fromUtf8("百花医"), QString::fromUtf8("百花蛊")};

    for (int i = 0; i < 3; i++) {
        acctGrid->addWidget(new QLabel(QString("窗口%1").arg(i + 1)), i + 1, 0);
        m_accEdits[i] = new QLineEdit();
        m_pwdEdits[i] = new QLineEdit();
        m_pwdEdits[i]->setEchoMode(QLineEdit::Password);
        m_accEdits[i]->setMaximumWidth(140);
        m_pwdEdits[i]->setMaximumWidth(100);
        m_nameEdits[i] = new QLineEdit();
        m_nameEdits[i]->setMaximumWidth(80);
        m_factionCombos[i] = new QComboBox();
        m_factionCombos[i]->addItems(factions);
        acctGrid->addWidget(m_accEdits[i], i + 1, 1);
        acctGrid->addWidget(m_pwdEdits[i], i + 1, 2);
        acctGrid->addWidget(m_nameEdits[i], i + 1, 3);
        acctGrid->addWidget(m_factionCombos[i], i + 1, 4);
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
    QSettings settings(QCoreApplication::applicationDirPath() + "/config.ini", QSettings::IniFormat);

    // 游戏路径
    QString gamePath = settings.value("Accounts/GamePath").toString();
    if (gamePath.isEmpty()) gamePath = findGamePath();
    m_gamePathLabel->setText(gamePath.isEmpty() ? QString::fromUtf8("(未找到)") : gamePath);

    // 账号
    for (int i = 0; i < 3; i++) {
        QString slotKey = QString("Slot%1").arg(i);
        m_accEdits[i]->setText(settings.value("Accounts/" + slotKey + "/Account").toString());
        m_pwdEdits[i]->setText(settings.value("Accounts/" + slotKey + "/Password").toString());
        m_nameEdits[i]->setText(settings.value("Accounts/" + slotKey + "/CharName").toString());
        int fi = m_factionCombos[i]->findText(settings.value("Accounts/" + slotKey + "/CharFaction").toString());
        if (fi >= 0) m_factionCombos[i]->setCurrentIndex(fi);
    }
}

void AccountDialog::save()
{
    QSettings settings(QCoreApplication::applicationDirPath() + "/config.ini", QSettings::IniFormat);

    QString gamePath = m_gamePathLabel->text();
    if (gamePath != QString::fromUtf8("(未找到)") && gamePath != QString::fromUtf8("(自动查找)"))
        settings.setValue("Accounts/GamePath", gamePath);

    for (int i = 0; i < 3; i++) {
        QString slotKey = QString("Slot%1").arg(i);
        settings.setValue("Accounts/" + slotKey + "/Account", m_accEdits[i]->text());
        settings.setValue("Accounts/" + slotKey + "/Password", m_pwdEdits[i]->text());
        settings.setValue("Accounts/" + slotKey + "/CharName", m_nameEdits[i]->text());
        settings.setValue("Accounts/" + slotKey + "/CharFaction", m_factionCombos[i]->currentText());
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

