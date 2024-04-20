#include <QtNetwork/QNetworkDatagram>
#include <QtNetwork/QNetworkInterface>
#include <QTime>
#include <QTextCodec>

#include "mainwindow.h"
#include "ui_mainwindow.h"

/// Конструктор
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Палитра для полей и кнопки
    normPal = ui->lineEditPort->palette();
    errPal = normPal;
    errPal.setColor(QPalette::Text, QColor(248, 23, 62));
    errPal.setColor(QPalette::ButtonText, QColor(248, 23, 62));

    // По нажатию Enter в поле ввода сообщения, сообщение должно отправляться
    connect(ui->lineEditMessage, &QLineEdit::returnPressed, this, &MainWindow::on_pushButtonSend_clicked);
    ui->lineEditMessage->setEnabled(false);
    ui->pushButtonSend->setEnabled(false);

    // Получение адреса текущего ПК
    auto adrs = QNetworkInterface::allAddresses();
    // В Windows (на компьютерах в Б217) выводится список с несколько другими адресами и там нужный адрес под индексом 1
    // В Linux (на личном компьютере) в списке нужный адрес находится под индексом 2
    // Также добавляю проверку на количество адресов, так как если устройство не подключено ни к какой сети,
    // то там всего 1-2 элеметов (localhost собственно)
#ifdef Q_OS_WIN
    if (adrs.length() >= 2)
    {
        QString adr = adrs[1].toString();
#else
    if (adrs.length() >= 3)
    {
        QString adr = adrs[2].toString();
#endif
        ui->labelLocalAddress->setText("Адрес компьютера: <b>" + adr + "</b>");
    }

    //#ifdef UTF8
    //    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
    //#endif

    spamTimer.setInterval(2000);
    spamTimer.start();
    connect(&spamTimer, &QTimer::timeout, this, &MainWindow::onSpam);
}

/// Деструктор
MainWindow::~MainWindow()
{
    delete ui;
}


/// Отправка введённого сообщения
void MainWindow::on_pushButtonSend_clicked()
{
    auto str = ui->lineEditMessage->text();
    if (str.length() > 0)
    {
        // Отправка данных
        QByteArray data;
        if (ui->checkBoxUtf8->isChecked())
            data = str.toUtf8();
        else
            data = str.toLocal8Bit();
        udpSocket->write(data);

        // Вывод сообщения в "консоль"
        QString ctStr = QTime::currentTime().toString("hh:mm:ss");
        log(QString("<b>[<span style=\"color: #8314C2\">Вы</span>]</b> >> %1  "
                    "<i style=\"color: #707070; font-size: 8pt;\">(%2)</i>")
            .arg(str, ctStr));

        ui->lineEditMessage->clear();
        ui->lineEditMessage->setFocus();
    }
}

/// Установка соединения
void MainWindow::on_pushButtonConnect_clicked()
{
    // Проверка введённных с формы портов
    bool okPort1, okPort2;
    quint16 port = quint16(ui->lineEditPort->text().toInt(&okPort1));
    quint16 localPort = quint16(ui->lineEditLocalPort->text().toInt(&okPort2));
    if (okPort1)
        ui->lineEditPort->setPalette(normPal);
    else
        ui->lineEditPort->setPalette(errPal);
    if (okPort2)
        ui->lineEditLocalPort->setPalette(normPal);
    else
        ui->lineEditLocalPort->setPalette(errPal);

    if (okPort1 && okPort2)
    {
        ui->pushButtonConnect->setPalette(normPal);
        ui->leftVerticalWidget->setEnabled(false);

        // Создание сокета и соединение слотов (если сокет уже существует, то его пересоздаём)
        if (udpSocket != nullptr)
        {
            // Если сокет уже открыт, то закрываем его и разрываем соединение с собеседникам,
            // после чего отправляем объект на удаление
            if (udpSocket->isOpen())
                udpSocket->close();
            //            if (udpSocket->isValid())
            //                udpSocket->disconnectFromHost();
            if (udpSocket->state() != QAbstractSocket::UnconnectedState &&
                    udpSocket->waitForDisconnected())
                udpSocket->deleteLater();
        }
        udpSocket = new QUdpSocket(this);
        connect(udpSocket, &QUdpSocket::readyRead, this, &MainWindow::onUdpReadyRead);
        connect(udpSocket, &QUdpSocket::connected, this, &MainWindow::onUdpConnect);
        // В версии Qt 5.15 добавили сигнал errorOccured, который является заменой ныне устаревшего сигнала error
#if QT_VERSION_MAJOR >= 5 && QT_VERSION_MINOR >= 15
        connect(udpSocket, &QUdpSocket::errorOccurred, this, &MainWindow::onUdpError);
#else
        connect(udpSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error), this, &MainWindow::onUdpError);
#endif

        ui->lineEditMessage->setEnabled(false);
        ui->pushButtonSend->setEnabled(false);
        ui->chatTextEdit->clear();
        log("<p style=\"color: #707070\">Создание...</p>\n");
        isError = false;

        // Устанавливаем соединение
        udpSocket->bind(QHostAddress::Any, localPort);
        udpSocket->connectToHost(ui->lineEditAddress->text(), port);
    }
    else
    {
        ui->pushButtonConnect->setPalette(errPal);
    }
}

