#ifndef MOUSELABEL_H
#define MOUSELABEL_H

#include <QObject>
#include <QMouseEvent>
#include <QEnterEvent>
#include <QLabel>
class MouseLabel : public QLabel
{
    Q_OBJECT
public:
    explicit MouseLabel(QWidget *parent = nullptr);

protected:
    // 重写鼠标事件处理函数
    void wheelEvent(QWheelEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
public slots:
    void keyPressEventSlot(int vkCode);
    void keyReleaseEventSlot(int vkCode);

signals:
};

#endif // MOUSELABEL_H
