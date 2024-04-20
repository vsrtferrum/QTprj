#include <QNetworkInterface>
#include "mainwindow.h"
#include "ui_mainwindow.h"

// Конструктор
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Скрываем все элементы
    ui->labelPort->hide();
    ui->labelAddress->hide();
    ui->lineEditPort->hide();
    ui->lineEditAddress->hide();
    ui->pushButtonConnect->hide();
    ui->pushButtonCreate->hide();
    ui->pushButtonAddresses->hide();
    ui->labelNickname->hide();
    ui->lineEditNickname->hide();

    ui->lineEditMessage->setEnabled(false);
    ui->pushButtonSend->setEnabled(false);
    ui->checkBoxSpam->setEnabled(false);

    // Соединение сигнала нажатия на Enter с слотом отправки сообщения
    connect(ui->lineEditMessage, &QLineEdit::returnPressed, this, &MainWindow::on_pushButtonSend_clicked);

    // Создание, запуск и подключение таймера для спама
    spamTimer = new QTimer(this);
    spamTimer->setInterval(5000);
    connect(spamTimer, &QTimer::timeout, this, &MainWindow::onSpamTimeout);
    spamTimer->start();
}

// Деструктор
MainWindow::~MainWindow()
{
    // Очищаем сокеты при отключении
    clearServer();
    clearSockets();

    delete ui;
}

// Выбор режима сервера
void MainWindow::on_radioButtonServer_clicked()
{
    // Переключаемся в режим сервера
    ui->labelPort->show();
    ui->labelAddress->hide();
    ui->lineEditPort->show();
    ui->lineEditAddress->hide();
    ui->pushButtonConnect->hide();
    ui->pushButtonCreate->show();
    ui->pushButtonAddresses->show();
    ui->labelNickname->hide();
    ui->lineEditNickname->hide();
}

// Выбор режима клиента
void MainWindow::on_radioButtonClient_clicked()
{
    // Переключаемся в режим клиента
    ui->labelPort->show();
    ui->labelAddress->show();
    ui->lineEditPort->show();
    ui->lineEditAddress->show();
    ui->pushButtonConnect->show();
    ui->pushButtonCreate->hide();
    ui->pushButtonAddresses->hide();
    ui->labelNickname->show();
    ui->lineEditNickname->show();
}

// Открытие сервера
void MainWindow::on_pushButtonCreate_clicked()
{
    if (server == nullptr)
    {
        // Берём порт для подключения с формы
        int port = ui->lineEditPort->text().toInt();

        if (port > 0 && port < 0x10000)
        {
            // Порт правильный

            // Закрываем все существующие соединения (если есть) и удаляем оставшиеся классы
            clearServer();
            clearSockets();

            // Создаём сервер
            server = new QTcpServer(this);
            connect(server, &QTcpServer::newConnection, this, &MainWindow::onServerConnection);
            // Открываем сервер
            if (server->listen(QHostAddress::Any, quint16(port)))
            {
                // Если удалось открыть
                ui->textEditInput->append("Сервер открыт на порте " + QString::number(port));

                ui->pushButtonCreate->setText("Закрыть");
            }
            else
            {
                // Если не удалось открыть
                ui->textEditInput->append("Не удалось запустить сервер");
                // Очищаем уже не нужный сервер
                server->deleteLater();
                server = nullptr;
            }
        }
        else
        {
            // Порт неправильный
            ui->textEditInput->append("ВВЕДЁН НЕПРАВИЛЬНЫЙ ПОРТ!");
        }
    }
    else
    {
        // Закрываем все существующие соединения (если есть) и удаляем оставшиеся классы
        clearServer();
        clearSockets();

        ui->textEditInput->append("Сервер закрыт");
    }
}

// Обработка нового подключения
void MainWindow::onServerConnection()
{
    // Получаем новое соединение с клиентом
    QTcpSocket *newSocket = server->nextPendingConnection();
    if (newSocket != nullptr)
    {
        // Соединяем слоты, добавляем сокет в вектор
        connect(newSocket, &QTcpSocket::disconnected, this, &MainWindow::onSocketDisconneted);
        connect(newSocket, &QTcpSocket::readyRead, this, &MainWindow::onSocketReadyRead);
        sockets.append(newSocket);

        // Приводим адрес к строке
        QString addressStr = newSocket->localAddress().toString().split(':').last();
        ui->textEditInput->append("Клиент " + addressStr + " подключился");

        ui->lineEditMessage->setEnabled(true);
        ui->pushButtonSend->setEnabled(true);
        ui->checkBoxSpam->setEnabled(true);
    }
}

