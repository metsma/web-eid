/*
 * Copyright (C) 2017 Martin Paljak
 */

#include "../Logger.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QLocalSocket>
#include <QProcess>
#include <QTimer>
#include <QDir>
#include <QFile>
#include <QThread>
#include <QStandardPaths>

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <iostream>

#ifdef _WIN32
#include <Windows.h>
// for setting stdio mode
#include <fcntl.h>
#include <io.h>
#else
#include <unistd.h>
#endif

class InputChecker: public QThread {
    Q_OBJECT

public:
    InputChecker(QObject *parent): QThread(parent) {}

    void run() {
        setTerminationEnabled(true);
        quint32 messageLength = 0;
        _log("Waiting for messages");
        // Here we do busy-sleep
        while (std::cin.read((char*)&messageLength, sizeof(messageLength))) {
            _log("Message size: %u", messageLength);
            QByteArray msg(int(messageLength), 0);
            std::cin.read(msg.data(), msg.size());
            _log("Message (%u): %s", messageLength, msg.constData());
            emit messageReceived(msg);
        }
        _log("Input reading thread is done.");
        // If input is closed, we quit
        QCoreApplication::exit(0);
    }

signals:
    void messageReceived(const QByteArray &msg);
};

class NMBridge: public QCoreApplication
{
    Q_OBJECT

public:
    NMBridge(int &argc, char *argv[], const QString &browser_name) : QCoreApplication(argc, argv),
        sock(new QLocalSocket(this)),
        browser(browser_name)
    {
        Logger::setFile("nm-bridge.log");
        _log("Running %s", qPrintable(applicationFilePath()));

        QCommandLineParser parser;
        QCommandLineOption debug("debug");
        parser.addOption(debug);
        parser.process(arguments());
        this->dbg = parser.isSet(debug);

        // TODO: figure out the right paths depending on free-form app location
#if defined(Q_OS_MACOS)
        serverApp = "open -b com.web-eid.app";
        // /tmp/martin-webeid
        serverName = QDir("/tmp").filePath(qgetenv("USER") + "-webeid");
#elif defined(Q_OS_WIN32)
        serverApp = QDir::toNativeSeparators(QDir(QCoreApplication::applicationDirPath()).filePath("Web-eID.exe"));

        // \\.\pipe\Martin_Paljak-webeid
        serverName = qgetenv("USERNAME").simplified().replace(" ", "_") + "-webeid";
#elif defined(Q_OS_LINUX)
        serverApp = "/usr/bin/web-eid";

        // /run/user/1000/webeid-socket
        serverName = QDir(QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)).filePath("webeid-socket");
#else
#error "Unsupported platform"
#endif
        // Override, for testing purposes
        if (qEnvironmentVariableIsSet("WEB_EID_APP"))
            serverApp = qgetenv("WEB_EID_APP");

        _log("Connecting to %s", qPrintable(serverName));
        _log("Running \"%s\" to get server", qPrintable(serverApp));

        // Starting app
        connect(sock, &QLocalSocket::connected, this, &NMBridge::connected);

        connect(sock, static_cast<void(QLocalSocket::*)(QLocalSocket::LocalSocketError)>(&QLocalSocket::error), [this] (QLocalSocket::LocalSocketError socketError) {
            if (socketError == QLocalSocket::PeerClosedError) {
                _log("QLocalSocket::PeerClosedError");
                quit();
                return;
            } else if (socketError == QLocalSocket::ConnectionRefusedError) {
                _log("QLocalSocket::ConnectionRefusedError");
            } else if (socketError == QLocalSocket::ServerNotFoundError) {
                _log("QLocalSocket::ServerNotFoundError");
            }
            if ((socketError == QLocalSocket::ConnectionRefusedError) || (socketError == QLocalSocket::ServerNotFoundError)) {
                // Start the server
                if (!server_started) {
                    // leave some time for startup
                    // TODO: possibly use QFileSystemWatcher ?
                    QTimer::singleShot(1000, [this] {sock->connectToServer(serverName);});
                    // TODO: set working folder
                    if (QProcess::startDetached(serverApp, this->dbg ? QStringList("--debug") : QStringList())) {
                        server_started++;
                        _log("Started %s", qPrintable(serverApp));
                    } else {
                        _log("Could not start server");
                    }
                } else {
                    if (server_started < 10) {
                        _log("Server has already been started, trying to reconnect (%d)", server_started);
                        server_started++;
                        QTimer::singleShot(500, [this] {sock->connectToServer(serverName);});
                    }
                }
            } else {
                _log("Connection failed: %d", socketError);
            }
        });

