#ifndef ACCOUNTDIALOG_H
#define ACCOUNTDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
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

    QLineEdit *m_accEdits[3];
    QLineEdit *m_pwdEdits[3];
    QLineEdit *m_nameEdits[3];
    QComboBox *m_factionCombos[3];
    QLabel *m_gamePathLabel;
};

#endif
