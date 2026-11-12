#ifndef SCREENSHOTWIDGET_H
#define SCREENSHOTWIDGET_H

#include <QWidget>
#include <QPixmap>
#include <QImage>
#include <QPolygon>
#include <QList>
#include <functional>

class QToolButton;

// 全屏截图遮罩：框选 / 调整 / 移动 / 标注 / 保存 / 复制
class ScreenshotWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ScreenshotWidget(QWidget *parent = nullptr);
    void start();                      // 抓取所有屏幕并显示遮罩

signals:
    void finished();

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
    void closeEvent(QCloseEvent *e) override;

private:
    enum State  { Idle, Selecting, Ready, Moving, Resizing, Drawing };
    enum Handle { HNone, HTL, HT, HTR, HR, HBR, HB, HBL, HL };
    enum Tool   { ToolNone, ToolRect, ToolEllipse, ToolArrow, ToolPen };
    struct Shape {
        Tool tool = ToolNone;
        QPoint p1, p2;
        QPolygon pts;
    };

    void buildToolbar();
    void showBar();
    void resetSelection();
    Handle hitHandle(const QPoint &pt) const;
    void updateCursorAt(const QPoint &pt);
    void drawShape(QPainter &p, const Shape &s) const;
    void drawMagnifier(QPainter &p) const;
    QPixmap result() const;
    void finishCopy();
    void saveToFile();
    void undo();

    QPixmap m_bg;          // 整个虚拟桌面截图（带 devicePixelRatio）
    QImage  m_bgImage;
    qreal   m_dpr = 1.0;

    State  m_state = Idle;
    bool   m_hasSel = false;
    QRect  m_sel;
    QRect  m_pressSel;
    QPoint m_origin;
    QPoint m_mouse;
    Handle m_handle = HNone;

    Tool m_tool = ToolNone;
    QList<Shape> m_shapes;
    Shape m_cur;

    QWidget *m_bar = nullptr;
    QList<QToolButton *> m_toolBtns;
};

#endif // SCREENSHOTWIDGET_H