/// Слот получения данных
void MainWindow::onUdpReadyRead()
{
    // Считываем и проверяем данные
    auto datagram = udpSocket->receiveDatagram();
    if (datagram.isValid() && datagram.data().length() > 0)
    {
        // Получаем адрес
        auto adr = datagram.senderAddress().toString().split(':').last();
        QString str;
        if (ui->checkBoxUtf8->isChecked())
            str = QString::fromUtf8(datagram.data());
        else
            str = QString::fromLocal8Bit(datagram.data());

        // Выводим сообщение в "консоль"
        QString ctStr = QTime::currentTime().toString("hh:mm:ss");
        log(QString("<b>[<span style=\"color: #C2145C\">%1</span>]</b> >> %2  "
                    "<i style=\"color: #707070; font-size: 8pt;\">(%3)</i>")
            .arg(adr, str, ctStr));
    }
}

/// Слот соединения
void MainWindow::onUdpConnect()
{
    if (!isError)
    {
        ui->lineEditMessage->setEnabled(true);
        ui->pushButtonSend->setEnabled(true);
        ui->lineEditMessage->setFocus();

        log(QString("<p style=\"color: #707070\">"
                    "Слушается адрес <span style=\"color: #8314C2\"><b>%1:%2</b></span><br>"
                    "Адрес собеседника <span style=\"color: #C2145C\"><b>%3:%4</b></span>"
                    "<br>------------- НАЧАЛО ЧАТА -------------</p>")
            .arg("0.0.0.0", ui->lineEditLocalPort->text(),
                 ui->lineEditAddress->text(), ui->lineEditPort->text()));

        ui->leftVerticalWidget->setEnabled(true);
    }
}

/// Слот ошибки
void MainWindow::onUdpError(QAbstractSocket::SocketError)
{
    isError = true;

    logError("Произошла ошибка...");

    ui->leftVerticalWidget->setEnabled(true);
}


/// Вывод "страшного" красного сообщения в "консоль"
void MainWindow::logError(QString text)
{
    log("<span style=\"color: #F8173E\">" + text + "</span>");
}

/// Вывод текста в "консоль", сокращение для ui->chatTextEdit->append
void MainWindow::log(QString text)
{
    ui->chatTextEdit->append(text);
}

/// Слот автоматической отправки сообщения по истечению времени таймера
void MainWindow::onSpam()
{
    if (ui->checkBoxSpam->isChecked() &&
            ui->lineEditMessage->isEnabled() &&
            udpSocket->isOpen())
    {
        QString str = "Hello World! Привет, мир!";
        // Отправка данных
        QByteArray data;
        if (ui->checkBoxUtf8->isChecked())
            data = str.toUtf8();
        else
            data = str.toLocal8Bit();
        udpSocket->write(data);

        // Вывод сообщения в "консоль"
        QString ctStr = QTime::currentTime().toString("hh:mm:ss");
        log(QString("<b>[<span style=\"color: #F8173E\">СПАМ</span>]</b> >> %1  "
                    "<i style=\"color: #707070; font-size: 8pt;\">(%2)</i>")
            .arg(str, ctStr));
    }
}
