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

#include "qwincrypt.h"

#ifdef Q_OS_WIN

#include "Logger.h"

#include <Windows.h>
#include <WinError.h>
#include <ncrypt.h>
#include <WinCrypt.h>
#include <cryptuiapi.h>



static BOOL isCardInReader(PCCERT_CONTEXT certContext) {
    DWORD flags = CRYPT_ACQUIRE_CACHE_FLAG | CRYPT_ACQUIRE_COMPARE_KEY_FLAG | CRYPT_ACQUIRE_SILENT_FLAG;
    NCRYPT_KEY_HANDLE key = 0;
    DWORD spec = 0;
    BOOL ncrypt = FALSE;
    BOOL haskey = CryptAcquireCertificatePrivateKey(certContext, flags, 0, &key, &spec, &ncrypt);
    if (!haskey) {
        _log("Can not acquire key: %d", GetLastError());
        return FALSE;
    }
    if (!key) {
        return FALSE;
    }
    if (ncrypt) {
        NCryptFreeObject(key);
    }
    _log("Key available");
    return TRUE;
}

QVector<QByteArray> QWinCrypt::getCertificates() {
    QVector<QByteArray> result;
    HCERTSTORE store = CertOpenSystemStore(0, L"MY");
    if (!store) {
        _log("Cold not open store: %d", GetLastError());
        return result;
    }

    // Enumerate certificates, so that no_certificates satus could be reported.
    PCCERT_CONTEXT pCertContextForEnumeration = nullptr;
    while (pCertContextForEnumeration = CertEnumCertificatesInStore(store, pCertContextForEnumeration)) {
        if (isCardInReader(pCertContextForEnumeration)) {
            result.append(QByteArray((const char *)pCertContextForEnumeration->pbCertEncoded, int(pCertContextForEnumeration->cbCertEncoded)));
        }
    }
    if (pCertContextForEnumeration) {
        CertFreeCertificateContext(pCertContextForEnumeration);
    }
    _log("Found a total of certs in MY store: %d", result.size());
    CertCloseStore(store, 0);
    return result;
}
#endif