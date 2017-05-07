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

#pragma once

#ifdef __APPLE__
#include <PCSC/winscard.h>
#include <PCSC/wintypes.h>
#else
#undef UNICODE
#include <winscard.h>
#endif

#include <vector>
#include <string>

struct PCSCReader {
    std::string name;
    std::vector<unsigned char> atr;
    bool inuse;
    bool exclusive;
    SCARD_READERSTATE state;
};

class PCSC {
public:
    static std::vector<std::vector<unsigned char>> atrList();
    static std::vector<PCSCReader> readerList(SCARDCONTEXT ctx = 0);
    static LONG cancel(SCARDCONTEXT ctx);

    LONG block();
    LONG connect(const std::string &reader, const std::string &protocol = "*");
    LONG wait(const std::string &reader, const std::string &protocol = "*");
    LONG transmit(const std::vector<unsigned char> &apdu, std::vector<unsigned char> &response);
    void disconnect();

    PCSCReader getStatus(); // XXX
    SCARDCONTEXT getContext(); // XXX
    ~PCSC();
    static const char *errorName(LONG err);
    DWORD protocol = SCARD_PROTOCOL_UNDEFINED; // XXX: maybe not public
private:

    LONG establish();

    bool established = false;
    bool connected = false;
    bool pnp = true;

    SCARDCONTEXT context;
    SCARDHANDLE card;
    PCSCReader status;
};
