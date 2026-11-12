#include "screenshotwidget.h"

#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDateTime>
#include <QFileDialog>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QStandardPaths>
#include <QToolButton>

static const QColor kGreen(7, 193, 96);
static const QColor kRed(255, 59, 48);

ScreenshotWidget::ScreenshotWidget(QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::CrossCursor);
    buildToolbar();
}

void ScreenshotWidget::start()
{
    QRect vg;
    qreal dpr = 1.0;
    const auto screens = QGuiApplication::screens();
    for (QScreen *s : screens) {
        vg = vg.united(s->geometry());
        dpr = qMax(dpr, s->devicePixelRatio());
    }
    m_dpr = dpr;

    QPixmap bg(vg.size() * dpr);
    bg.setDevicePixelRatio(dpr);
    bg.fill(Qt::black);
    QPainter p(&bg);
    for (QScreen *s : screens) {
        QPixmap shot = s->grabWindow(0);
        p.drawPixmap(QRect(s->geometry().topLeft() - vg.topLeft(), s->geometry().size()), shot);
    }
    p.end();

    m_bg = bg;
    m_bgImage = bg.toImage();
    m_mouse = mapFromGlobal(QCursor::pos());

    setGeometry(vg);
    show();
    raise();
    activateWindow();
    setFocus();
}

// ---------------------------------------------------------------- 工具栏
void ScreenshotWidget::buildToolbar()
{
    m_bar = new QWidget(this);
    m_bar->setObjectName("bar");
    m_bar->setAttribute(Qt::WA_StyledBackground, true);
    m_bar->setStyleSheet(
        "QWidget#bar{background:#2b2b2b;border-radius:4px;}"
        "QToolButton{color:#eee;background:transparent;border:none;padding:4px 8px;font-size:14px;}"
        "QToolButton:hover{background:#444;}"
        "QToolButton:checked{background:#07c160;}");
    auto *lay = new QHBoxLayout(m_bar);
    lay->setContentsMargins(6, 4, 6, 4);
    lay->setSpacing(2);

    auto addTool = [this, lay](const QString &text, const QString &tip, Tool t) {
        auto *b = new QToolButton(m_bar);
        b->setText(text);
        b->setToolTip(tip);
        b->setCheckable(true);
        connect(b, &QToolButton::clicked, this, [this, b, t](bool on) {
            m_tool = on ? t : ToolNone;
            for (QToolButton *o : m_toolBtns)
                if (o != b) o->setChecked(false);
            updateCursorAt(m_mouse);
        });
        m_toolBtns.append(b);
        lay->addWidget(b);
    };
    auto addAction = [this, lay](const QString &text, const QString &tip, std::function<void()> fn) {
        auto *b = new QToolButton(m_bar);
        b->setText(text);
        b->setToolTip(tip);
        connect(b, &QToolButton::clicked, this, [fn] { fn(); });
        lay->addWidget(b);
    };

    addTool("▭", "矩形", ToolRect);
    addTool("◯", "椭圆", ToolEllipse);
    addTool("↗", "箭头", ToolArrow);
    addTool("✎", "画笔", ToolPen);
    addAction("↶", "撤销 (Ctrl+Z)", [this] { undo(); });

    auto *sep = new QFrame(m_bar);
    sep->setFrameShape(QFrame::VLine);
    sep->setStyleSheet("color:#555;");
    lay->addWidget(sep);

    addAction("✕", "取消 (Esc)", [this] { close(); });
    addAction("保存", "保存到文件 (Ctrl+S)", [this] { saveToFile(); });
    addAction("✓ 完成", "复制到剪贴板 (Enter)", [this] { finishCopy(); });

    m_bar->hide();
}

void ScreenshotWidget::showBar()
{
    m_bar->adjustSize();
    int x = qMax(0, m_sel.right() - m_bar->width() + 1);
    int y = m_sel.bottom() + 8;
    if (y + m_bar->height() > height())
        y = m_sel.top() - m_bar->height() - 8;
    if (y < 0)
        y = m_sel.bottom() - m_bar->height() - 8;
    m_bar->move(x, y);
    m_bar->show();
    m_bar->raise();
}

void ScreenshotWidget::resetSelection()
{
    m_hasSel = false;
    m_state = Idle;
    m_shapes.clear();
    m_tool = ToolNone;
    for (QToolButton *b : m_toolBtns) b->setChecked(false);
    m_bar->hide();
    setCursor(Qt::CrossCursor);
    update();
}