        out.open(stdout, QFile::WriteOnly);
        input = new InputChecker(this);
        connect(input, &InputChecker::messageReceived, this, &NMBridge::messageFromBrowser, Qt::QueuedConnection);

        connect(sock, &QLocalSocket::disconnected, [this] {
            // XXX: on Linux, error() with QLocalSocket::PeerClosedError is also thrown
            _log("Socket disconnected");
            quit();
        });
        sock->connectToServer(serverName);
        connect(this, &QCoreApplication::aboutToQuit, [this] {
#ifdef Q_OS_WIN
            input->terminate();
#else
            close(0);
            input->wait();
#endif
        });
    }

public slots:
    void connected() {
        _log("Connected to %s", qPrintable(sock->fullServerName()));
        server_started = 0;
        // Start input reading thread, if not already running
        if (!input->isRunning()) {
            input->start();
        }

        connect(sock, &QLocalSocket::readyRead, [this] {
            // Data available from app, read message and pass to browser
            _log("Handling message from application");
            _log("%d bytes available from app", sock->bytesAvailable());
            quint32 msgsize = 0;
            if (sock->bytesAvailable() < qint64(sizeof(msgsize) + 1)) {
                _log("Not enought data available %d, waiting for next update", sock->bytesAvailable());
                return;
            }
            if (sock->read((char*)&msgsize, sizeof(msgsize)) == sizeof(msgsize)) {
                if (msgsize > 8 * 1024) {
                    _log("Bad message size, closing");
                    sock->abort();
                    return;
                }
                QByteArray msg(int(msgsize), 0);
                qint64 readsize = sock->read(msg.data(), msgsize) ;
                if (readsize == msgsize) {
                    _log("Read message of %d bytes", msgsize);
                    // Pass verbatim
                    quint32 responseLength = msg.size();
                    _log("Response(%u) %s", responseLength, msg.constData());
                    out.write((const char*)&responseLength, sizeof(responseLength));
                    out.write(msg);
                    out.flush();
                } else {
                    _log("Could not read message, read %d of %d", readsize, msgsize);
                    sock->abort();
                }
            } else {
                _log("Could not read message size");
                // TODO: shutdown? close? abort?
                sock->abort();
            }
        });
    }

    void messageFromBrowser(const QByteArray &msg) {
        _log("Handling message from browser");
        quint32 responseLength = msg.size();
        // TODO: error handling?
        // TODO: add browser type (means parsign JSON)
        sock->write((const char*)&responseLength, sizeof(responseLength));
        sock->write(msg);
    }

private:
    // We have a single connection to the server app
    QLocalSocket *sock;
    int server_started = 0;
    QString serverName;
    QString serverApp;
    InputChecker *input;
    QFile out;
    QString browser;
    bool dbg = false;
};

int main(int argc, char *argv[])
{
    const char *msg = "This is not a regular program, it is expected to be run from a browser.\n";
    // Check if run as a browser extension
    if (argc < 2) {
        printf("%s", msg);
        exit(1);
    }
    std::string arg1(argv[1]);
    QString browser = "unknown";
    if (arg1.find("chrome-extension://") == 0) {
        browser = "chrome";
    } else if (QFile::exists(QString::fromStdString(arg1))) {
        browser = "firefox";
    }
    // Check that input is a pipe (the app is not run from command line)
    bool isPipe = false;
#ifdef _WIN32
    isPipe = GetFileType(GetStdHandle(STD_INPUT_HANDLE)) == FILE_TYPE_PIPE;
#else
    struct stat sb;
    if (fstat(fileno(stdin), &sb) != 0) {
        exit(1);
    }
    isPipe = S_ISFIFO(sb.st_mode);
#endif
    if (!isPipe) {
        printf("%s", msg);
// FIXME        exit(1);
    }
#ifdef _WIN32
    // Set files to binary mode, to be able to read the uint32 msg size
    _setmode(_fileno(stdin), O_BINARY);
    _setmode(_fileno(stdout), O_BINARY);
#endif
    return NMBridge(argc, argv, browser).exec();
}

#include "nm-bridge.moc"
