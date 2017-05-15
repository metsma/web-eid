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

#include <QVariantMap>

enum MessageType {
    // Dialog related
    ShowDialog, // Show a dialog
    HideDialog, // Hide a dialog from outside
    SelectCertificate, //resolves once the certificate has been selected.
    ShowSelectCertificate, // asks main thread to show a Qt cert selection window
    CertificateSelected, // signalled from cert selection window when certificate is chosen by user

    // PCSC related
    WaitForReaderEvents,
    CardConnect,

    // Context related
    Authenticate, // resolves with token or error
    Sign // resolves with signature or error
};


struct InternalMessage {
    MessageType type;
    QVariantMap data;

    const QString contextId() const {
        return data["id"].toString();
    }

    bool error() const {
        return data.contains("error");
    }
};
