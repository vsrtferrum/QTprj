#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#define UTF8

#include <QMainWindow>
#include <QtNetwork/QUdpSocket>
#include <QTimer>

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
    void on_pushButtonSend_clicked();
    void on_pushButtonConnect_clicked();

    void onUdpConnect();
    void onUdpError(QAbstractSocket::SocketError err);
    void onUdpReadyRead();

    void onSpam();

private:
    Ui::MainWindow *ui;

    QPalette normPal;
    QPalette errPal;

    QTimer spamTimer;

    QUdpSocket *udpSocket = nullptr;

    bool isError;

    void logError(QString text);
    void log(QString text);
};
#endif // MAINWINDOW_H
