/*
 * Copyright (C) 2017 Martin Paljak
 */

#pragma once

#include <QThread>
#include <QMutex>
#include <QPair>

#include "Logger.h"

#ifdef __APPLE__
#include <PCSC/winscard.h>
#include <PCSC/wintypes.h>
#else
#undef UNICODE
#include <winscard.h>
#endif

#include "context.h"

class QtPCSC;

// Lives in a separate thread because of possibly blocking
// transmits, that owns the context and card handles
class QPCSCReaderWorker: public QObject {
    Q_OBJECT

public:
    ~QPCSCReaderWorker();

public slots:
    // establish context in thread and connect to reader
    void connectCard(const QString &reader, const QString &protocol);
    void transmit(const QByteArray &bytes);
    void reconnectCard(const QString &protocol);
    void disconnectCard();

signals:
    // When the connection has been established
    void connected(const QByteArray &atr, const QString &protocol);
    void reconnected(const QByteArray &atr, const QString &protocol);
    // When reader is disconnected, either implicitly during connect or transmit
    // or explicitly via disconnect()
    void disconnected(const LONG err);
    // bytes received from the card after transmit()
    void received(const QByteArray &bytes);

private:
    SCARDCONTEXT context = 0; // Only required on unix
    SCARDHANDLE card = 0;
    DWORD protocol = SCARD_PROTOCOL_UNDEFINED;
    DWORD mode = SCARD_SHARE_EXCLUSIVE;
    QString name;
};

// Represents a connection to a reader and a card.
// It lives in main thread but has a worker thread
class QPCSCReader: public QObject {
    Q_OBJECT
public:
    QPCSCReader(WebContext *webcontext, QtPCSC *pcsc, const QString &name, const QString &proto): QObject(webcontext), name(name), PCSC(pcsc), protocol(proto) {
        setObjectName(name);
    };

    ~QPCSCReader() {
        if (thread.isRunning()) {
            thread.quit();
            thread.wait();
        }
    }

    bool isConnected() {
        return isOpen;
    };
    QString name;

public slots:
    void open();
    void transmit(const QByteArray &apdu);
    void reconnect(const QString &protocol);
    void disconnect();

    void cardInserted(const QString &reader, const QByteArray &atr, const QStringList &flags);
    void readerRemoved(const QString &reader);

signals:
    // command signals
    void connectCard(const QString &reader, const QString &protocol);
    void reconnectCard(const QString &protocol);
    void disconnectCard();
    void transmitBytes(const QByteArray &bytes);

    // Proxied signals
    void received(const QByteArray &apdu);
    void disconnected(const LONG err);
    void connected(const QByteArray &atr, const QString &protocol);
    void reconnected(const QByteArray &atr, const QString &protocol);

private:
    bool isOpen = false;
    QtPCSC *PCSC;
    QString protocol;
    QThread thread;
    QPCSCReaderWorker worker;
};


class QPCSCEventWorker: public QObject {
    Q_OBJECT

public slots:
    void start();
    SCARDCONTEXT getContext() {
        QMutexLocker locker(&mutex);
        return context;
    };

    QMap<QString, QPair<QByteArray, QStringList>> getReaders();

signals:
    void stopped(LONG rv);

    void cardInserted(const QString &reader, const QByteArray &atr, const QStringList flags);
    void cardRemoved(const QString &reader);

    void readerAttached(const QString &name);
    void readerRemoved(const QString &name);

    void readerChanged(const QString &reader, const QByteArray &atr, const QStringList flags);

    void readerListChanged(const QMap<QString, QPair<QByteArray, QStringList>> &readers); // if any of the above triggered, this will trigger as well

private:
    SCARDCONTEXT context = 0;
    bool pnp = true;
    const char *pnpReaderName = "\\\\?PnP?\\Notification";
    QMap<QString, QPair<QByteArray, DWORD>> known; // Known readers
    QMutex mutex; // Lock that guards the known readers
};


// Synthesizes PC/SC events to Qt signals
class QtPCSC: public QObject {
    Q_OBJECT

public:
    QtPCSC() {
        thread.start();
        worker.moveToThread(&thread);
        running = true;
        connect(&worker, &QPCSCEventWorker::stopped, this, [this] (LONG rv) {
            running = false;
            _log("PCSC stopped: %s", errorName(rv));
        }, Qt::QueuedConnection);
        connect(&worker, &QPCSCEventWorker::cardInserted, this, &QtPCSC::cardInserted, Qt::QueuedConnection);
        connect(&worker, &QPCSCEventWorker::cardRemoved, this, &QtPCSC::cardRemoved, Qt::QueuedConnection);
        connect(&worker, &QPCSCEventWorker::readerAttached, this, &QtPCSC::readerAttached, Qt::QueuedConnection);
        connect(&worker, &QPCSCEventWorker::readerRemoved, this, &QtPCSC::readerRemoved, Qt::QueuedConnection);
        connect(&worker, &QPCSCEventWorker::readerChanged, this, &QtPCSC::readerChanged, Qt::QueuedConnection);
        connect(&worker, &QPCSCEventWorker::readerListChanged, this, &QtPCSC::readerListChanged, Qt::QueuedConnection);
        connect(this, &QtPCSC::startSignal, &worker, &QPCSCEventWorker::start, Qt::QueuedConnection);
        emit startSignal();
    }

    void cancel();

    void start() {
        if (!running) {
            running = true;
            return emit startSignal();
        } else {
            _log("Already running");
        }
    }
    QMap<QString, QPair<QByteArray, QStringList>> getReaders();
    QPCSCReader *connectReader(WebContext *webcontext, const QString &reader, const QString &protocol, bool wait);

    static const char *errorName(LONG err);

    ~QtPCSC() {
        if (running) {
            cancel();
        }
        thread.quit();
        thread.wait();
    }
signals:
    void cardInserted(const QString &reader, const QByteArray &atr, const QStringList flags);
    void cardRemoved(const QString &reader);

    void readerAttached(const QString &name);
    void readerRemoved(const QString &name);

    void readerListChanged(const QMap<QString, QPair<QByteArray, QStringList>> &readers); // if any of the above triggered, this will trigger as well

    void readerChanged(const QString &reader, const QByteArray &atr, const QStringList flags);

    void error(const QString &reader, const LONG err);

    void startSignal();
private:
    bool running = false;
    QMap<QString, QPair<QByteArray, DWORD>> known; // Known readers

    QThread thread;
    QPCSCEventWorker worker;
};
