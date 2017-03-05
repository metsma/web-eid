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

#include "modulemap.h"
#include "Logger.h"
#include "pcsc.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#ifndef _WIN32
#include <unistd.h>
#else
#include <io.h>
#endif

#ifdef _WIN32
#define F_OK 02
#define access _access
#endif

struct ModuleATR {
    std::string atr;
    std::string path;
};

static std::vector<ModuleATR> createMap() {
    // First add specific ATR-s
#ifdef __APPLE__
    // FIXME: Estonian PKCS#11 driver is buggy
    const std::string openscPath("/Library/OpenSC/lib/opensc-pkcs11.so");
    const std::string etokenPath("/Library/Frameworks/eToken.framework/Versions/Current/libeToken.dylib");
    const std::string estPath("/Library/EstonianIDCard/lib/esteid-pkcs11.so");
    const std::string latPath("/Library/latvia-eid/lib/otlv-pkcs11.so");
    const std::string finPath("/Library/mPolluxDigiSign/libcryptoki.dylib");
    const std::string litPath("/System/Library/Security/tokend/CCSuite.tokend/Contents/Frameworks/libccpkip11.dylib");
#else
    const std::string estPath("/usr/lib/pkcs11/opensc-pkcs11.so");
    const std::string latPath("otlv-pkcs11.so");
    const std::string finPath("opensc-pkcs11.so");
    const std::string litPath("/usr/lib/ccs/libccpkip11.so");
#endif


    std::vector<ModuleATR> m{
#ifdef __APPLE__
        {"3BD518008131FE7D8073C82110F4", etokenPath},
#endif
        {"3BFE9400FF80B1FA451F034573744549442076657220312E3043", estPath},
        {"3BDE18FFC080B1FE451F034573744549442076657220312E302B", estPath},
        {"3B5E11FF4573744549442076657220312E30", estPath},
        {"3B6E00004573744549442076657220312E30", estPath},

        {"3BFE1800008031FE454573744549442076657220312E30A8", estPath},
        {"3BFE1800008031FE45803180664090A4561B168301900086", estPath},
        {"3BFE1800008031FE45803180664090A4162A0083019000E1", estPath},
        {"3BFE1800008031FE45803180664090A4162A00830F9000EF", estPath},

        {"3BF9180000C00A31FE4553462D3443432D303181", estPath},
        {"3BF81300008131FE454A434F5076323431B7", estPath},
        {"3BFA1800008031FE45FE654944202F20504B4903", estPath},

        {"3BDD18008131FE45904C41545649412D65494490008C", latPath},

        {"3B7B940000806212515646696E454944", finPath},

        {"3BF81300008131FE45536D617274417070F8", litPath},
        {"3B7D94000080318065B08311C0A983009000", litPath},

        // Then add wildcards
#ifdef __linux__
        {"*", "/usr/lib/pkcs11/opensc-pkcs11.so"},
        {"*", "p11-kit-proxy.so"},
#elif defined __APPLE__
        {"*", "/Library/OpenSC/lib/opensc-pkcs11.so"},
#endif
    };

    // Convert all ATR strings to upper case
    for (auto e: m) {
        std::transform(e.atr.begin(), e.atr.end(), e.atr.begin(), ::toupper);
    }
    for (auto e: m) {
        _log("%s -> %s", e.atr.c_str(), e.path.c_str());
    }
    return m;
}

// Given a list of ATR-s, return a list of PKCS#11 module paths.
// Note that the size of the returned list can be longer than the size
// of atr list. Order is not in any preferred order.

std::vector<std::string> P11Modules::getPaths(const std::vector<std::vector<unsigned char>> &atrs) {
    static const std::vector<ModuleATR> atrToDriverList = createMap();
    std::vector<std::string> result;

    for (const auto &atrbytes: atrs) {
        std::string key = toHex(atrbytes);
        std::transform(key.begin(), key.end(), key.begin(), ::toupper);
        _log("Looking for %s", key.c_str());
        for (const auto &conf: atrToDriverList) {
            // The order up there is important
            // Check that the file actually exists
            // FIXME: check is only valid iv path contains /
            // try to dlopen if not slash
            if (access(conf.path.c_str(), F_OK ) != -1) {
                // TODO: only first driver is used now
                if (conf.atr == "*" || conf.atr == key) {
                    result.push_back(conf.path);
                    _log("selected PKCS#11 module %s for ATR %s", conf.path.c_str(), conf.atr.c_str());
                    break; // found a "valid" module, take next ATR
                }
                // Given ATR does not match
            } else {
                _log("ignoring missing PKCS#11 module %s", conf.path.c_str());
            }
        }
    }
    if (result.empty()) {
        _log("no suitable drivers found for a total of %d cards", atrs.size());
    }
    return result;
}
