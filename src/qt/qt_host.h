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

#include "pkcs11module.h"
#include "qt_input.h"
#include "qt_pcsc.h"

#include <QApplication>
#include <QTranslator>
#include <QFile>
#include <QVariantMap>
#include <QJsonObject>

#ifdef _WIN32
#include <qt_windows.h>
#endif

Q_DECLARE_METATYPE(std::string)
Q_DECLARE_METATYPE(std::vector<unsigned char>)

class QtHost: public QApplication
{
    Q_OBJECT

public:
    QtHost(int &argc, char *argv[]);

    // TODO: It is currently assumed that all invocations from one origin
    // go to the same PKCS#11 module
    PKCS11Module pkcs11;

    // Thus the origin can not change, once set
    QString origin;

    // Friently origin is something that can be shown to the user
    QString friendly_origin;

    // And the chosen signing certificate can not change either
    // Only with a new cert message
    std::vector<unsigned char> signcert;

    // We keep a flag around that show if the selected cert is from CAPI
    bool winsign = false;

    // PCSC subsystem, in a separate thread
    QtPCSC PCSC;
    QThread *pcsc_thread;

public slots:
    // Called when a message has been received from the
    // browser, using the Qt signaling mechanism
    void incoming(const QJsonObject &json);

    // Called when a message is to be sent back to the browser
    void outgoing(const QVariantMap &resp);

    void reader_connected(LONG status, std::string reader, std::string protocol, std::vector<unsigned char> atr);
    void apdu_sent(LONG status, std::vector<unsigned char> response);
    void reader_disconnected();

    void show_insert_card(bool show, std::string name, SCARDCONTEXT ctx);
    void cancel_insert(SCARDCONTEXT ctx);

signals:
    void connect_reader(std::string reader, std::string protocol);
    void send_apdu(std::vector<unsigned char> apdu);
    void disconnect_reader();

private:
    QString msgid; // If a message is being processed, set to ID

    SCARDCONTEXT cancel_ctx = 0; // If we need to cancel the "insert card" process, we need this context

    QFile out;
    void write(QVariantMap &resp);
    void shutdown(int exitcode);
    InputChecker *input;

    QTranslator translator;
#ifdef _WIN32
    HWND parent_window;
#endif
};
