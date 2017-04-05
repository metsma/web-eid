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

#include "WinSigner.h"

#include "Common.h"
#include "Logger.h"

#include "pkcs11.h"

#include <Windows.h>
#include <ncrypt.h>
#include <WinCrypt.h>
#include <cryptuiapi.h>

using namespace std;

CK_RV WinSigner::sign(const std::vector<unsigned char> &hash, const std::vector<unsigned char> &cert, std::vector<unsigned char> &result) {

    CK_RV rv = CKR_OK;
    BCRYPT_PKCS1_PADDING_INFO padInfo;
    DWORD obtainKeyStrategy = CRYPT_ACQUIRE_PREFER_NCRYPT_KEY_FLAG;

    ALG_ID alg = 0;

    switch (hash.size())
    {
    case BINARY_SHA224_LENGTH:
        padInfo.pszAlgId = L"SHA224";
        obtainKeyStrategy = CRYPT_ACQUIRE_ONLY_NCRYPT_KEY_FLAG;
        break;
    case BINARY_SHA256_LENGTH:
        padInfo.pszAlgId = NCRYPT_SHA256_ALGORITHM;
        alg = CALG_SHA_256;
        break;
    case BINARY_SHA384_LENGTH:
        padInfo.pszAlgId = NCRYPT_SHA384_ALGORITHM;
        alg = CALG_SHA_384;
        break;
    case BINARY_SHA512_LENGTH:
        padInfo.pszAlgId = NCRYPT_SHA512_ALGORITHM;
        alg = CALG_SHA_512;
        break;
    default:
        return CKR_ARGUMENTS_BAD;
    }

    SECURITY_STATUS err = 0;
    DWORD size = 256; // FIXME use NULL and resize accordingly
    result.resize(size);

    HCERTSTORE store = CertOpenSystemStore(0, L"MY");
    if (!store) {
        _log("Could not open MY store"); // TODO lasterror
        return CKR_GENERAL_ERROR;
    }

    PCCERT_CONTEXT certFromBinary = CertCreateCertificateContext(X509_ASN_ENCODING, cert.data(), cert.size());
    PCCERT_CONTEXT certInStore = CertFindCertificateInStore(store, X509_ASN_ENCODING, 0, CERT_FIND_EXISTING, certFromBinary, 0);
    CertFreeCertificateContext(certFromBinary);

    if (!certInStore) {
        CertCloseStore(store, 0);
        return CKR_KEY_NEEDED; // FIXME: TBS
    }

    DWORD flags = obtainKeyStrategy | CRYPT_ACQUIRE_COMPARE_KEY_FLAG;
    DWORD spec = 0;
    BOOL freeKeyHandle = false;
    HCRYPTPROV_OR_NCRYPT_KEY_HANDLE key = NULL;
    BOOL gotKey = true;
    gotKey = CryptAcquireCertificatePrivateKey(certInStore, flags, 0, &key, &spec, &freeKeyHandle);
    CertFreeCertificateContext(certInStore);
    CertCloseStore(store, 0);

    // Certificate not needed any more, key handle acquired
    switch (spec)
    {
    case CERT_NCRYPT_KEY_SPEC:
    {
        err = NCryptSignHash(key, &padInfo, PBYTE(hash.data()), DWORD(hash.size()), result.data(), DWORD(result.size()), (DWORD*)&size, BCRYPT_PAD_PKCS1);
        if (freeKeyHandle) {
            NCryptFreeObject(key);
        }
        break;
    }
    case AT_SIGNATURE:
    {
        HCRYPTHASH capihash = 0;
        if (!CryptCreateHash(key, alg, 0, 0, &capihash)) {
            if (freeKeyHandle) {
                CryptReleaseContext(key, 0);
            }
            _log("CreateHash failed");
            return CKR_GENERAL_ERROR;
        }

        if (!CryptSetHashParam(capihash, HP_HASHVAL, hash.data(), 0))	{
            if (freeKeyHandle) {
                CryptReleaseContext(key, 0);
            }
            CryptDestroyHash(capihash);
            _log("CryptSetHashParam failed");
            return CKR_GENERAL_ERROR;
        }

        INT retCode = CryptSignHashW(capihash, AT_SIGNATURE, 0, 0, result.data(), &size);
        err = retCode ? ERROR_SUCCESS : GetLastError();
        _log("CryptSignHash() return code: %u (%s) %x", retCode, retCode ? "SUCCESS" : "FAILURE", err);
        if (freeKeyHandle) {
            CryptReleaseContext(key, 0);
        }
        CryptDestroyHash(capihash);
        // TODO: link to docs
        reverse(result.begin(), result.end());
        result.resize(size);
        break;
    }
    default:
        _log("Invalid key type (not CERT_NCRYPT_KEY_SPEC nor AT_SIGNATURE)");
        return CKR_GENERAL_ERROR;
    }

    switch (err)
    {
    case ERROR_SUCCESS:
        return CKR_OK;
    case SCARD_W_CANCELLED_BY_USER:
    case ERROR_CANCELLED:
        return CKR_FUNCTION_CANCELED;
    case SCARD_W_CHV_BLOCKED:
        return CKR_PIN_LOCKED;
    case NTE_INVALID_HANDLE: // TODO: document
        _log("NTE_INVALID_HANDLE");
        return CKR_GENERAL_ERROR;
    default:
        _log("Signing failed: 0x%u08x", err);
        return CKR_GENERAL_ERROR;
    }
}
