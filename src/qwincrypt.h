/*
 * Copyright (C) 2017 Martin Paljak
 */

#pragma once
#include <QtGlobal>
#ifdef Q_OS_WIN
#include <Windows.h>
#include "webeid.h"
#include "pkcs11.h"
#include <QVariant>
#include <QByteArray>
#include "dialogs/winop.h"

typedef const wchar_t *LPCWSTR;

class QWinCrypt {

public:
    struct ErroredResponse {
        CK_RV error;
        QList<QByteArray> result;
    };
    enum HashType {
        SHA256,
        SHA384,
        SHA512
    };

    static ErroredResponse getCertificates();
    static ErroredResponse sign(const QByteArray &cert, const QByteArray &hash, const HashType hashtype, const HWND parent);
    static ErroredResponse selectCertificate(CertificatePurpose type, const QString &title, const QString &message, const HWND parent);
};

#endif