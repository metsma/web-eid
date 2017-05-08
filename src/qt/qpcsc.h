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

#include <QObject>

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
class QtPCSC: public QObject {
    Q_OBJECT

public:
    // Ongoing dialogs of PCSC subsystem
    QtInsertCard insert_dialog; // FIXME: remove 
    QtReaderInUse inuse_dialog; // FIXME: remove 
    QtSelectReader select_dialog; // FIXME: remove 

    QtPCSC() {
        connect(&this->select_dialog, &QtSelectReader::reader_selected, this, &QtPCSC::reader_selected, Qt::QueuedConnection);
    }

    static const char *errorName(LONG err);

public slots:
    void connect_reader(const QString &protocol);
    void send_apdu(const QByteArray &apdu);
    void disconnect_reader();

    void reader_selected(const LONG status, const QString &reader, const QString &protocol);
    void cancel_reader(); // Signalled from QtReaderInUse dialog
    void receiveIPC(InternalMessage message);

signals:
    // Useful signals
    void cardInserted(); // emitted when a new card is inserted. Forces PKI to refresh cert list
    void cardRemoved();

    void readerAttached();
    void readerRemoved();

    void readerListChanged(); // if any of the above triggered, this will trigger as well

    // Old slots. remove.
    void reader_connected(LONG status, const QString &reader, const QString &protocol, const QByteArray &atr);
    void apdu_sent(LONG status, const QByteArray &response);
    void reader_disconnected();
    void show_insert_card(bool show, const QString &name, const SCARDCONTEXT ctx);
    void show_select_reader(const QString &protocol);

    // Generic messaging
    void sendIPC(InternalMessage message);

private:
    QMap<QString, MessageType> ongoing; // Keep track of ongoing operations
    void wait(); // forces the thread to sleep and wait for events

    PCSC pcsc;
    LONG error = SCARD_S_SUCCESS;
};
