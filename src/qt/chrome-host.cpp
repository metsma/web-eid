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

#include "authenticate.h"
#include "sign.h"
#include "qt_pcsc.h"
#include "pcsc.h"

#include "util.h"
#include "Common.h" // TODO: rename
#include "Logger.h" // TODO: rename

#include <QIcon>
#include <QJsonDocument>
#include <QSslCertificate>
#include <QCommandLineParser>
#include <QTranslator>
#include <QUrl>
#include <QString>

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
            // XXX: we can not actually utilize the window handle, as it is always 0
            // See issue #12
            _log("Parent window handle: %d", stoi(parser.value(pwindow).toStdString()));
        }

        // Open the output file
        out.open(stdout, QFile::WriteOnly);

        // FIXME UX: Without the icon firefox launches the "exec terminal" icon.
        setWindowIcon(QIcon(":/hwcrypto-native.png"));
        setQuitOnLastWindowClosed(false);


        // InputChecker runs a blocking input reading loop and signals the main
        // Qt appliction when a message gas been read.
        input = new InputChecker(this);

        // From input thread to host process
        connect(input, &InputChecker::messageReceived, this, &QtHost::incoming, Qt::QueuedConnection);

        // From host process to PCSC and vice versa
        connect(this, &QtHost::connect_reader, &PCSC, &QtPCSC::connect_reader, Qt::QueuedConnection);
        connect(&PCSC, &QtPCSC::reader_connected, this, &QtHost::reader_connected, Qt::QueuedConnection);

        connect(this, &QtHost::send_apdu, &PCSC, &QtPCSC::send_apdu, Qt::QueuedConnection);
        connect(&PCSC, &QtPCSC::apdu_sent, this, &QtHost::apdu_sent, Qt::QueuedConnection);

        connect(this, &QtHost::disconnect_reader, &PCSC, &QtPCSC::disconnect_reader, Qt::QueuedConnection);
        connect(&PCSC, &QtPCSC::reader_disconnected, this, &QtHost::reader_disconnected, Qt::QueuedConnection);
        connect(&PCSC, &QtPCSC::show_insert_card, this, &QtHost::show_insert_card, Qt::QueuedConnection);

        // Wire up signals for reader dialogs
        connect(&PCSC.inuse_dialog, &QDialog::rejected, &PCSC, &QtPCSC::cancel_reader, Qt::QueuedConnection);
        connect(&PCSC.insert_dialog, &QtInsertCard::cancel_insert, this, &QtHost::cancel_insert, Qt::QueuedConnection);

        // Start input reading thread with inherited priority
        input->start();

        // Start PCSC thread
        pcsc_thread = new QThread;
        pcsc_thread->start();
        PCSC.moveToThread(pcsc_thread);
    }

void QtHost::shutdown(int exitcode) {
    _log("Exiting with %d", exitcode);
    // This should make the input thread close nicely.
#ifdef _WIN32
    //close(_fileno(stdin));
    input->terminate();
#else
    close(0);
#endif
    _log("input closed");
    pcsc_thread->exit(0);
    pcsc_thread->wait();
    exit(exitcode);
}

