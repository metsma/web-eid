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

#include "qt_pcsc.h"

#include "Logger.h"
#include "util.h"
#include "pcsc.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QJsonObject>
#include <QJsonDocument>


void QtPCSC::reader_selected(const LONG status, const QString &reader, const QString &protocol) {
    if (status != SCARD_S_SUCCESS) {
        return emit reader_connected(status, reader, protocol, {0});
    }
    _log("PCSC: using reader %s", reader.toStdString().c_str());
    LONG err = pcsc.connect(reader.toStdString(), protocol.toStdString());
    // XXX: this should be more logical with a single call to PC/SC
    // If empty at first, wait for insertion, with a dialog
    if (err == LONG(SCARD_E_NO_SMARTCARD)) {
        emit show_insert_card(true, reader, pcsc.getContext());
        err = pcsc.wait(reader.toStdString(), protocol.toStdString());
        emit show_insert_card(false, reader, pcsc.getContext());
    }
    if (err == SCARD_S_SUCCESS) {
        emit reader_connected(err, reader, pcsc.protocol == SCARD_PROTOCOL_T0 ? "T=0" : "T=1", v2ba(pcsc.getStatus().atr));
    } else {
        emit reader_connected(err, reader, protocol, {0});
    }
}

// Process DISCONNECT command
void QtPCSC::disconnect_reader() {
    _log("PCSC: disconnecting reader");
    pcsc.disconnect();
    emit reader_disconnected();
}

// Reader access cancelled from the "reader in use" dialog
void QtPCSC::cancel_reader() {
    _log("PCSC: cancel reader access");
    // FIXME: maybe not a good idea, only give a notification with the possibility of removing card?
    error = SCARD_E_CANCELLED;
    pcsc.disconnect();
    // Note: nothing is emitted here, ongoing APDU is transmitted
    // and above error returned on next call
}

// Process CONNECT command
void QtPCSC::connect_reader(const QString &protocol) {
    _log("PCSC: connecting to reader");
    return emit show_select_reader(protocol);
}

// Process APDU command
void QtPCSC::send_apdu(const QByteArray &apdu) {
    std::vector<unsigned char> response;
    // When the dialog is cancelled, set a local error and use it here for next invocation
    if (error != SCARD_S_SUCCESS) {
        emit apdu_sent(error, 0);
        error = SCARD_S_SUCCESS; // set back to normal
        return;
    }
    response.resize(4096); // More than most APDU buffers on cards
    _log("PCSC: sending APDU: %s", apdu.toHex().toStdString().c_str());
    LONG err = pcsc.transmit(ba2v(apdu), response);
    emit apdu_sent(err, v2ba(response));
}

