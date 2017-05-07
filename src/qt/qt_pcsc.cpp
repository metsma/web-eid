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


template < typename Func, typename... Args>
LONG SCCall(const char *fun, const char *file, int line, const char *function, Func func, Args... args)
{
    // TODO: log parameters
    LONG err = func(args...);
    Logger::writeLog(fun, file, line, "%s: %s", function, PCSC::errorName(err));
    return err;
}
#define SCard(API, ...) SCCall(__FUNCTION__, __FILE__, __LINE__, "SCard"#API, SCard##API, __VA_ARGS__)

// return the rv is not CKR_OK
#define check_SCard(API, ...) do { \
    LONG _ret = SCCall(__FUNCTION__, __FILE__, __LINE__, "SCard"#API, SCard##API, __VA_ARGS__); \
    if (_ret != SCARD_S_SUCCESS) { \
       Logger::writeLog(__FUNCTION__, __FILE__, __LINE__, "returning %s", PCSC::errorName(_ret)); \
       return _ret; \
    } \
} while(0)


#define PNP_READER_NAME "\\\\?PnP?\\Notification"



// List taken from pcsc-lite source
const char *PCSC::errorName(LONG err) {
#define CASE(X) case LONG(X): return #X
    switch (err)
    {
        CASE(SCARD_S_SUCCESS);
        CASE(SCARD_E_CANCELLED);
        CASE(SCARD_E_CANT_DISPOSE);
        CASE(SCARD_E_INSUFFICIENT_BUFFER);
        CASE(SCARD_E_INVALID_ATR);
        CASE(SCARD_E_INVALID_HANDLE);
        CASE(SCARD_E_INVALID_PARAMETER);
        CASE(SCARD_E_INVALID_TARGET);
        CASE(SCARD_E_INVALID_VALUE);
        CASE(SCARD_E_NO_MEMORY);
        CASE(SCARD_F_COMM_ERROR);
        CASE(SCARD_F_INTERNAL_ERROR);
        CASE(SCARD_F_UNKNOWN_ERROR);
        CASE(SCARD_F_WAITED_TOO_LONG);
        CASE(SCARD_E_UNKNOWN_READER);
        CASE(SCARD_E_TIMEOUT);
        CASE(SCARD_E_SHARING_VIOLATION);
        CASE(SCARD_E_NO_SMARTCARD);
        CASE(SCARD_E_UNKNOWN_CARD);
        CASE(SCARD_E_PROTO_MISMATCH);
        CASE(SCARD_E_NOT_READY);
        CASE(SCARD_E_SYSTEM_CANCELLED);
        CASE(SCARD_E_NOT_TRANSACTED);
        CASE(SCARD_E_READER_UNAVAILABLE);
        CASE(SCARD_W_UNSUPPORTED_CARD);
        CASE(SCARD_W_UNRESPONSIVE_CARD);
        CASE(SCARD_W_UNPOWERED_CARD);
        CASE(SCARD_W_RESET_CARD);
        CASE(SCARD_W_REMOVED_CARD);
#ifdef SCARD_W_INSERTED_CARD
        CASE(SCARD_W_INSERTED_CARD);
#endif
        CASE(SCARD_E_UNSUPPORTED_FEATURE);
        CASE(SCARD_E_PCI_TOO_SMALL);
        CASE(SCARD_E_READER_UNSUPPORTED);
        CASE(SCARD_E_DUPLICATE_READER);
        CASE(SCARD_E_CARD_UNSUPPORTED);
        CASE(SCARD_E_NO_SERVICE);
        CASE(SCARD_E_SERVICE_STOPPED);
        CASE(SCARD_E_NO_READERS_AVAILABLE);
    default:
        return "UNKNOWN";
    };
}






void QtPCSC::receiveIPC(InternalMessage message) {
    // Receive messages from main thread
    // Message must contain the context id (internal to app)
    if (message.type == MessageType::WaitForReaderEvents) {
        wait();
    } else {
        _log("Unknown message: %d", message.type);
    }
}


void QtPCSC::wait() {
    _log("Waiting for events");
    LONG err;
    do {
        // Wait for card/reader insertion/removal
        err = pcsc.block();
        // TODO: emit events HERE.
    } while (err == SCARD_S_SUCCESS || err == SCARD_E_TIMEOUT);
}

void QtPCSC::reader_selected(const LONG status, const QString &reader, const QString &protocol) {
    if (status != SCARD_S_SUCCESS) {
        return emit reader_connected(status, reader, protocol, {0});
    }
    _log("PCSC: using reader %s", reader.toStdString().c_str());
    LONG err = pcsc.connect(reader.toStdString(), protocol.toStdString());
    // XXX: this should be more logical with a single call to PC/SC
    // If empty at first, wait for insertion, with a dialog
    if (err == LONG(SCARD_E_NO_SMARTCARD) || err == LONG(SCARD_W_REMOVED_CARD)) {
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

