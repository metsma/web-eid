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

#include <QApplication>
#include <QTranslator>
#include <QFile>
#include <QVariantMap>
#include <QJsonObject>

#ifdef _WIN32
#include <qt_windows.h>
#endif

class QtHost: public QApplication
{
    Q_OBJECT

public:
    QtHost(int &argc, char *argv[]);

    // It is assumed that all invocations from one origin
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

public slots:
    // Called when a message has been received from the
    // browser, using the Qt signaling mechanism
    void processMessage(const QJsonObject &json);

private:
    QFile out;
    void write(QVariantMap &resp, const QString &nonce = QString());
    void shutdown(int exitcode);
    InputChecker input;

    QTranslator translator;
#ifdef _WIN32
    HWND parent_window;
#endif
};
