/*
 * Chrome Token Signing Native Host
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

#include "Common.h"
#include "pcsc.h"

#include <vector>

#include "qt_insert_dialog.h"
#include "qt_inuse_dialog.h"
#include "qt_select_reader_dialog.h"

// Handles PCSC stuff in a dedicated thread.
class QtPCSC: public QObject {
    Q_OBJECT

public:
    // Called from main thread
    static PCSCReader getReader(const QString &origin);

    // Ongoing dialogs of PCSC subsystem
    QtInsertCard insert_dialog;
    QtReaderInUse inuse_dialog;

public slots:
    void connect_reader(std::string reader, std::string protocol);
    void send_apdu(std::vector<unsigned char> apdu);
    void disconnect_reader();

    void cancel_reader(); // Signalled from QtReaderInUse

signals:
    void reader_connected(LONG status, std::string reader, std::string protocol, std::vector<unsigned char> atr);
    void apdu_sent(LONG status, std::vector<unsigned char> response);
    void reader_disconnected();
    void show_insert_card(bool show, std::string name, SCARDCONTEXT ctx);

private:
    PCSC pcsc;
    LONG error = SCARD_S_SUCCESS;
};