// Обработка отключения от сокета
void MainWindow::onSocketDisconneted()
{
    if (server == nullptr)
    {
        // Сейчас режим клиента
        if (!socketClearing)
        {
            ui->textEditInput->append("Вы отключились от сервера");
            // Удаляем единственный сокет подключения
            clearSockets();
        }

        ui->lineEditMessage->setEnabled(false);
        ui->pushButtonSend->setEnabled(false);
        ui->checkBoxSpam->setEnabled(false);
    }
    else
    {
        // Сейчас режим сервера
        QTcpSocket *senderSocket = static_cast<QTcpSocket *>(sender());

        // Узнаём по указателю на сокет, индекс пользователя в массиве
        int userIndex = sockets.indexOf(senderSocket);
        // Берём из массива ник пользователя, который отправил это сообщение
        QString nick = nicknames[userIndex];
        ui->textEditInput->append("Клиент " + nick + " отключился");

        // Формируем данные для отправки другим клиентам
        QByteArray disconnectData = nick.toUtf8();
        disconnectData.prepend(char(1));
        // Отправляем данные другим клиентам
        for (int i = 0; i < sockets.length(); i++)
        {
            if (sockets[i] != senderSocket)
                sockets[i]->write(disconnectData);
        }

        // Удаляем отключившегося клиента из массивов
        sockets.removeAt(userIndex);
        nicknames.removeAt(userIndex);

        if (sockets.length() == 0)
        {
            // Если уже все клиенты отключились
            ui->lineEditMessage->setEnabled(false);
            ui->pushButtonSend->setEnabled(false);
            ui->checkBoxSpam->setEnabled(false);
        }
    }
}

