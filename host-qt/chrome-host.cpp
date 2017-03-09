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

#include "qt_host.h"
#include "qt_input.h"

#include "authenticate.h"
#include "sign.h"

#include "util.h"
#include "Common.h" // TODO: rename
#include "Logger.h" // TODO: rename
#include "Labels.h" // TODO: rename

#include <QIcon>
#include <QJsonDocument>
#include <QSslCertificate>
#include <QCommandLineParser>
#include <QUrl>

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <iostream>

#ifdef _WIN32
// for setting stdio mode
#include <fcntl.h>
#include <io.h>
#else
#include <unistd.h>
#endif

// Set a default version, if run from IDE
#ifndef VERSION
#define VERSION "1.0.0.0"
#endif


// The lifecycle of the native components is the lifecycle of a page.
// Every message must have an origin and the origin must not change
// during the lifecycle of the program.

QtHost::QtHost(int &argc, char *argv[]) : QApplication(argc, argv) {
        _log("Starting native host %s args %s", VERSION, arguments().join(" ").toStdString().c_str());
        // Parse the window handle
        QCommandLineParser parser;
        QCommandLineOption pwindow("parent-window");
        pwindow.setValueName("handle");
        parser.addOption(pwindow);
        parser.process(arguments());
        if (parser.isSet(pwindow)) {
            // TODO: actually utilize the window
            _log("Parent window handle: %d", stoi(parser.value(pwindow).toStdString()));
        }

        // Open the output file
        out.open(stdout, QFile::WriteOnly);

        // FIXME UX: Without the icon firefox launches the "exec terminal" icon.
        setWindowIcon(QIcon(":/hwcrypto-native.png"));
        setQuitOnLastWindowClosed(false);

        // This runs a blocking input reading loop and signals the main
        // Qt appliction when a message gas been read.
        InputChecker *np = new InputChecker(this);

        // Trigger processMessage with every successfully read JSON message
        connect(np, &InputChecker::messageReceived, this, &QtHost::processMessage, Qt::QueuedConnection);

        // Start input reading thread with inherited priority
        np->start();
    }

// Called whenever a message is read from browser for processing
void QtHost::processMessage(const QJsonObject &json)
{
    _log("Processing message");

    QVariantMap resp;
    QString msgnonce;

    try {
        if (json.isEmpty()) {
            resp = {{"result", "invalid_argument"}, {"version", VERSION}};
            write(resp, json.value("nonce").toString());
            return exit(EXIT_FAILURE);
        }

        if(!json.contains("type") || !json.contains("nonce") || !json.contains("origin")) {
            resp = {{"result", "invalid_argument"}};
            write(resp, json.value("nonce").toString());
            return exit(EXIT_FAILURE);
        }
        msgnonce = json.value("nonce").toString();

        // Origin. If unset for instance, set
        if (origin.isEmpty()) {
            // Check if origin is secure
            QUrl url(json.value("origin").toString());
            // https, file or localhost
            if (url.scheme() == "https" || url.scheme() == "file" || url.host() == "localhost") {
                origin = json.value("origin").toString();
            } else {
                resp = {{"result", "not_allowed"}};
                write(resp, json.value("nonce").toString());
                return exit(EXIT_FAILURE);
            }
        } else if (origin != json.value("origin").toString()) {
            // Otherwise if already set, it must match
            resp = {{"result", "invalid_argument"}};
            write(resp, json.value("nonce").toString());
            return exit(EXIT_FAILURE);
        }

        if (json.contains("lang")) {
            Labels::l10n.setLanguage(json.value("lang").toString().toStdString());
        }

        // Command dispatch
        QString type = json.value("type").toString();
        if (type == "VERSION") {
            resp = {{"version", VERSION}};
        } else if (type == "SIGN") {
            resp = Sign::sign(this, json);
        } else if (type == "CERT") {
            resp = Sign::select(this, json);
        } else if (type == "AUTH") {
            resp = Authenticate::authenticate(this, json);
        } else {
            resp = {{"result", "invalid_argument"}};
        }
    } catch (const UserCanceledError &) {
        _log("UserCanceledException");
        resp = {{"result", "user_cancel"}};
    } catch (const std::runtime_error &e) {
        _log("Error technical error: %s", e.what());
        resp = {{"result", "technical_error"}};
    } catch (const std::invalid_argument &e) {
        _log("Error invalid argument: %s", e.what());
        resp = {{"result", "invalid_argument"}};
    }
    // TODO: take original message as argument instead of nonce
    write(resp, msgnonce);
}

void QtHost::write(QVariantMap &resp, const QString &nonce) const
{
    if (!nonce.isEmpty())
        resp["nonce"] = nonce;

    if (!resp.contains("result"))
        resp["result"] = "ok";

    QByteArray response =  QJsonDocument::fromVariant(resp).toJson();
    quint32 responseLength = response.size();
    _log("Response(%u) %s", responseLength, response.constData());
    QFile out;
    out.open(stdout, QFile::WriteOnly);
    out.write((const char*)&responseLength, sizeof(responseLength));
    out.write(response);
}

int main(int argc, char *argv[])
{
    // Check that input is a pipe (the app is not run from command line)
    bool isPipe = false;
#ifdef _WIN32
    isPipe = GetFileType(GetStdHandle(STD_INPUT_HANDLE)) == FILE_TYPE_PIPE;
#else
    struct stat sb;
    if (fstat(fileno(stdin), &sb) != 0) {
        exit(1);
    }
    isPipe = S_ISFIFO(sb.st_mode);
#endif
    if (!isPipe) {
        printf("This is not a regular program, it is expected to be run from a browser.\n");
        exit(1);
    }
#ifdef _WIN32
    // Set files to binary mode, to be able to read the uint32 msg size
    _setmode(_fileno(stdin), O_BINARY);
    _setmode(_fileno(stdout), O_BINARY);
#endif
    return QtHost(argc, argv).exec();
}
