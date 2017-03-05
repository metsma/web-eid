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


#include "pkcs11module.h"
#include "pkcs11.h"
#include "Logger.h"
#include "util.h"
#include "Common.h"


#include <algorithm>
#include <cstring>
#include <string>
#include <stdexcept>
#include <iostream>
#include <map>
#include <vector>

#include <QSslCertificate>
#include <QSslCertificateExtension>
#include <QByteArray>
#include <QJsonDocument>
#include <QMetaType>
#include <QList>

#ifndef _WIN32
#include <dlfcn.h>
#endif


#define C(API, ...) Call(__FILE__, __LINE__, "C_"#API, fl->C_##API, __VA_ARGS__)

// Wrapper around a single PKCS#11 module

template <typename Func, typename... Args>
void Call(const char *file, int line, const char *function, Func func, Args... args)
{
        CK_RV rv = func(args...);
        Logger::writeLog(function, file, line, "return value 0x%08x", rv);
        switch (rv) {
        // FIXME: make it return the rv and do not throw anything here.
        case CKR_CRYPTOKI_ALREADY_INITIALIZED:
        case CKR_OK:
            break;
        case CKR_FUNCTION_CANCELED:
            throw UserCanceledError();
        case CKR_PIN_INCORRECT:
            throw AuthenticationError();
        case CKR_PIN_LEN_RANGE:
            throw AuthenticationBadInput();
        default:
            throw std::runtime_error("PKCS11 method failed.");
        }
}

std::vector<unsigned char> PKCS11Module::attribute(CK_ATTRIBUTE_TYPE type, CK_SESSION_HANDLE sid, CK_OBJECT_HANDLE obj) const
{
	CK_ATTRIBUTE attr = {type, nullptr, 0};
    C(GetAttributeValue, sid, obj, &attr, 1UL);
	std::vector<unsigned char> data(attr.ulValueLen, 0);
	attr.pValue = data.data();
    C(GetAttributeValue, sid, obj, &attr, 1UL);
	return data;
}

std::vector<CK_OBJECT_HANDLE> PKCS11Module::objects(CK_OBJECT_CLASS objectClass, CK_SESSION_HANDLE session, CK_ULONG count) const
{
	return objects({ {CKA_CLASS, &objectClass, sizeof(objectClass)} }, session, count);
}

std::vector<CK_OBJECT_HANDLE> PKCS11Module::objects(const std::vector<CK_ATTRIBUTE> &attr, CK_SESSION_HANDLE session, CK_ULONG count) const
{
	C(FindObjectsInit, session, const_cast<CK_ATTRIBUTE*>(attr.data()), CK_ULONG(attr.size()));
	CK_ULONG objectCount = count;
	std::vector<CK_OBJECT_HANDLE> objects(objectCount);
	C(FindObjects, session, objects.data(), objects.size(), &objectCount);
	C(FindObjectsFinal, session);
	objects.resize(objectCount);
	return objects;
}

std::vector<CK_OBJECT_HANDLE> PKCS11Module::getKey(CK_SESSION_HANDLE session, const std::vector<unsigned char> &id) const {
    _log("Looking for key with id %s length %d", toHex(id).c_str(), id.size());
    CK_OBJECT_CLASS keyclass = CKO_PRIVATE_KEY;
    return objects({
        {CKA_CLASS, &keyclass, sizeof(keyclass)},
        {CKA_ID, (void*)id.data(), id.size()}
    }, session, 1);
}


static bool usage_matches(const std::vector<unsigned char> &certificateCandidate, bool auth) {
        QSslCertificate cert = v2cert(certificateCandidate);
        bool isCa = true;
        bool isSSLClient = false;
        bool isNonRepudiation = false;

        QList<QSslCertificateExtension> exts = cert.extensions();

		for (const QSslCertificateExtension &ext: cert.extensions()) {
			QVariant v = ext.value();
//            _log("ext: %s", ext.name().toStdString().c_str());
			if (ext.name() == "basicConstraints") {
				QVariantMap m = ext.value().toMap();
                isCa = m.value("ca").toBool();
			} else if (ext.oid() == "2.5.29.37") {
                // 2.5.29.37 - extendedKeyUsage
                // XXX: these are not declared stable by Qt.
                // Linux returns parsed map (OpenSSL?) OSX 5.8 returns QByteArrays, 5.5 works
                if (v.canConvert<QByteArray>()) {
                    // XXX: this is 06082b06010505070302 what is 1.3.6.1.5.5.7.3.2 what is "TLS Client"
                    if (v.toByteArray().toHex().contains("06082b06010505070302")) {
                        isSSLClient = true;
                    }
                } else if (v.canConvert<QList<QVariant>>()) {
                    // Linux
                    if (v.toList().contains("TLS Web Client Authentication")) {
                        isSSLClient = true;
                    }
                }
			} else if (ext.name() == "keyUsage") {
                if (v.canConvert<QList<QVariant>>()) {
                    // Linux
                    if (v.toList().contains("Non Repudiation")) {
                        isNonRepudiation = true;
                    }
                }
                // FIXME: detect NR from byte array
                // Do a ugly trick for esteid only
                QList<QString> ou = cert.subjectInfo(QSslCertificate::OrganizationalUnitName);
                if (!isNonRepudiation && ou.size() > 0 && ou.at(0) == "digital signature") {
                    isNonRepudiation = true;
                }
                _log("keyusage: %s", v.toByteArray().toHex().toStdString().c_str());
            }
        }
        _log("Certificate flags: ca=%d auth=%d nonrepu=%d", isCa , isSSLClient, isNonRepudiation);
        if (auth) {
            return isSSLClient && !isCa;
        } else {
            return isNonRepudiation && !isCa;
        }
}