// Обработка чтения данных из сокета
void MainWindow::onSocketReadyRead()
{
    // Получаем указатель на сокет, который вызвал сигнал
    QTcpSocket *senderSocket = static_cast<QTcpSocket *>(sender());
    if (senderSocket != nullptr)
    {
        // Считываем данные из сокета
        QByteArray data = senderSocket->readAll();
        if (data.length() > 0)
        {
            char command = data[0]; // Считываем команду
            data = data.mid(1);
            // Проверка в каком режиме запущена программа в данный момент
            if (server == nullptr)
            {
                // Если сейчас запущен режим клиента:
                if (command == 0)
                {
                    // Пользователь подключился
                    QString nick = QString::fromUtf8(data);
                    ui->textEditInput->append("Пользователь "+nick+" подключился");
                }
                else if (command == 1)
                {
                    // Пользователь отключился
                    QString nick = QString::fromUtf8(data);
                    ui->textEditInput->append("Пользователь "+nick+" отключился");
                }
                else if (command == 2)
                {
                    // Сервер отправил сообщение
                    QString text = QString::fromUtf8(data);
                    ui->textEditInput->append("Сервер: " + text);
                }
                else if (command == 3)
                {
                    // Пользователь отправил сообщение
                    // Получаем индекс первого байта, где у нас записан 0
                    // 0 - разделитель между ником и строкой
                    int nickSepIndex = data.indexOf(char(0));

                    // Дополнительная проверка на "склейку" пакетов при спаме
                    int sepIndex = data.indexOf(char(1));
                    if (sepIndex != -1)
                    {
                        // Считываем данные с начала до разделителя (не включительно)
                        QString nick = QString::fromUtf8(data.left(nickSepIndex));
                        // Считываем данные после индекса и дополнительного флага
                        QString text = QString::fromUtf8(data.mid(nickSepIndex + 1, sepIndex - nickSepIndex));
                        ui->textEditInput->append(nick+": " + text);

                        data = data.mid(sepIndex + 1);
                        // Пользователь отключился
                        nick = QString::fromUtf8(data);
                        ui->textEditInput->append("Пользователь "+nick+" отключился");

                    }
                    else
                    {
                        // Считываем данные с начала до разделителя (не включительно)
                        QString nick = QString::fromUtf8(data.left(nickSepIndex));
                        // Считываем данные после индекса и до конца
                        QString text = QString::fromUtf8(data.mid(nickSepIndex + 1));
                        ui->textEditInput->append(nick+": " + text);
                    }
                }
            }
            else
            {
                // Если сейчас запущен режим сервера:
                // Превращаем байты в строку

                if (command == 0)
                {
                    // Если пользователь отправляет свой ник
                    QString nick = QString::fromUtf8(data);
                    nicknames.append(nick); // Добавляем ник в массив
                    // Приводим адрес к строке
                    // ":fff0:127.0.0.1".split(':') => ["","fff0","127.0.0.1"].last() => "127.0.0.1"
                    /*
                     * "Hello World!" => ['H','e','l','l','o',' ',...'l','d','!']
                     * "Hello World!".split(' ') => ["Hello","World!"]
                     * "1,2,3,4,5".split(',')    => ["1", "2", "3", "4", "5"]
                     * ["Hello","World!"].last() => "World!"
                     * [1,2,3,4,5].last()        => 5
                     */
                    QString addressStr = senderSocket->localAddress().toString().split(':').last();
                    ui->textEditInput->append(addressStr+" называет себя \""+nick+"\"");

                    // Формируем данные для отправки другим клиентам
                    QByteArray connectData = data;
                    connectData.prepend(char(0));
                    // Отправляем данные другим клиентам
                    for (int i = 0; i < sockets.length(); i++)
                    {
                        if (sockets[i] != senderSocket)
                            sockets[i]->write(connectData);
                    }
                }
                else if (command == 1)
                {
                    // Если пользователь отправляет сообщение
                    // Узнаём по указателю на сокет, индекс пользователя в массиве
                    int userIndex = sockets.indexOf(senderSocket);
                    // Берём из массива ник пользователя, который отправил это сообщение
                    QString nick = nicknames[userIndex];
                    QString text = QString::fromUtf8(data);
                    ui->textEditInput->append(nick+": " + text);

                    // Формируем данные для отправки другим клиентам
                    QByteArray messageData = data;
                    messageData.prepend(char(0)); // Добавляем разделитель между текстом и ником
                    messageData.prepend(nick.toUtf8()); // Добавляем ник
                    messageData.prepend(char(3));
                    // Отправляем данные другим клиентам
                    for (int i = 0; i < sockets.length(); i++)
                    {
                        if (sockets[i] != senderSocket)
                            sockets[i]->write(messageData);
                    }
                }
            }
        }
    }
}

// Подключение к серверу
void MainWindow::on_pushButtonConnect_clicked()
{
    if (sockets.length() == 0)
    {
        // Берём порт для подключения с формы
        int port = ui->lineEditPort->text().toInt();
        // Берём ник с формы (убираем пробелы с начала и конца строки)
        QString nick = ui->lineEditNickname->text().trimmed();

        if (port > 0 && port < 0x10000)
        {
            // Порт правильный
            if (nick.size() > 0)
            {
                // Закрываем все существующие соединения (если есть) и удаляем оставшиеся классы
                clearServer();
                clearSockets();

                // Создаём сокет
                QTcpSocket *newSocket = new QTcpSocket(this);
                // Пробуем подключиться
                newSocket->connectToHost(ui->lineEditAddress->text(), quint16(port));
                if (newSocket->waitForConnected(5000))
                {
                    // Если удалось подключиться
                    ui->textEditInput->append("Подключение установлено");
                    // Соединяем сигналы
                    connect(newSocket, &QTcpSocket::disconnected, this, &MainWindow::onSocketDisconneted);
                    connect(newSocket, &QTcpSocket::readyRead, this, &MainWindow::onSocketReadyRead);
                    sockets.append(newSocket);

                    // Отправляем серверу свой ник
                    QByteArray nickData = nick.toUtf8();
                    nickData.prepend(char(0));
                    newSocket->write(nickData);

                    ui->lineEditMessage->setEnabled(true);
                    ui->pushButtonSend->setEnabled(true);
                    ui->checkBoxSpam->setEnabled(true);

                    ui->pushButtonConnect->setText("Отключиться");
                    ui->lineEditNickname->setEnabled(false);
                }
                else
                {
                    // Если не удалось подключиться
                    ui->textEditInput->append("Ошибка подключения");
                    // Удаляем сокет (так как он больше не нужен)
                    newSocket->deleteLater();
                }
            }
            else
            {
                ui->textEditInput->append("ВВЕДЁН НЕПРАВИЛЬНЫЙ НИК!");
            }
        }
        else
        {
            ui->textEditInput->append("ВВЕДЁН НЕПРАВИЛЬНЫЙ ПОРТ!");
        }
    }
    else
    {
        // Закрываем все существующие соединения (если есть) и удаляем оставшиеся классы
        clearServer();
        clearSockets();
    }
}

