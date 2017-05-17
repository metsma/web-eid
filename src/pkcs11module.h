/*
 * Copyright (C) 2017 Martin Paljak
 */

#pragma once

#include <vector>
#include <string>
#include <map>

#include "pkcs11.h"
#include "Common.h"

#ifdef _WIN32
#include <windows.h>
#endif

// Helper structure for PKCS#11 token properties
struct P11Token {
    int pin_min; // Minimal pin length
    int pin_max; // Maximum pin length
    std::string label; // Token label
    bool has_pinpad; // true if pinpad present
    CK_SLOT_ID slot; // Associated slot ID
    CK_FLAGS flags; // all of the flags
    std::string module; // name of module
};

class PKCS11Module {
public:
    CK_RV load(const std::string &module);
    CK_RV login(const std::vector<unsigned char> &cert, const char *pin);
    CK_RV sign(const std::vector<unsigned char> &cert, const std::vector<unsigned char> &hash, std::vector<unsigned char> &result);

    CK_RV refresh();

    bool isLoaded() {
        return !certs.empty();
    }
    std::map<std::vector <unsigned char>, P11Token> getCerts();

    const P11Token getP11Token(const std::vector<unsigned char> &cert) const;

    bool isPinpad(const std::vector<unsigned char> &cert) const;
    static int getPINRetryCount(const P11Token &cert);
    std::pair<int, int> getPINLengths(const std::vector<unsigned char> &cert);

    ~PKCS11Module();

    static const char *errorName(CK_RV err);

private:
    std::string path;
    bool initialized = false;
#ifdef _WIN32
    HINSTANCE library = 0;
#else
    void *library = nullptr;
#endif

    CK_FUNCTION_LIST_PTR fl = nullptr;
    CK_SESSION_HANDLE session = CK_INVALID_HANDLE; // active session

    // Contains all the certificates this module exposes
    // der maps to a pair of slot id and object id
    std::map<std::vector<unsigned char>, std::pair<P11Token, std::vector<unsigned char>>> certs;

    // locates the key handle for the key with the given ID
    std::vector<CK_OBJECT_HANDLE> getKey(CK_SESSION_HANDLE session, const std::vector<unsigned char> &id) const;
    std::vector<unsigned char> attribute(CK_ATTRIBUTE_TYPE type, CK_SESSION_HANDLE session, CK_OBJECT_HANDLE obj) const;
    std::vector<CK_OBJECT_HANDLE> objects(CK_OBJECT_CLASS objectClass, CK_SESSION_HANDLE session, CK_ULONG count) const;
    std::vector<CK_OBJECT_HANDLE> objects(const std::vector<CK_ATTRIBUTE> &attr, CK_SESSION_HANDLE session, CK_ULONG count) const;
};
