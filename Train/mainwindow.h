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


    void clientGetLogInfo(int inttype, const std::string &type, const std::string &logstr, const std::string &time);
signals:
    void startBox();
    void stopBox();
    void startTrain();
    void stopTrain();
    void log(const QString &info);
private slots:
    void on_boxBtn_clicked();

    void on_trainBtn_clicked();

    void on_openImagePathBtn_clicked();

    void on_startBox();
    void on_stopBox();
    void on_startTrain();
    void on_stopTrain();
    void on_log(const QString &info);
private:
    void init();
private:
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
