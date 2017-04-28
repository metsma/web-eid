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

#include "WinCertSelect.h"

#include "util.h"
#include "Logger.h"

#include <Windows.h>
#include <WinError.h>
#include <ncrypt.h>
#include <WinCrypt.h>
#include <cryptuiapi.h>

extern "C" {

    typedef BOOL(WINAPI * PFNCCERTDISPLAYPROC)(
        __in  PCCERT_CONTEXT pCertContext,
        __in  HWND hWndSelCertDlg,
        __in  void *pvCallbackData
    );

    typedef struct _CRYPTUI_SELECTCERTIFICATE_STRUCT {
        DWORD               dwSize;
        HWND                hwndParent;
        DWORD               dwFlags;
        LPCWSTR             szTitle;
        DWORD               dwDontUseColumn;
        LPCWSTR             szDisplayString;
        PFNCFILTERPROC      pFilterCallback;
        PFNCCERTDISPLAYPROC pDisplayCallback;
        void *              pvCallbackData;
        DWORD               cDisplayStores;
        HCERTSTORE *        rghDisplayStores;
        DWORD               cStores;
        HCERTSTORE *        rghStores;
        DWORD               cPropSheetPages;
        LPCPROPSHEETPAGEW   rgPropSheetPages;
        HCERTSTORE          hSelectedCertStore;
    } CRYPTUI_SELECTCERTIFICATE_STRUCT, *PCRYPTUI_SELECTCERTIFICATE_STRUCT;

    typedef const CRYPTUI_SELECTCERTIFICATE_STRUCT
    *PCCRYPTUI_SELECTCERTIFICATE_STRUCT;

    PCCERT_CONTEXT WINAPI CryptUIDlgSelectCertificateW(
        __in  PCCRYPTUI_SELECTCERTIFICATE_STRUCT pcsc
    );

#define CryptUIDlgSelectCertificate CryptUIDlgSelectCertificateW

}  // extern "C"

BOOL isCardInReader(PCCERT_CONTEXT certContext) {
    DWORD flags = CRYPT_ACQUIRE_CACHE_FLAG | CRYPT_ACQUIRE_COMPARE_KEY_FLAG | CRYPT_ACQUIRE_SILENT_FLAG;
    NCRYPT_KEY_HANDLE key = 0;
    DWORD spec = 0;
    BOOL ncrypt = FALSE;
    BOOL haskey = CryptAcquireCertificatePrivateKey(certContext, flags, 0, &key, &spec, &ncrypt);
    if (!haskey) {
        _log("Can not acquire key: %d", GetLastError());
    }
    if (!key) {
        return FALSE;
    }
    if (ncrypt) {
        NCryptFreeObject(key);
    }
    return TRUE;
}

bool hasClientSSL(PCCERT_CONTEXT certContext) {
    std::vector<unsigned char> exts(sizeof(CERT_ENHKEY_USAGE));
    DWORD usages = 1; // size reporting does not work if initial size is 0 ...
    BOOL enh = CertGetEnhancedKeyUsage(certContext, CERT_FIND_EXT_ONLY_ENHKEY_USAGE_FLAG, NULL, &usages);
    // FIXME: return code check
    exts.resize(usages);
    enh = CertGetEnhancedKeyUsage(certContext, CERT_FIND_EXT_ONLY_ENHKEY_USAGE_FLAG, (PCERT_ENHKEY_USAGE) &exts[0], &usages);
    // FIXME: return code check
    PCERT_ENHKEY_USAGE ug = (PCERT_ENHKEY_USAGE) &exts[0];

    for (DWORD i = 0; i < ug->cUsageIdentifier; i++ ) {
        // 1.3.6.1.3.5.5.7.3.2
        if (strcmp(ug->rgpszUsageIdentifier[i], szOID_PKIX_KP_CLIENT_AUTH) == 0) {
            // this is authentication cert
            _log("This is Client SSL cert!");
            return true;
        }
    }
    return false;
}

