/*
 * Copyright (C) 2017 Martin Paljak
 */

#include "modulemap.h"
#include "debuglog.h"
#include "util.h"

#include <sstream>

#include <stdio.h>
#include <stdlib.h>

#ifndef _WIN32
#include <unistd.h>
#include <dlfcn.h>
#else
#include <windows.h>
#include <io.h>
#define F_OK 02
#define access _access
#endif


// We have a list of named lists
struct ModuleATR {
    std::string name;
    std::vector<std::string> atrs;
    std::vector<std::string> paths;
};

static std::vector<ModuleATR> createMap() {
    // First add specific ATR-s
    std::vector<ModuleATR> m{
        {   "e-token",
            {"3BD518008131FE7D8073C82110F4"},
            {"/Library/Frameworks/eToken.framework/Versions/Current/libeToken.dylib"}
        },
        {   "Estonian ID-card",
            {   "3BFE9400FF80B1FA451F034573744549442076657220312E3043",
                "3BDE18FFC080B1FE451F034573744549442076657220312E302B",
                "3B5E11FF4573744549442076657220312E30",
                "3B6E00004573744549442076657220312E30",
                "3BFE1800008031FE454573744549442076657220312E30A8",
                "3BFE1800008031FE45803180664090A4561B168301900086",
                "3BFE1800008031FE45803180664090A4162A0083019000E1",
                "3BFE1800008031FE45803180664090A4162A00830F9000EF",
                "3BF9180000C00A31FE4553462D3443432D303181",
                "3BF81300008131FE454A434F5076323431B7",
                "3BFA1800008031FE45FE654944202F20504B4903"
            },
            {"/Library/EstonianIDCard/lib/esteid-pkcs11.so", "/Library/OpenSC/lib/opensc-pkcs11.so", "opensc-pkcs11.so", "CAPI"}
        },
        {   "Latvian ID-card",
            {"3BDD18008131FE45904C41545649412D65494490008C"},
            {"/Library/latvia-eid/lib/otlv-pkcs11.so", "otlv-pkcs11.so"}
        },
        {   "Finnish ID-card",
            {"3B7B940000806212515646696E454944"},
            {"/Library/mPolluxDigiSign/libcryptoki.dylib", "opensc-pkcs11.so"}
        },
        {   "Lithuanian ID-card",
            {   "3BF81300008131FE45536D617274417070F8",
                "3B7D94000080318065B08311C0A983009000"
            },
            {"/System/Library/Security/tokend/CCSuite.tokend/Contents/Frameworks/libccpkip11.dylib", "/usr/lib/ccs/libccpkip11.so"}
        },
        // Then add some last resort wildcards
        {"OpenSC fallback", {"*"}, {"/Library/OpenSC/lib/opensc-pkcs11.so", "opensc-pkcs11.so"}},
#ifdef __linux__
        //{"p11-kit fallback", {"*"}, {"p11-kit-proxy.so"}},
#endif
    };

    // In each configuration list element
    for (auto &e: m) {
        // convert the ATR list element contents to upper case
        for (auto &ae: e.atrs) {
            std::transform(ae.begin(), ae.end(), ae.begin(), ::toupper);
        }
    }

    // Log
    for (auto &e: m) {
        std::stringstream msg;
        msg << e.name << " is handled by ";
        for (auto &me: e.paths) {
            msg << me << " ";
        }
        _log("%s", msg.str().c_str());
    }
    return m;
}

// Given a list of ATR-s, return a list of PKCS#11 modules.
// We do not know which ATR is of the card that is supposed to be used
// nor do we know for sure which card is handled by which driver.

std::vector<std::string> P11Modules::getPaths(const std::vector<std::vector<unsigned char>> &atrs) {
    static const std::vector<ModuleATR> atrToDriverList = createMap();
    std::vector<std::string> result;

    // For every ATR ...
    for (const auto &atrbytes: atrs) {
        // convert ATR byte array to upper case HEX
        std::string key = toHex(atrbytes);
        std::transform(key.begin(), key.end(), key.begin(), ::toupper);
        _log("Looking for %s", key.c_str());
        for (const auto &conf: atrToDriverList) {
            // Checking if ATR matches one in the list
            bool atr_match = std::any_of(conf.atrs.cbegin(), conf.atrs.cend(), [&](const std::string &atr) {
                if (atr == "*" || atr == key) {
                    _log("ATR matches %s: %s", conf.name.c_str(), atr.c_str());
                    return true;
                }
                return false;
            });
            // ATR matched config entry.
            if (!atr_match)
                continue;
            // Check if any of the modules is usable/available
            for (const auto &path: conf.paths) {
                // 1. If module contains path separators, check if file exists
                if (path.find_first_of("/\\") != std::string::npos) {
                    if (access(path.c_str(), F_OK ) == -1) {
                        _log("ignoring missing PKCS#11 module %s", path.c_str());
                        continue;
                    }
                }
                // try to open XXX: wrap in some common header
                // TODO: maybe check if function list present ?
#ifdef _WIN32
                HINSTANCE handle = LoadLibraryA(path.c_str());
#else
                void *handle = dlopen(path.c_str(), RTLD_LOCAL | RTLD_NOW);
#endif
                if (!handle) {
                    _log("ignoring PKCS#11 module that did not load: %s", path.c_str());
                    continue;
                }
#ifdef _WIN32
                FreeLibrary(handle);
#else
                dlclose(handle);
#endif
                // Assume usable module if dlopen is successful
                result.push_back(path);
                _log("%s found usable as %s via %s", key.c_str(), conf.name.c_str(), path.c_str());
                break;
            }
        }
    }
    if (result.empty()) {
        _log("no suitable drivers found for a total of %d cards", atrs.size());
    }
    return result;
}