// ---------------------------------------------------------------- 命中/光标
ScreenshotWidget::Handle ScreenshotWidget::hitHandle(const QPoint &pt) const
{
    if (!m_hasSel) return HNone;
    const QRect r = m_sel;
    const int cx = r.center().x(), cy = r.center().y();
    struct { Handle h; QPoint c; } hs[] = {
        {HTL, r.topLeft()},        {HT, QPoint(cx, r.top())},      {HTR, r.topRight()},
        {HR,  QPoint(r.right(), cy)}, {HBR, r.bottomRight()},      {HB, QPoint(cx, r.bottom())},
        {HBL, r.bottomLeft()},     {HL, QPoint(r.left(), cy)}
    };
    const int m = 6;
    for (const auto &x : hs)
        if (qAbs(pt.x() - x.c.x()) <= m && qAbs(pt.y() - x.c.y()) <= m)
            return x.h;
    return HNone;
}

void ScreenshotWidget::updateCursorAt(const QPoint &pt)
{
    if (m_state != Ready && m_state != Idle) return;
    if (!m_hasSel) { setCursor(Qt::CrossCursor); return; }
    switch (hitHandle(pt)) {
    case HTL: case HBR: setCursor(Qt::SizeFDiagCursor); return;
    case HTR: case HBL: setCursor(Qt::SizeBDiagCursor); return;
    case HT:  case HB:  setCursor(Qt::SizeVerCursor);   return;
    case HL:  case HR:  setCursor(Qt::SizeHorCursor);   return;
    default: break;
    }
    if (m_sel.contains(pt))
        setCursor(m_tool == ToolNone ? Qt::SizeAllCursor : Qt::CrossCursor);
    else
        setCursor(Qt::ArrowCursor);
}

// ---------------------------------------------------------------- 鼠标
void ScreenshotWidget::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::RightButton) {       // 右键：有选区则重选，否则退出
        if (m_hasSel) resetSelection(); else close();
        return;
    }
    if (e->button() != Qt::LeftButton) return;

    const QPoint pt = e->pos();
    m_origin = pt;

    if (m_hasSel && m_state == Ready) {
        Handle h = hitHandle(pt);
        if (h != HNone) {
            m_handle = h;
            m_pressSel = m_sel;
            m_state = Resizing;
            m_bar->hide();
        } else if (m_sel.contains(pt)) {
            if (m_tool != ToolNone) {
                m_cur = Shape();
                m_cur.tool = m_tool;
                m_cur.p1 = m_cur.p2 = pt;
                if (m_tool == ToolPen) m_cur.pts << pt;
                m_state = Drawing;
            } else {
                m_pressSel = m_sel;
                m_state = Moving;
                m_bar->hide();
            }
        }
        return;
    }
    if (!m_hasSel) {
        m_state = Selecting;
        m_sel = QRect(pt, pt);
    }
}

void ScreenshotWidget::mouseMoveEvent(QMouseEvent *e)
{
    m_mouse = e->pos();
    switch (m_state) {
    case Idle:
        update();
        break;
    case Selecting:
        m_sel = QRect(m_origin, m_mouse).normalized();
        m_hasSel = true;
        update();
        break;
    case Moving: {
        QRect r = m_pressSel.translated(m_mouse - m_origin);
        r.moveLeft(qBound(0, r.left(), width() - r.width()));
        r.moveTop(qBound(0, r.top(), height() - r.height()));
        const QPoint real = r.topLeft() - m_sel.topLeft();
        for (Shape &s : m_shapes) {          // 标注跟着选区一起移动
            s.p1 += real; s.p2 += real; s.pts.translate(real);
        }
        m_sel = r;
        update();
        break;
    }
    case Resizing: {
        const QPoint d = m_mouse - m_origin;
        QRect r = m_pressSel;
        switch (m_handle) {
        case HTL: r.setLeft(r.left() + d.x());  r.setTop(r.top() + d.y());       break;
        case HT:  r.setTop(r.top() + d.y());                                     break;
        case HTR: r.setRight(r.right() + d.x()); r.setTop(r.top() + d.y());      break;
        case HR:  r.setRight(r.right() + d.x());                                 break;
        case HBR: r.setRight(r.right() + d.x()); r.setBottom(r.bottom() + d.y()); break;
        case HB:  r.setBottom(r.bottom() + d.y());                               break;
        case HBL: r.setLeft(r.left() + d.x());  r.setBottom(r.bottom() + d.y()); break;
        case HL:  r.setLeft(r.left() + d.x());                                   break;
        default: break;
        }
        m_sel = r.normalized().intersected(rect());
        update();
        break;
    }
    case Drawing: {
        QPoint pt(qBound(m_sel.left(), m_mouse.x(), m_sel.right()),
                  qBound(m_sel.top(),  m_mouse.y(), m_sel.bottom()));
        m_cur.p2 = pt;
        if (m_cur.tool == ToolPen) m_cur.pts << pt;
        update();
        break;
    }
    case Ready:
        updateCursorAt(m_mouse);
        break;
    }
}

