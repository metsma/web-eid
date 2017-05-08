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

#include "pcsc.h"
#include "internal.h"

#include <vector>

#include "dialogs/insert_card.h"
#include "dialogs/reader_in_use.h"
#include "dialogs/select_reader.h"


// Represents a connection to a reader and a card.
class QPCSCReader: public QObject {
    Q_OBJECT

public:
    QWidget *dialog; // "Reader is in use by ..." dialog

public slots:
    void send_apdu(const QByteArray &apdu);
    void disconnect();

signals:
    void apdu_received(const QByteArray &apdu);
    void disconnected();

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