void PKCS11Module::load(const std::string &module) {
        // Clear any present modules
        certs.clear();
        CK_C_GetFunctionList C_GetFunctionList = nullptr;
#ifdef _WIN32
        library = LoadLibraryA(module.c_str());
        if (library)
            C_GetFunctionList = CK_C_GetFunctionList(GetProcAddress(library, "C_GetFunctionList"));
#else
        library = dlopen(module.c_str(), RTLD_LOCAL | RTLD_NOW);
        if (library)
            C_GetFunctionList = CK_C_GetFunctionList(dlsym(library, "C_GetFunctionList"));
#endif

        if (!C_GetFunctionList) {
            throw std::runtime_error("Module does not have C_GetFunctionList");
        }
        Call(__FILE__, __LINE__, "C_GetFunctionList", C_GetFunctionList, &fl);
        C(Initialize, nullptr);

        // Locate all slots with tokens
        std::vector<CK_SLOT_ID> slots_with_tokens;
        CK_ULONG slotCount = 0;
        C(GetSlotList, CK_TRUE, nullptr, &slotCount);
        _log("slotCount = %i", slotCount);
        slots_with_tokens.resize(slotCount);
        C(GetSlotList, CK_TRUE, slots_with_tokens.data(), &slotCount);
        for (auto slot: slots_with_tokens) {
            // Check the content of the slot
            CK_TOKEN_INFO token;
            CK_SESSION_HANDLE sid = 0;
            _log("Checking slot %u", slot);
            C(GetTokenInfo, slot, &token);
            std::string label = QString::fromUtf8((const char* )token.label, sizeof(token.label)).simplified().toStdString();
            _log("Token has a label: \"%s\"", label.c_str());
            C(OpenSession, slot, CKF_SERIAL_SESSION, nullptr, nullptr, &sid);
            _log("Opened session: %u", sid);
            // CK_OBJECT_CLASS objectClass
            std::vector<CK_OBJECT_HANDLE> objectHandle = objects(CKO_CERTIFICATE, sid, 2);
            // We now have the certificate handles (valid for this session) in objectHandle
            _log("Found %u certificates from slot %u", objectHandle.size(), slot);
            for (CK_OBJECT_HANDLE handle: objectHandle) {
                // Get DER
                std::vector<unsigned char> certCandidate = attribute(CKA_VALUE, sid, handle);
                // Get certificate ID
                std::vector<unsigned char> certid = attribute(CKA_ID, sid, handle);
                _log("Found certificate: %s %s", x509subject(certCandidate).c_str(), toHex(certid).c_str());
                // add to map
                certs[certCandidate] = std::make_pair(P11Token({(int)token.ulMinPinLen, (int)token.ulMaxPinLen, label, (bool)(token.flags & CKF_PROTECTED_AUTHENTICATION_PATH), slot, token.flags}), certid);
            }
            // Close session with this slot
            C(CloseSession, sid);
        }
        // List all found certs
        _log("listing found certs");
        for(const auto &cpairs : certs) {
            auto location = cpairs.second;
            _log("certificate: %s in slot %d with id %s", x509subject(cpairs.first).c_str(), location.first.slot, toHex(location.second).c_str());
        }
}

std::vector<std::vector <unsigned char>> PKCS11Module::getSignCerts() {
        std::vector<std::vector<unsigned char>> res;
          for(auto const &crts: certs) {
             if (usage_matches(crts.first, false)) {
                 _log("certificate: %s", x509subject(crts.first).c_str());
                 res.push_back(crts.first);
             }
        }
        return res;
}

std::vector<std::vector <unsigned char>> PKCS11Module::getAuthCerts() {
        std::vector<std::vector<unsigned char>> res;
          for(auto const &crts: certs) {
             if (usage_matches(crts.first, true)) {
                 _log("certificate: %s", x509subject(crts.first).c_str());
                 res.push_back(crts.first);
             }
        }
        return res;
}


