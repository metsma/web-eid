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

// Represents a connection to a reader and a card.
// It lives in main thread but has a worker thread
class QPCSCReader: public QObject {
    Q_OBJECT
public:
    QPCSCReader(QObject *parent) {

    }
    QWidget *dialog; // "Reader is in use by ..." dialog

public slots:
    void connect(const QString &reader, const QString &protocol);
    void send_apdu(const QByteArray &apdu);
    void disconnect();

signals:
    void apdu_received(const QByteArray &apdu);
    void disconnected();
    void connected(const QString &protocol, const QByteArray &atr);

private:
    SCARDCONTEXT context; // Only on unix, where it is necessary
    SCARDHANDLE card;
    DWORD protocol = SCARD_PROTOCOL_UNDEFINED;
};

// Synthesizes PC/SC events to Qt signals
class QtPCSC: public QThread {
    Q_OBJECT

public:
    void run();
    void cancel();
    static const char *errorName(LONG err);

    QMap<QString, QStringList> getReaders();
    QPCSCReader connect(const QString &reader);

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
