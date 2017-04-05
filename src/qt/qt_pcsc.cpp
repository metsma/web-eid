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
void QtPCSC::connect_reader(const QString &reader, const QString &protocol) {
    _log("PCSC: connecting to %s", reader.toStdString().c_str());
    LONG err = pcsc.connect(reader.toStdString(), protocol.toStdString());
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

// Called from main thread. Show the reader selection window
PCSCReader QtPCSC::getReader(const QString &origin)
{
    std::vector<PCSCReader> readers = PCSC::readerList();
    QtSelectReader dialog(origin);
    QTreeWidget *table = dialog.findChild<QTreeWidget*>();

    // Geometry re-calculation

    // Construct the list that is shown to the user.
    for (const auto &r: readers)
    {
        QTreeWidgetItem *item = new QTreeWidgetItem(table, QStringList{
            QString::fromStdString(r.name),
            QString::number(&r - &readers[0])}); // Index of list
        // TODO: nicer UI
        if (r.exclusive) {
            item->setFlags(Qt::NoItemFlags);
            item->setForeground(0, QBrush(Qt::darkRed));
        }
        if (r.atr.empty()) {
            item->setForeground(0, QBrush(Qt::darkGreen));
        }
        table->insertTopLevelItem(0, item);
    }

    // FIXME: auto-select first usable reader
    table->setCurrentIndex(table->model()->index(0, 0));
    // FIXME: somebody please show how the fsck to work with Qt...

    table->resizeColumnToContents(0); // creates the horizontal scroll
    table->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    table->adjustSize();
    table->updateGeometry();
    //table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // FIXME: can still be made smaller than the content of the table
    dialog.setSizePolicy(QSizePolicy::Fixed, QSizePolicy::MinimumExpanding);
    dialog.adjustSize();
    dialog.updateGeometry();

    dialog.resize(table->width() + 48, table->height());

    dialog.show();
    // Make sure window is in foreground and focus
    dialog.raise();
    dialog.activateWindow();
    if (dialog.exec() == 0) {
        // FIXME: return USER CANCEL
    }
    return readers[table->currentItem()->text(1).toUInt()];
}

