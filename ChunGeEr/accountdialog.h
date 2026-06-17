#ifndef ACCOUNTDIALOG_H
#define ACCOUNTDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QSettings>

class AccountDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AccountDialog(QWidget *parent = nullptr);

signals:
    void accountsChanged();

private:
    void save();
    void load();
    QString findGamePath();
    void pickCharImage(int slotIdx);

    QLabel *m_charPreviews[3];
    QString m_charPaths[3];
    QLineEdit *m_accEdits[3];
    QLineEdit *m_pwdEdits[3];
    QLabel *m_gamePathLabel;
};

#endif