// Called whenever a message is read from browser for processing
void QtHost::incoming(const QJsonObject &json)
{
    _log("Processing message");
    QVariantMap resp;


    // Serial access
    if (!msgid.isEmpty()) {
        _log("Already processing message %s", msgid.toStdString().c_str());
        resp = {{"result", "process_ongoing"}, {"version", VERSION}};
        write(resp);
        return;
    }


    try {
        if (json.isEmpty()) {
            resp = {{"result", "invalid_argument"}, {"version", VERSION}};
            write(resp);
            return shutdown(EXIT_FAILURE);
        }

        if(!json.contains("type") || !json.contains("id") || !json.contains("origin")) {
            resp = {{"result", "invalid_argument"}};
            write(resp);
            return shutdown(EXIT_FAILURE);
        }

        msgid = json.value("id").toString();

        // Origin. If unset for instance, set
        if (origin.isEmpty()) {
            // Check if origin is secure
            QUrl url(json.value("origin").toString());
            // https, file or localhost
            if (url.scheme() == "https" || url.scheme() == "file" || url.host() == "localhost") {
                origin = json.value("origin").toString();
                // set the "human readable origin"
                // use localhost for file url-s
                if (url.scheme() == "file") {
                    friendly_origin = "localhost";
                } else {
                    friendly_origin = url.host();
                }
            } else {
                resp = {{"result", "not_allowed"}};
                write(resp);
                return shutdown(EXIT_FAILURE);
            }
            // Setting the language is also a onetime operation, thus do it here.
            QLocale locale = json.contains("lang") ? QLocale(json.value("lang").toString()) : QLocale::system();
            _log("Setting language to %s", locale.name().toStdString().c_str());
            // look up translation rom resource :/translations/strings_XX.qm
            if (translator.load(QLocale(json.value("lang").toString()), QLatin1String("strings"), QLatin1String("_"), QLatin1String(":/translations"))) {
                if (installTranslator(&translator)) {
                    _log("Language set");
                } else {
                    _log("Language NOT set");
                }
            } else {
                _log("Failed to load translation");
            }
        } else if (origin != json.value("origin").toString()) {
            // Otherwise if already set, it must match
            resp = {{"result", "invalid_argument"}};
            write(resp);
            return shutdown(EXIT_FAILURE);
        }

        // Command dispatch
        QString type = json.value("type").toString();
        if (type == "CONNECT") {
            // Choose reader (in main thread)
            PCSCReader reader = QtPCSC::getReader(friendly_origin);
            // Open reader
            emit connect_reader(reader.name, "*"); // TODO: incoming protocol ?
            return;
        } else if (type == "DISCONNECT") {
            emit disconnect_reader();
            return;
        } else if (type == "APDU") {
            emit send_apdu(hex2v(json.value("bytes").toString().toStdString()));
            return;
        } else if (type == "VERSION") {
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
    if (!resp.empty()) {
        write(resp);
    }
}

// Callbacks from PCSC
void QtHost::reader_connected(LONG status, std::string reader, std::string protocol, std::vector<unsigned char> atr) {
    if (status == SCARD_S_SUCCESS) {
        _log("HOST: reader connected");
        PCSC.inuse_dialog.showit(friendly_origin, QString::fromStdString(reader));
        outgoing({{"result", "ok"},
                  {"reader", QString::fromStdString(reader)},
                  {"atr", QString::fromStdString(toHex(atr))},
                  {"protocol", QString::fromStdString(protocol)}});
    } else {
        _log("HOST: reader NOT connected: %s", PCSC::errorName(status));
         outgoing({{"result", PCSC::errorName(status)}}); // TODO
    }
}

void QtHost::apdu_sent(LONG status, std::vector<unsigned char> response) {
    _log("HOST: APDU sent");
    if (status == SCARD_S_SUCCESS) {
        outgoing({{"result", "ok"}, {"bytes", QString::fromStdString(toHex(response))}});
    } else {
        outgoing({{"result", PCSC::errorName(status)}});
    }
}

void QtHost::show_insert_card(bool show, std::string name, SCARDCONTEXT ctx) {
    if (show) {
        PCSC.insert_dialog.showit(friendly_origin, QString::fromStdString(name), ctx);
    } else {
        PCSC.insert_dialog.hide();
    }
}

// Called from the "insert card" dialog
void QtHost::cancel_insert(SCARDCONTEXT ctx) {
    _log("Cancelling insert");
    PCSC::cancel(ctx);
}
void QtHost::reader_disconnected() {
    _log("HOST: reader disconnected");
    PCSC.inuse_dialog.hide();
    outgoing({{"result", "ok"}});
}

void QtHost::outgoing(const QVariantMap &resp) {

    QVariantMap map = resp;
    write(map);
}

void QtHost::write(QVariantMap &resp)
{
    if (!msgid.isEmpty()) {
        resp["id"] = msgid;
        msgid.clear();
    }

    if (!resp.contains("result"))
        resp["result"] = "ok";

    QByteArray response =  QJsonDocument::fromVariant(resp).toJson();
    quint32 responseLength = response.size();
    _log("Response(%u) %s", responseLength, response.constData());
    out.write((const char*)&responseLength, sizeof(responseLength));
    out.write(response);
    out.flush();
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
