#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

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
    void on_boxBtn_clicked();

    void on_trainBtn_clicked();

    void on_openImagePathBtn_clicked();
private:
    void init();
private:
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
