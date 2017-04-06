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

#include "qt_pcsc.h"
#include "qt_pki.h"

#include "util.h"
#include "Logger.h" // TODO: rename

#include <QIcon>
#include <QJsonDocument>
#include <QSslCertificate>
#include <QCommandLineParser>
#include <QTranslator>
#include <QUrl>
#include <QString>
#include <QMenu>

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
QtHost::QtHost(int &argc, char *argv[], bool standalone) : QApplication(argc, argv), tray(this) {

    if (standalone) {
        _log("Starting standalone app v%s", VERSION);
        tray.setIcon(QIcon(":/web-eid.png"));
        tray.show();
        tray.setToolTip("Web eID is running on port XXXX. Click to quit.");
        tray.showMessage("Web eID starts", "Click the icon to quit", QSystemTrayIcon::Warning);
        connect(&tray, &QSystemTrayIcon::activated, [&] {
            // TODO: Show window "do you want to start again"
            exit(1);
        });
        // TODO: add HTTP listener
    } else {
        _log("Starting browser extension host v%s args \"%s\"", VERSION, arguments().join("\" \"").toStdString().c_str());
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

        // InputChecker runs a blocking input reading loop and signals the main
        // Qt appliction when a message gas been read.
        input = new InputChecker(this);

        // Start input reading thread with inherited priority
        input->start();

        // From input thread to host process
        connect(input, &InputChecker::messageReceived, this, &QtHost::incoming, Qt::QueuedConnection);

    }


    setWindowIcon(QIcon(":/web-eid.png"));
    setQuitOnLastWindowClosed(false);

    // Register slots and signals
    // FRAGILE: registered types and explicit queued connections are necessary to
    // make the Qt signal magic work. Otherwise runtime errors of type "could not serialize type CK_RV" will happen

    qRegisterMetaType<CertificatePurpose>();
    qRegisterMetaType<P11Token>();

    // From host process to PCSC and vice versa
    connect(this, &QtHost::connect_reader, &PCSC, &QtPCSC::connect_reader, Qt::QueuedConnection);
    connect(&PCSC, &QtPCSC::reader_connected, this, &QtHost::reader_connected, Qt::QueuedConnection);

    connect(this, &QtHost::send_apdu, &PCSC, &QtPCSC::send_apdu, Qt::QueuedConnection);
    connect(&PCSC, &QtPCSC::apdu_sent, this, &QtHost::apdu_sent, Qt::QueuedConnection);

    connect(this, &QtHost::disconnect_reader, &PCSC, &QtPCSC::disconnect_reader, Qt::QueuedConnection);
    connect(&PCSC, &QtPCSC::reader_disconnected, this, &QtHost::reader_disconnected, Qt::QueuedConnection);

    // PCSC related dialogs
    connect(&PCSC, &QtPCSC::show_insert_card, this, &QtHost::show_insert_card, Qt::QueuedConnection);
    connect(&PCSC, &QtPCSC::show_select_reader, this, &QtHost::show_select_reader, Qt::QueuedConnection);

    // Wire up signals for reader dialogs
    connect(&PCSC.inuse_dialog, &QDialog::rejected, &PCSC, &QtPCSC::cancel_reader, Qt::QueuedConnection);
    connect(&PCSC.insert_dialog, &QtInsertCard::cancel_insert, this, &QtHost::cancel_insert, Qt::QueuedConnection);

    // From host to PKI and vice versa
    connect(this, &QtHost::authenticate, &PKI, &QtPKI::authenticate, Qt::QueuedConnection);
    connect(&PKI, &QtPKI::authentication_done, this, &QtHost::authentication_done, Qt::QueuedConnection);

    connect(this, &QtHost::select_certificate, &PKI, &QtPKI::select_certificate, Qt::QueuedConnection);
    connect(&PKI, &QtPKI::select_certificate_done, this, &QtHost::select_certificate_done, Qt::QueuedConnection);

    connect(this, &QtHost::sign, &PKI, &QtPKI::sign, Qt::QueuedConnection);
    connect(&PKI, &QtPKI::sign_done, this, &QtHost::sign_done, Qt::QueuedConnection);

    // PKI related dialogs
    connect(&PKI, &QtPKI::show_cert_select, this, &QtHost::show_cert_select, Qt::QueuedConnection);
    connect(&PKI.select_dialog, &QtCertSelect::cert_selected, &PKI, &QtPKI::cert_selected, Qt::QueuedConnection);

    // When PIN dialog needs to be shown for PKCS#11
    connect(&PKI, &QtPKI::show_pin_dialog, this, &QtHost::show_pin_dialog, Qt::QueuedConnection);
    connect(&PKI, &QtPKI::hide_pin_dialog, this, &QtHost::hide_pin_dialog, Qt::QueuedConnection);
    connect(&PKI.pin_dialog, &QtPINDialog::login, &PKI, &QtPKI::login, Qt::QueuedConnection);

    // Start PCSC thread
    pki_thread = new QThread;
    pcsc_thread = new QThread;
    pki_thread->start();
    pcsc_thread->start();

    PCSC.moveToThread(pcsc_thread);
    PKI.moveToThread(pki_thread);
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
    pki_thread->exit(0);
    pcsc_thread->wait();
    pki_thread->wait();
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
        resp = {{"error", "process_ongoing"}, {"version", VERSION}};
        write(resp);
        return;
    }


    if (json.isEmpty() || !json.contains("id") || !json.contains("origin")) {
        resp = {{"error", "protocol"}, {"version", VERSION}};
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
            resp = {{"error", "protocol"}};
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
        resp = {{"error", "protocol"}};
        write(resp);
        return shutdown(EXIT_FAILURE);
    }

    // Command dispatch
    if (json.contains("options")) {
        resp = {{"version", VERSION}}; // TODO: add something here
    } else if (json.contains("SCardConnect")) {
        emit connect_reader(json.value("SCardConnect").toObject().value("protocol").toString());
    } else if (json.contains("SCardDisconnect")) {
        emit disconnect_reader();
    } else if (json.contains("SCardTransmit")) {
        emit send_apdu(QByteArray::fromHex(json.value("SCardTransmit").toObject().value("bytes").toString().toLatin1()));
    } else if (json.contains("sign")) {
        emit sign(origin, QByteArray::fromBase64(json.value("sign").toObject().value("cert").toString().toLatin1()), QByteArray::fromBase64(json.value("sign").toObject().value("hash").toString().toLatin1()), json.value("sign").toObject().value("hashalgo").toString());
    } else if (json.contains("cert")) {
        emit select_certificate(origin, Signing, false);
    } else if (json.contains("auth")) {
        emit authenticate(origin, json.value("auth").toObject().value("nonce").toString());
    } else {
        resp = {{"error", "protocol"}};
    }
    if (!resp.empty()) {
        write(resp);
    }
}


