/*
 * Web eID app, (C) 2017 Web eID team and contributors
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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

#include "internal.h"

#include <vector>

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
    SCARDCONTEXT context; // Only on unix, where it is necessary
    SCARDHANDLE card;
    DWORD protocol = SCARD_PROTOCOL_UNDEFINED;
};

// Represents a connection to a reader and a card.
// It lives in main thread but has a worker thread
class QPCSCReader: public QObject {
    Q_OBJECT
public:
    QPCSCReader(QObject *parent, const QString &name, const QString &proto): QObject(parent), reader(name), protocol(proto) {};

    // FIXME: is this necessary?
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

    void showDialog();

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
    static const char *errorName(LONG err);

    QMap<QString, QStringList> getReaders();
    SCARDCONTEXT getContext() {
        QMutexLocker locker(&mutex);
        return context;
    };

signals:
    void cardInserted(const QString &reader, const QByteArray &atr);
    void cardRemoved(const QString &reader);

    void readerAttached(const QString &name);
    void readerRemoved(const QString &name);

    void readerListChanged(const QMap<QString, QStringList> &readers); // if any of the above triggered, this will trigger as well

    void error(const QString &reader, const LONG err);

private:
    SCARDCONTEXT context;
    QMap<std::string, DWORD> known; // Known readers
    QMutex mutex; // Lock that guards the known readers
    bool pnp = true;
    QStringList stateNames(DWORD state) const;
};
