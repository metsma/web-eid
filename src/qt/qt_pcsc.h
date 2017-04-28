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

#include "pcsc.h"

#include <vector>

#include "dialogs/insert_card.h"
#include "dialogs/reader_in_use.h"
#include "dialogs/select_reader.h"

// Handles PCSC stuff in a dedicated thread.
class QtPCSC: public QObject {
    Q_OBJECT

public:
    // Ongoing dialogs of PCSC subsystem
    QtInsertCard insert_dialog;
    QtReaderInUse inuse_dialog;
    QtSelectReader select_dialog;

    QtPCSC() {
        connect(&this->select_dialog, &QtSelectReader::reader_selected, this, &QtPCSC::reader_selected, Qt::QueuedConnection);
    }


public slots:
    void connect_reader(const QString &protocol);
    void send_apdu(const QByteArray &apdu);
    void disconnect_reader();

    void reader_selected(const LONG status, const QString &reader, const QString &protocol);
    void cancel_reader(); // Signalled from QtReaderInUse dialog

signals:
    void reader_connected(LONG status, const QString &reader, const QString &protocol, const QByteArray &atr);
    void apdu_sent(LONG status, const QByteArray &response);
    void reader_disconnected();
    void show_insert_card(bool show, const QString &name, const SCARDCONTEXT ctx);
    void show_select_reader(const QString &protocol);

private:
    PCSC pcsc;
    LONG error = SCARD_S_SUCCESS;
};

