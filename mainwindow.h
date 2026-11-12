#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QAbstractNativeEventFilter>

class QSystemTrayIcon;
class QMenu;
class QLabel;

class MainWindow : public QMainWindow, public QAbstractNativeEventFilter
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    bool nativeEventFilter(const QByteArray &eventType, void *message, long *result) override;

public slots:
    void startCapture();

protected:
    void closeEvent(QCloseEvent *e) override;

private:
    QIcon makeIcon() const;

    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_menu = nullptr;
    QLabel *m_label = nullptr;
    bool m_capturing = false;
    bool m_restoreMain = false;
    bool m_tipShown = false;
    bool m_hotkeyOk = false;
};

#endif // MAINWINDOW_H
