#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "nlohmann/json.hpp"
using json =  nlohmann::json;
QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
private slots:
    void on_clientRadioButton_clicked();
    void on_serverRadioButton_clicked();
    void on_clickPushButton_clicked();

    void on_testButton_clicked();
    void clientRecMegSlot(const json &msg);
private:
    Ui::MainWindow *ui;
    bool isMaster;
};
#endif // MAINWINDOW_H