// Callback from PKI
void QtHost::authentication_done(const CK_RV status, const QString &token) {
    _log("authentication done");
    if (status == CKR_OK) {
        outgoing({{"token", token}});
    } else {
        outgoing({{"error", QtPKI::errorName(status)}});
    }
}

void QtHost::sign_done(const CK_RV status, const QByteArray &signature) {
    _log("sign done");
    if (status == CKR_OK) {
        outgoing({{"signature", signature.toBase64()}});
    } else {
        outgoing({{"error", QtPKI::errorName(status)}});
    }
}

void QtHost::select_certificate_done(const CK_RV status, const QByteArray &certificate) {
    _log("select done: %s", certificate.toBase64().toStdString().c_str());
    if (status != CKR_OK) {
        outgoing({{"error", QtPKI::errorName(status)}});
    } else {
        outgoing({{"cert", certificate.toBase64()}});
    }
}

// Show certificate selection dialog and emit the chosen dialog
// TODO: emit straight from dialog, removing signal from this object
void QtHost::show_cert_select(const QString origin, std::vector<std::vector<unsigned char>> certs, CertificatePurpose purpose) {
    _log("Showign cert select dialog");
    // Trigger dialog
    PKI.select_dialog.getCert(certs, friendly_origin, purpose); // FIXME: signature (use Q)
}

void QtHost::show_pin_dialog(const CK_RV last, P11Token token, QByteArray cert, CertificatePurpose purpose) {
    _log("Show pin dialog");
    PKI.pin_dialog.showit(last, token, ba2v(cert), origin, purpose);
}

// Called after pinpad login has returned
void QtHost::hide_pin_dialog() {
    PKI.pin_dialog.hide();
}

// Callbacks from PCSC
void QtHost::reader_connected(LONG status, const QString &reader, const QString &protocol, const QByteArray &atr) {
    if (status == SCARD_S_SUCCESS) {
        _log("HOST: reader connected");
        PCSC.inuse_dialog.showit(friendly_origin, reader);
        outgoing({{"reader", reader},
            {"atr", atr.toHex()},
            {"protocol", protocol}
        });
    } else {
        _log("HOST: reader NOT connected: %s", PCSC::errorName(status));
        outgoing({{"error", PCSC::errorName(status)}});
    }
}

void QtHost::apdu_sent(LONG status, const QByteArray &response) {
    _log("HOST: APDU sent");
    if (status == SCARD_S_SUCCESS) {
        outgoing({{"bytes", response.toHex()}});
    } else {
        outgoing({{"error", PCSC::errorName(status)}});
    }
}

void QtHost::show_insert_card(bool show, const QString &name, const SCARDCONTEXT ctx) {
    if (show) {
        PCSC.insert_dialog.showit(friendly_origin, name, ctx);
    } else {
        PCSC.insert_dialog.hide();
    }
}

void QtHost::show_select_reader(const QString &protocol) {
    PCSC.select_dialog.showit(friendly_origin, protocol, PCSC::readerList());
}


// Called from the "insert card" dialog in the main thread to cancel
// SCardGetStatusChange in the PC/SC thread
void QtHost::cancel_insert(const SCARDCONTEXT ctx) {
    _log("HOST: Canceling ongoing PC/SC calls");
    PCSC::cancel(ctx);
}

// Called from the PC/SC thread to close the "Reader in use" dialog
void QtHost::reader_disconnected() {
    _log("HOST: reader disconnected");
    PCSC.inuse_dialog.hide();
    outgoing({}); // FIXME: why this here?
}

void QtHost::outgoing(const QVariantMap &resp) {
    QVariantMap map = resp;
    write(map);
}

void QtHost::write(QVariantMap &resp)
{
    // Without a valid message ID it is a "technical send"
    if (!msgid.isEmpty()) {
        resp["id"] = msgid;
        msgid.clear();
    }

    QByteArray response =  QJsonDocument::fromVariant(resp).toJson();
    quint32 responseLength = response.size();
    _log("Response(%u) %s", responseLength, response.constData());
    out.write((const char*)&responseLength, sizeof(responseLength));
    out.write(response);
    out.flush();
}

int main(int argc, char *argv[])
{
    bool standalone = false;

    // Check if run as a browser extension
    if (argc > 1) {
        std::string arg1(argv[1]);
        if (arg1.find("chrome-extension://") == 0) {
            // Chrome extension
        } else if (QFile::exists(QString::fromStdString(arg1))) {
            // printf("Probably Firefox\n");
        }
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
    } else if (argc == 1) {
        standalone = true;
    }
    return QtHost(argc, argv, standalone).exec();
}