void ScreenshotWidget::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) return;
    switch (m_state) {
    case Selecting:
        if (m_sel.width() < 4 || m_sel.height() < 4) {
            m_hasSel = false;
            m_state = Idle;
        } else {
            m_state = Ready;
            showBar();
        }
        break;
    case Moving:
    case Resizing:
        m_state = Ready;
        showBar();
        break;
    case Drawing:
        if (m_cur.p1 != m_cur.p2 || m_cur.tool == ToolPen)
            m_shapes.append(m_cur);
        m_state = Ready;
        break;
    default:
        break;
    }
    update();
}

void ScreenshotWidget::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && m_hasSel && m_sel.contains(e->pos()))
        finishCopy();
}

void ScreenshotWidget::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape) {
        close();
    } else if ((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) && m_hasSel && m_state == Ready) {
        finishCopy();
    } else if (e->matches(QKeySequence::Undo)) {
        undo();
    } else if (e->matches(QKeySequence::Save) && m_hasSel && m_state == Ready) {
        saveToFile();
    } else if (e->matches(QKeySequence::Copy) && m_hasSel && m_state == Ready) {
        finishCopy();
    }
}

void ScreenshotWidget::closeEvent(QCloseEvent *e)
{
    QWidget::closeEvent(e);
    emit finished();
}

