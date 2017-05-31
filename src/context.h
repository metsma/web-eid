/*
 * Copyright (C) 2017 Martin Paljak
 */

#pragma once

#include <QWebSocket>
#include <QLocalSocket>
#include <QDialog>
#include <QUuid>
#include <QTimer>
#include <QFutureWatcher>

class QtPCSC;
class QPCSCReader;
class QPKI;

// Handles a browser context, either
// via WebSocket or LocalSocket, which it owns.
// Lives in main thread, is created by main.cpp
class WebContext: public QObject {
    Q_OBJECT

public:
    WebContext(QObject *parent, QWebSocket *client);
    WebContext(QObject *parent, QLocalSocket *client);

    const QString id = QUuid::createUuid().toString();

    static bool isSecureOrigin(const QString &origin);
    QString origin; // TODO: access
    QString friendlyOrigin() const;
    void terminate();

    QTimer timer;

    // Any running UI widget, associated with the context
    QDialog *dialog = nullptr;
    void outgoing(QVariantMap message); // So that main.cpp could send version on connect

signals:
    void disconnected();

private:
    void processMessage(const QVariantMap &message); // Message received from client

    // message transport
    QWebSocket *ws = nullptr;
    QLocalSocket *ls = nullptr;

    // browser context
    QString msgid;
    QPKI *PKI;
    QtPCSC *PCSC;

    // Used readers
    QMap<QString, QPCSCReader *> readers;
};
