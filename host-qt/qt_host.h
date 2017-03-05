#pragma once

#include "pkcs11module.h"

#include <QApplication>
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
    std::string origin;

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
    void write(QVariantMap &resp, const QString &nonce = QString()) const;

#ifdef _WIN32
    HWND parent_window;
#endif
};