// return the certificates found from this module.
std::vector<std::vector <unsigned char>> PKCS11Module::getCerts() {
        std::vector<std::vector<unsigned char>> res;
          for(auto const &crts: certs) {
             _log("certificate: %s", toHex(crts.first).c_str());
             res.push_back(crts.first);
        }
        return res;
}


PKCS11Module::~PKCS11Module() {
    if (session)
        C(CloseSession, session);

    if (!library)
        return;

    C(Finalize, nullptr);
#ifdef _WIN32
    FreeLibrary(library);
#else
    dlclose(library);
#endif
}

// Perform actual technical signing.
std::vector<unsigned char> PKCS11Module::sign(const std::vector<unsigned char> &cert, const std::vector<unsigned char> &hash, const char *pin) const {

        // Locate the slot (std::pair)
        auto slot = certs.find(cert)->second;

        CK_TOKEN_INFO token;
        CK_SESSION_HANDLE sid = 0;

        _log("Using key from slot %d with ID %s", slot.first.slot, toHex(slot.second).c_str());
        C(GetTokenInfo, slot.first.slot, &token);
        _log("Token has a label: \"%s\"", slot.first.label.c_str());
        C(OpenSession, slot.first.slot, CKF_SERIAL_SESSION, nullptr, nullptr, &sid);

        // Login to the session
        C(Login, sid, CKU_USER, (unsigned char*)pin, pin ? strlen(pin) : 0);
        // FIXME: error handling

        // Locate private key
        std::vector<CK_OBJECT_HANDLE> key = getKey(sid, slot.second);
        if (key.size() != 1) {
            _log("Can not sign - no key or found multiple matches");
            throw std::runtime_error("Did not find the key");
        }

        CK_MECHANISM mechanism = {CKM_RSA_PKCS, 0, 0};
        C(SignInit, sid, &mechanism, key[0]);
        std::vector<unsigned char> hashWithPadding;
        switch (hash.size()) {
        case BINARY_SHA1_LENGTH:
            hashWithPadding = {0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2b, 0x0e, 0x03, 0x02, 0x1a, 0x05, 0x00, 0x04, 0x14};
            break;
        case BINARY_SHA224_LENGTH:
            hashWithPadding = {0x30, 0x2d, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x04, 0x05, 0x00, 0x04, 0x1c};
            break;
        case BINARY_SHA256_LENGTH:
            hashWithPadding = {0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20};
            break;
        case BINARY_SHA384_LENGTH:
            hashWithPadding = {0x30, 0x41, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x02, 0x05, 0x00, 0x04, 0x30};
            break;
        case BINARY_SHA512_LENGTH:
            hashWithPadding = {0x30, 0x51, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x03, 0x05, 0x00, 0x04, 0x40};
            break;
        default:
            _log("incorrect digest length, dropping padding");
        }
        hashWithPadding.insert(hashWithPadding.end(), hash.begin(), hash.end());
        CK_ULONG signatureLength = 0;

        // Get response size
        C(Sign, sid, hashWithPadding.data(), hashWithPadding.size(), nullptr, &signatureLength);
        std::vector<unsigned char> signature(signatureLength, 0);

        // Get actual signature
        C(Sign, sid, hashWithPadding.data(), hashWithPadding.size(), signature.data(), &signatureLength);
        C(Logout, sid);
        _log("Signature: %s", toHex(signature).c_str());

        return signature;
    }

std::pair<int, int> PKCS11Module::getPINLengths(const std::vector<unsigned char> &cert) {
    // TODO: put into helper
    auto slotinfo = certs.find(cert);
    if (slotinfo == certs.end()) {
        throw std::runtime_error("Certificate not found in module");
    }
    auto slot = slotinfo->second;
    return std::make_pair(slot.first.pin_min, slot.first.pin_max);
}

bool PKCS11Module::isPinpad(const std::vector<unsigned char> &cert) const {
    auto slotinfo = certs.find(cert);
    if (slotinfo == certs.end()) {
        throw std::runtime_error("Certificate not found in module");
    }
    auto slot = slotinfo->second;
    return slot.first.has_pinpad;
}

// FIXME: assumes 3 tries for a PIN
int PKCS11Module::getPINRetryCount(const std::vector<unsigned char> &cert) const {
    auto slotinfo = certs.find(cert);
    if (slotinfo == certs.end()) {
        throw std::runtime_error("Certificate not found in module");
    }
    auto slot = slotinfo->second;
    if (slot.first.flags & CKF_USER_PIN_LOCKED)
        return 0;
    if (slot.first.flags & CKF_USER_PIN_FINAL_TRY)
        return 1;
    if (slot.first.flags & CKF_USER_PIN_COUNT_LOW)
        return 2;
    return 3;
}

P11Token PKCS11Module::getP11Token(std::vector<unsigned char> cert) const {
    auto slotinfo = certs.find(cert);
    if (slotinfo == certs.end()) {
        throw std::runtime_error("Certificate not found in module");
    }
    return slotinfo->second.first;
}