// Отправка сообщения
void MainWindow::on_pushButtonSend_clicked()
{
    // Получаем текст с формы
    QString text = ui->lineEditMessage->text();
    if (text.length() > 0)
    {
        ui->textEditOutput->append(text);

        // Приводим текст в байты
        QByteArray data = text.toUtf8();
        // Если включён режим сервера, то мы должны вставить команду 2
        // Если включён режим клиента, то команду 1
        if (server == nullptr)
            data.prepend(char(1));
        else
            data.prepend(char(2));
        // Если есть активные подключения
        for (int i = 0; i < sockets.length(); i++)
        {
            // то отправляем данные
            sockets[i]->write(data);
        }

        // Очищаем поле ввода и ставим фокус на нём
        ui->lineEditMessage->clear();
        ui->lineEditMessage->setFocus();
    }
}

// Закрытие и удаление сервера
void MainWindow::clearServer()
{
    if (server != nullptr)
    {
        // Если сервер слушает порт, то закрываем его
        if (server->isListening())
            server->close();
        // и отправляем класс сервера на удаление
        server->deleteLater();
        server = nullptr;
    }

    ui->pushButtonCreate->setText("Создать");
}

// Закрытие и удаление сокетоа
void MainWindow::clearSockets()
{
    socketClearing = true; // Включаем флаг
    for (int i = 0; i < sockets.length(); i++)
    {
        // Для каждого сокета в векторе:
        // Если сокет открыт, то закрываем соединение
        if (sockets[i]->isOpen())
        {
            sockets[i]->close();
            sockets[i]->disconnect();
        }
        // и отправляет класс на удаление
        sockets[i]->deleteLater();
    }
    socketClearing = false; // Отключаем флаг
    // Удаляем оставшиеся указатели
    sockets.clear();
    nicknames.clear();

    ui->pushButtonConnect->setText("Подключиться");
    ui->lineEditNickname->setEnabled(true);
}

// Вывод адресов компьютера
void MainWindow::on_pushButtonAddresses_clicked()
{
    // Получаем все адреса, которые принадлежат компьютеру
    QList<QHostAddress> addresses = QNetworkInterface::allAddresses();

    ui->textEditInput->append("Адреса компьютера:");
    for (int i = 0; i < addresses.length(); i++)
    {
        // Выводим только IPv4 адреса (в "более красивой" форме, только сам адрес)
        if (addresses[i].protocol() == QAbstractSocket::IPv4Protocol)
            ui->textEditInput->append(addresses[i].toString().split(':').last());
    }
}

// Отправка спама в чат
void MainWindow::onSpamTimeout()
{
    // Проверяем, что чекбокс активен и нажат
    if (ui->checkBoxSpam->isEnabled() && ui->checkBoxSpam->isChecked())
    {
        // Текст спама
        QString text = "СПАМ!!! СПАМ!!!";
        // Приводим текст в байты
        QByteArray data = text.toUtf8();
        // Если включён режим сервера, то мы должны вставить команду 2
        // Если включён режим клиента, то команду 1
        if (server == nullptr)
            data.prepend(char(1));
        else
            data.prepend(char(2));
        // Если есть активные подключения
        for (int i = 0; i < sockets.length(); i++)
        {
            // то отправляем данные
            sockets[i]->write(data);
        }

        ui->textEditOutput->append(text);
    }
}
