#include "mainwindow.h"
#include "screenshotwidget.h"

#include <QApplication>
#include <QCloseEvent>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
static const int kHotkeyId = 0x5A01;
#endif

// 全局快捷键：Ctrl + Alt + A（想改就改这里；微信默认是 Alt+A，会和微信冲突所以没用）
#define HOTKEY_TEXT "Ctrl+Alt+A"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle("截图小工具");
    setWindowIcon(makeIcon());
    resize(320, 160);

    auto *central = new QWidget(this);
    auto *lay = new QVBoxLayout(central);
    m_label = new QLabel(central);
    m_label->setWordWrap(true);
    m_label->setAlignment(Qt::AlignCenter);
    auto *btn = new QPushButton("开始截图", central);
    btn->setMinimumHeight(36);
    connect(btn, &QPushButton::clicked, this, &MainWindow::startCapture);
    lay->addWidget(m_label);
    lay->addWidget(btn);
    setCentralWidget(central);

    // 托盘
    m_tray = new QSystemTrayIcon(makeIcon(), this);
    m_menu = new QMenu(this);
    m_menu->addAction("截图 (" HOTKEY_TEXT ")", this, &MainWindow::startCapture);
    m_menu->addAction("显示主窗口", this, [this] { show(); raise(); activateWindow(); });
    m_menu->addSeparator();
    m_menu->addAction("退出", qApp, &QApplication::quit);
    m_tray->setContextMenu(m_menu);
    m_tray->setToolTip("截图小工具 (" HOTKEY_TEXT ")");
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::Trigger || r == QSystemTrayIcon::DoubleClick) {
            show(); raise(); activateWindow();
        }
    });
    m_tray->show();

#ifdef Q_OS_WIN
    qApp->installNativeEventFilter(this);
    m_hotkeyOk = RegisterHotKey(nullptr, kHotkeyId, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'A');
#endif
    m_label->setText(m_hotkeyOk
        ? "按 " HOTKEY_TEXT " 随时截图\n点击关闭按钮会最小化到系统托盘"
        : "全局快捷键注册失败（可能被占用）\n可使用按钮或托盘菜单截图");
}

MainWindow::~MainWindow()
{
#ifdef Q_OS_WIN
    if (m_hotkeyOk) UnregisterHotKey(nullptr, kHotkeyId);
    qApp->removeNativeEventFilter(this);
#endif
}

bool MainWindow::nativeEventFilter(const QByteArray &eventType, void *message, long *)
{
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG" || eventType == "windows_dispatcher_MSG") {
        MSG *msg = static_cast<MSG *>(message);
        if (msg->message == WM_HOTKEY && msg->wParam == kHotkeyId) {
            QMetaObject::invokeMethod(this, "startCapture", Qt::QueuedConnection);
            return true;
        }
    }
#else
    Q_UNUSED(eventType) Q_UNUSED(message)
#endif
    return false;
}

void MainWindow::startCapture()
{
    if (m_capturing) return;
    m_capturing = true;
    m_restoreMain = isVisible();
    if (m_restoreMain) hide();           // 先隐藏自己，避免截到主窗口

    QTimer::singleShot(m_restoreMain ? 250 : 60, this, [this] {
        auto *w = new ScreenshotWidget;  // WA_DeleteOnClose
        connect(w, &ScreenshotWidget::finished, this, [this] {
            m_capturing = false;
            if (m_restoreMain) show();
        });
        w->start();
    });
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    if (m_tray && m_tray->isVisible()) {
        hide();
        if (!m_tipShown) {
            m_tray->showMessage("截图小工具", "程序已最小化到托盘，按 " HOTKEY_TEXT " 截图",
                                QSystemTrayIcon::Information, 2000);
            m_tipShown = true;
        }
        e->ignore();
    } else {
        e->accept();
    }
}

QIcon MainWindow::makeIcon() const
{
    QPixmap pm(64, 64);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QColor(7, 193, 96));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(QRect(2, 2, 60, 60), 14, 14);
    QPen pen(Qt::white, 4, Qt::DashLine);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawRect(QRect(16, 18, 32, 28));
    return QIcon(pm);
}
