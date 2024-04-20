#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpServer>
#include <QTcpSocket>
#include <QVector>
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
    void on_radioButtonServer_clicked();
    void on_radioButtonClient_clicked();
    void on_pushButtonCreate_clicked();
    void on_pushButtonConnect_clicked();
    void on_pushButtonSend_clicked();

    void onServerConnection();

    void onSocketDisconneted();
    void onSocketReadyRead();

    void on_pushButtonAddresses_clicked();

    void onSpamTimeout();

private:
    Ui::MainWindow *ui;

    QTcpServer *server = nullptr;
    QVector<QTcpSocket *> sockets;
    QVector<QString> nicknames;

    bool socketClearing = false;

    QTimer *spamTimer = nullptr;

    void clearServer();
    void clearSockets();
};
#endif // MAINWINDOW_H