bool hasNonRepudiation(PCCERT_CONTEXT certContext) {
    // Check for nonrepudiation bit
    BYTE keyUsage;
    CertGetIntendedKeyUsage(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, certContext->pCertInfo, &keyUsage, 1);
    if (keyUsage & CERT_NON_REPUDIATION_KEY_USAGE) {
        _log("This is NonRepudiation cert!");
        return true;
    }
    return false;
}

bool isTimeValid(PCCERT_CONTEXT certContext) {
    if (CertVerifyTimeValidity(NULL, certContext->pCertInfo) != 0) {
        _log("Not valid (time)");
        return false;
    }
    return true;
}


bool isValidAuthenticationCert(PCCERT_CONTEXT certContext) {
    return hasClientSSL(certContext) && isTimeValid(certContext) && isCardInReader(certContext);
}

BOOL WINAPI filter_auth(PCCERT_CONTEXT certContext, BOOL *pfInitialSelectedCert, void *pvCallbackData) {
    // silence C4100 warnings
    (void) pfInitialSelectedCert;
    (void) pvCallbackData;
    _log("Checking certificate for authentication");
    return isValidAuthenticationCert(certContext);
}

bool isValidSigningCert(PCCERT_CONTEXT certContext) {
    return hasNonRepudiation(certContext) && isTimeValid(certContext) && isCardInReader(certContext);
}
BOOL WINAPI filter_sign(PCCERT_CONTEXT certContext, BOOL *pfInitialSelectedCert, void *pvCallbackData) {
    // silence C4100 warnings
    (void) pfInitialSelectedCert;
    (void) pvCallbackData;
    _log("Checking certificate for signing");
    return isValidSigningCert(certContext);
}


CK_RV WinCertSelect::getCert(CertificatePurpose p, LPCWSTR message, std::vector<unsigned char> &result) {

    HCERTSTORE store = CertOpenSystemStore(0, L"MY");
    if (!store)	{
        _log("Cold not open store: %d", GetLastError());
        return CKR_GENERAL_ERROR;
    }

    // Enumerate certificates, so that no_certificates satus could be reported.
    PCCERT_CONTEXT pCertContextForEnumeration = nullptr;
    int certificatesCount = 0;
    while (pCertContextForEnumeration = CertEnumCertificatesInStore(store, pCertContextForEnumeration)) {
        _log("Checking certificate during enumeration");
        if (p == Signing ? isValidSigningCert(pCertContextForEnumeration) : isValidAuthenticationCert(pCertContextForEnumeration)) {
            certificatesCount++;
        }
    }
    if (pCertContextForEnumeration) {
        CertFreeCertificateContext(pCertContextForEnumeration);
    }
    _log("Found a total of certs in MY store: %d for %d", certificatesCount, p);

    if (certificatesCount == 0) {
        CertCloseStore(store, 0);
        return CKR_KEY_NEEDED;
    }
    // Show selection dialog
    CRYPTUI_SELECTCERTIFICATE_STRUCT pcsc = { sizeof(pcsc) };
    if (p == Authentication) {
        pcsc.pFilterCallback = filter_auth;
    } else if (p == Signing) {
        pcsc.pFilterCallback = filter_sign;
    }
    pcsc.szDisplayString = message;
    pcsc.pvCallbackData = nullptr; // TODO: use a single callback with arguments instead ?
    pcsc.cDisplayStores = 1;
    pcsc.rghDisplayStores = &store;
    PCCERT_CONTEXT cert_context = CryptUIDlgSelectCertificate(&pcsc);

    if (!cert_context)
    {
        CertCloseStore(store, 0);
        _log("User pressed cancel");
        return CKR_FUNCTION_CANCELED;
    }
    std::vector<unsigned char> cert(cert_context->pbCertEncoded, cert_context->pbCertEncoded + cert_context->cbCertEncoded);
    result = cert;
    _log("Selected certificate with subject %s", x509subject(result).c_str()); // XXX: requires OpenSSL dll-s with Qt
    CertFreeCertificateContext(cert_context);
    CertCloseStore(store, 0);
    return CKR_OK;
}
