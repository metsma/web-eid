/*
 * Copyright (C) 2017 Martin Paljak
 */

#pragma once

#include <QThread>
#include <QMutex>
#include <QPair>

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
    QPCSCReader(WebContext *webcontext, QtPCSC *pcsc, const QString &name, const QString &proto): QObject(webcontext), name(name), PCSC(pcsc), protocol(proto) {};

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

// Synthesizes PC/SC events to Qt signals
class QtPCSC: public QThread {
    Q_OBJECT

public:
    void run();
    void cancel();

    QMap<QString, QPair<QByteArray, QStringList>> getReaders();
    QPCSCReader *connectReader(WebContext *webcontext, const QString &reader, const QString &protocol, bool wait);

    static const char *errorName(LONG err);

signals:
    void cardInserted(const QString &reader, const QByteArray &atr, const QStringList flags);
    void cardRemoved(const QString &reader);

    void readerAttached(const QString &name);
    void readerRemoved(const QString &name);

    void readerListChanged(const QMap<QString, QPair<QByteArray, QStringList>> &readers); // if any of the above triggered, this will trigger as well

    void error(const QString &reader, const LONG err);

private:
    SCARDCONTEXT getContext() {
        QMutexLocker locker(&mutex);
        return context;
    };

    SCARDCONTEXT context = 0;
    QMap<QString, QPair<QByteArray, DWORD>> known; // Known readers
    QMutex mutex; // Lock that guards the known readers
    bool pnp = true;
    const char *pnpReaderName = "\\\\?PnP?\\Notification";
};
