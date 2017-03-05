#pragma once

#include <vector>
#include <string>
#include <map>

#include "pkcs11.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
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
};

class PKCS11Module {
public:
    void load(const std::string &module);
    std::vector<unsigned char> sign(const std::vector<unsigned char> &cert, const std::vector<unsigned char> &hash, const char *pin) const;

    bool isLoaded() {return !certs.empty();}
    std::vector<std::vector <unsigned char>> getCerts();
    std::vector<std::vector <unsigned char>> getSignCerts();
    std::vector<std::vector <unsigned char>> getAuthCerts();

    P11Token getP11Token(std::vector<unsigned char> cert) const;

    bool isPinpad(const std::vector<unsigned char> &cert) const;
    int getPINRetryCount(const std::vector<unsigned char> &cert) const;
    std::pair<int, int> getPINLengths(const std::vector<unsigned char> &cert);

    ~PKCS11Module();

private:
    std::string path;
#ifdef _WIN32
    HINSTANCE library = 0;
#else
    void *library = nullptr;
#endif

    CK_FUNCTION_LIST_PTR fl = nullptr;
    CK_SESSION_HANDLE session = 0; // active session

    // Contains all the certificates this module exposes
    // der maps to a pair of slot id and object id
    std::map<std::vector<unsigned char>, std::pair<P11Token, std::vector<unsigned char>>> certs;

    // locates the key handle for the key with the given ID
    std::vector<CK_OBJECT_HANDLE> getKey(CK_SESSION_HANDLE session, const std::vector<unsigned char> &id) const;
    std::vector<unsigned char> attribute(CK_ATTRIBUTE_TYPE type, CK_SESSION_HANDLE session, CK_OBJECT_HANDLE obj) const;
    std::vector<CK_OBJECT_HANDLE> objects(CK_OBJECT_CLASS objectClass, CK_SESSION_HANDLE session, CK_ULONG count) const;
    std::vector<CK_OBJECT_HANDLE> objects(const std::vector<CK_ATTRIBUTE> &attr, CK_SESSION_HANDLE session, CK_ULONG count) const;
};
