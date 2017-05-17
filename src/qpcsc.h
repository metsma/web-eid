/*
 * Copyright (C) 2017 Martin Paljak
 */

#pragma once

#include <QThread>
#include <QMutex>

#ifdef __APPLE__
#include <PCSC/winscard.h>
#include <PCSC/wintypes.h>
#else
#undef UNICODE
#include <winscard.h>
#endif

#include "context.h"

#include "dialogs/reader_in_use.h"
#include "dialogs/insert_card.h"

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
    void disconnectCard();

signals:
    // When the connection has been established
    void connected(const QByteArray &atr, const QString &protocol);
    // When reader is disconnected, either implicitly during connect or transmit
    // or explicitly via disconnect()
    void disconnected(const LONG err);
    // bytes received from the card after transmit()
    void received(const QByteArray &bytes);

private:
    SCARDCONTEXT context = 0; // Only required on unix
    SCARDHANDLE card = 0;
    DWORD protocol = SCARD_PROTOCOL_UNDEFINED;
};

// Represents a connection to a reader and a card.
// It lives in main thread but has a worker thread
class QPCSCReader: public QObject {
    Q_OBJECT
public:
    QPCSCReader(WebContext *webcontext, QtPCSC *pcsc, const QString &name, const QString &proto): QObject(webcontext), PCSC(pcsc), reader(name), protocol(proto) {};

    ~QPCSCReader() {
        if (thread.isRunning()) {
            thread.quit();
            thread.wait();
        }
    }

    bool isConnected() {
        return isOpen;
    };

public slots:
    void open();
    void transmit(const QByteArray &apdu);
    void disconnect();

    void cardInserted(const QString &reader, const QByteArray &atr);
    void readerRemoved(const QString &reader);

signals:
    // command signals
    void connectCard(const QString &reader, const QString &protocol);
    void disconnectCard();
    void transmitBytes(const QByteArray &bytes);

    // Proxied signals
    void received(const QByteArray &apdu);
    void disconnected(const LONG err);
    void connected(const QByteArray &atr, const QString &protocol);

private:

    bool isOpen = false;
    QtPCSC *PCSC;
    QString reader;
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

    QMap<QString, QStringList> getReaders();
    QPCSCReader *connectReader(WebContext *webcontext, const QString &reader, const QString &protocol, bool wait);

    static const char *errorName(LONG err);

signals:
    void cardInserted(const QString &reader, const QByteArray &atr);
    void cardRemoved(const QString &reader);

    void readerAttached(const QString &name);
    void readerRemoved(const QString &name);

    void readerListChanged(const QMap<QString, QStringList> &readers); // if any of the above triggered, this will trigger as well

    void error(const QString &reader, const LONG err);

private:
    SCARDCONTEXT getContext() {
        QMutexLocker locker(&mutex);
        return context;
    };

    SCARDCONTEXT context = 0;
    QMap<std::string, DWORD> known; // Known readers
    QMutex mutex; // Lock that guards the known readers
    bool pnp = true;
};