// ---------------------------------------------------------------- 绘制
void ScreenshotWidget::drawShape(QPainter &p, const Shape &s) const
{
    p.setPen(QPen(kRed, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    switch (s.tool) {
    case ToolRect:    p.drawRect(QRect(s.p1, s.p2).normalized()); break;
    case ToolEllipse: p.drawEllipse(QRect(s.p1, s.p2).normalized()); break;
    case ToolPen:     p.drawPolyline(s.pts); break;
    case ToolArrow: {
        QLineF l(s.p1, s.p2);
        if (l.length() < 2) break;
        p.drawLine(l);
        QLineF back(l.p2(), l.p1());          // 从箭头尖端指回起点
        back.setLength(16);
        QLineF a = back, b = back;
        a.setAngle(back.angle() + 25);
        b.setAngle(back.angle() - 25);
        p.drawLine(a);
        p.drawLine(b);
        break;
    }
    default: break;
    }
}

void ScreenshotWidget::drawMagnifier(QPainter &p) const
{
    const int N = 15, Z = 8, S = N * Z;       // 15x15 像素放大 8 倍
    const QPoint dc = m_mouse * m_dpr;
    const QImage crop = m_bgImage.copy(dc.x() - N / 2, dc.y() - N / 2, N, N);
    const QColor c = m_bgImage.valid(dc) ? m_bgImage.pixelColor(dc) : QColor(Qt::black);

    int x = m_mouse.x() + 20, y = m_mouse.y() + 20;
    if (x + S + 2 > width())  x = m_mouse.x() - S - 20;
    if (y + S + 44 > height()) y = m_mouse.y() - S - 44 - 20;

    p.save();
    p.setRenderHint(QPainter::Antialiasing, false);
    p.drawImage(QRect(x, y, S, S), crop);
    p.setPen(QPen(kGreen, 1));
    p.drawRect(QRect(x + (N / 2) * Z, y + (N / 2) * Z, Z, Z));      // 中心像素
    p.setPen(Qt::white);
    p.drawRect(QRect(x, y, S, S));
    p.fillRect(QRect(x, y + S, S, 40), QColor(0, 0, 0, 200));
    p.setPen(Qt::white);
    p.drawText(QRect(x + 6, y + S + 2, S - 12, 18), Qt::AlignLeft | Qt::AlignVCenter,
               QString("%1 × %2").arg(qRound(m_mouse.x() * m_dpr)).arg(qRound(m_mouse.y() * m_dpr)));
    p.drawText(QRect(x + 6, y + S + 20, S - 12, 18), Qt::AlignLeft | Qt::AlignVCenter,
               QString("RGB(%1,%2,%3)").arg(c.red()).arg(c.green()).arg(c.blue()));
    p.restore();
}

void ScreenshotWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.drawPixmap(0, 0, m_bg);
    p.fillRect(rect(), QColor(0, 0, 0, 110));          // 变暗

    if (m_hasSel && m_sel.width() > 0 && m_sel.height() > 0) {
        // 选区内还原原图
        p.drawPixmap(m_sel, m_bg, QRect(m_sel.topLeft() * m_dpr, m_sel.size() * m_dpr));

        // 标注（裁剪在选区内）
        p.save();
        p.setClipRect(m_sel);
        p.setRenderHint(QPainter::Antialiasing, true);
        for (const Shape &s : m_shapes) drawShape(p, s);
        if (m_state == Drawing) drawShape(p, m_cur);
        p.restore();

        // 边框 + 8 个控制点
        p.setPen(QPen(kGreen, 2));
        p.setBrush(Qt::NoBrush);
        p.drawRect(m_sel.adjusted(0, 0, -1, -1));
        const int cx = m_sel.center().x(), cy = m_sel.center().y();
        const QPoint pts[] = {
            m_sel.topLeft(), QPoint(cx, m_sel.top()), m_sel.topRight(),
            QPoint(m_sel.right(), cy), m_sel.bottomRight(), QPoint(cx, m_sel.bottom()),
            m_sel.bottomLeft(), QPoint(m_sel.left(), cy)
        };
        p.setPen(Qt::NoPen);
        p.setBrush(kGreen);
        for (const QPoint &pt : pts) p.drawRect(QRect(pt.x() - 3, pt.y() - 3, 7, 7));

        // 尺寸标签
        const QString txt = QString("%1 × %2")
                .arg(qRound(m_sel.width() * m_dpr)).arg(qRound(m_sel.height() * m_dpr));
        const int tw = QFontMetrics(p.font()).boundingRect(txt).width() + 14;
        int ty = m_sel.top() - 26;
        if (ty < 0) ty = m_sel.top() + 4;
        QRect tr(m_sel.left(), ty, tw, 22);
        p.setBrush(QColor(0, 0, 0, 170));
        p.drawRoundedRect(tr, 3, 3);
        p.setPen(Qt::white);
        p.drawText(tr, Qt::AlignCenter, txt);
    }

    if (m_state == Idle || m_state == Selecting)
        drawMagnifier(p);
}

// ---------------------------------------------------------------- 输出
QPixmap ScreenshotWidget::result() const
{
    QPixmap out(m_sel.size() * m_dpr);
    out.setDevicePixelRatio(m_dpr);
    QPainter p(&out);
    p.drawPixmap(QPoint(0, 0), m_bg, QRect(m_sel.topLeft() * m_dpr, m_sel.size() * m_dpr));
    p.setRenderHint(QPainter::Antialiasing, true);
    p.translate(-m_sel.topLeft());
    for (const Shape &s : m_shapes) drawShape(p, s);
    p.end();
    return out;
}

void ScreenshotWidget::finishCopy()
{
    QApplication::clipboard()->setPixmap(result());
    close();
}

void ScreenshotWidget::saveToFile()
{
    const QPixmap img = result();
    hide();                                   // 先隐藏置顶遮罩，避免挡住对话框
    const QString def = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)
            + "/截图_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".png";
    const QString f = QFileDialog::getSaveFileName(nullptr, "保存截图", def,
                                                   "PNG (*.png);;JPEG (*.jpg);;BMP (*.bmp)");
    if (f.isEmpty()) {                        // 取消保存：回到截图界面
        show();
        activateWindow();
        setFocus();
        return;
    }
    img.save(f);
    close();
}

void ScreenshotWidget::undo()
{
    if (!m_shapes.isEmpty()) {
        m_shapes.removeLast();
        update();
    }
}
