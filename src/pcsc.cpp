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

#include "pcsc.h"
#include "Logger.h"
#include "util.h"

#include <cstring>

template < typename Func, typename... Args>
LONG SCCall(const char *fun, const char *file, int line, const char *function, Func func, Args... args)
{
    // TODO: log parameters
    LONG err = func(args...);
    Logger::writeLog(fun, file, line, "%s: %s", function, PCSC::errorName(err));
    return err;
}
#define SCard(API, ...) SCCall(__FUNCTION__, __FILE__, __LINE__, "SCard"#API, SCard##API, __VA_ARGS__)


const PCSCReader *from_name(const std::string &name, const std::vector<PCSCReader> &readers) {
    for (const auto &reader: readers) {
        if (reader.name == name) {
            return &reader;
        }
    }
    return nullptr;
}

// XXX
PCSCReader PCSC::getStatus()
{
  return status;
}

SCARDCONTEXT PCSC::getContext() {
  return context;
}

LONG PCSC::connect(const std::string &reader, const std::string &protocol) {
    _log("Connecting to card in %s with %s", reader.c_str(), protocol.c_str());
    LONG err = SCARD_S_SUCCESS;

    // Create context, if not yet connected
    if (!established) {
        err = SCard(EstablishContext, SCARD_SCOPE_USER, nullptr, nullptr, &context);
        if (err != SCARD_S_SUCCESS) {
            return err;
        }
        established = true;
    }

    // Quick query. XXX: glitch ? if the reader has not been listed in this context,
    // connect on Linux sometimes fails with SCARD_E_REDER_UNAVAILABLE ?
    std::vector<PCSCReader> readers = readerList(context);

    const PCSCReader *wanted = from_name(reader, readers);
    if (!wanted) {
        _log("Reader %s not found from reader list", reader.c_str());
    }

    if (wanted->exclusive) {
        _log("Can not connect to a reader used in exclusive mode");
        return SCARD_E_SHARING_VIOLATION;
    }

    DWORD proto = SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1;
    if (protocol == "T=0") {
        proto = SCARD_PROTOCOL_T0;
    } else if (protocol == "T=1") {
        proto = SCARD_PROTOCOL_T1;
    } else if (protocol == "*") {
        // do nothing, both already enabled
    } else {
        return SCARD_E_INVALID_PARAMETER;
    }


    // TODO: Windows and exclusive access
    DWORD mode = SCARD_SHARE_SHARED;
    if (wanted->inuse) {
#ifdef _WIN32
        return SCARD_E_SHARING_VIOLATION; // FIXME: lots of UX love here
#endif
        err = SCard(Connect, context, reader.c_str(), SCARD_SHARE_SHARED, proto, &card, &this->protocol);
        if (err != SCARD_S_SUCCESS)
            return err;
    } else {
        _log("Reader is not in use, assuming exclusive access is possible");
        mode = SCARD_SHARE_EXCLUSIVE;
        err = SCard(Connect, context, reader.c_str(), mode, proto, &card, &this->protocol);
        if (err != SCARD_S_SUCCESS)
            return err;
    }
#ifndef _WIN32
    err = SCard(BeginTransaction, card);
    if (err != SCARD_S_SUCCESS)
        return err;
#endif
    _log("Connected to %s in %s mode, protocol %s", reader.c_str(), mode == SCARD_SHARE_EXCLUSIVE ? "exclusive" : "shared", this->protocol == SCARD_PROTOCOL_T0 ? "T=0" : "T=1");
    status = *wanted;
    connected = true;
    return err;
}
LONG PCSC::wait(const std::string &reader, const std::string &protocol) {

    // FIXME: copypaste
    // Quick query
    std::vector<PCSCReader> readers = readerList();
    LONG err = SCARD_S_SUCCESS;

    const PCSCReader *wanted = from_name(reader, readers);
    if (!wanted) {
        _log("Reader %s not found from reader list", reader.c_str());
    }

    if (wanted->exclusive) {
        _log("Can not connect to a reader used in exclusive mode");
        return SCARD_E_SHARING_VIOLATION;
    }

    // Create context, if not yet connected FIXME: copypaste
    if (!established) {
        err = SCard(EstablishContext, SCARD_SCOPE_USER, nullptr, nullptr, &context);
        if (err != SCARD_S_SUCCESS) {
            return err;
        }
        established = true;
    }

    if (wanted->atr.empty()) {
        SCARD_READERSTATE state;
        state.dwCurrentState = wanted->state.dwEventState;
        do {
            // Wait for card insertion
            state.szReader = wanted->name.c_str();
            state.dwEventState = SCARD_STATE_UNAWARE;
            err = SCard(GetStatusChange, context, 10 * 1000, &state, DWORD(1));
            if (err != SCARD_S_SUCCESS) {
                 return err;
            }
            state.dwCurrentState = state.dwEventState;
            // TODO: visual feedback in reader selection UI that the card is mute
        } while (!(state.dwCurrentState & SCARD_STATE_PRESENT) || (state.dwCurrentState & SCARD_STATE_MUTE));

    }
    return connect(reader, protocol);
}


void PCSC::disconnect() {
    if (connected) {
#ifndef _WIN32
        // No transactions on Windows due to the "5 second rule"
        SCard(EndTransaction, card, SCARD_LEAVE_CARD);
#endif
        SCard(Disconnect, card, SCARD_RESET_CARD);
    }
    connected = false;
}

// Intended to be called from a different thread than the rest of the code
LONG PCSC::cancel(SCARDCONTEXT ctx) {
    return SCard(Cancel, ctx);
}


LONG PCSC::transmit(const std::vector<unsigned char> &apdu, std::vector<unsigned char> &response) {
    _log("PCSC: sending %s", toHex(apdu).c_str());

    SCARD_IO_REQUEST req;
    req.dwProtocol = protocol;
    DWORD rlen = response.size();
    LONG err = SCard(Transmit, card, &req, &apdu[0], DWORD(apdu.size()), &req, &response[0], &rlen);
    if (err != SCARD_S_SUCCESS) {
        response.resize(0);
        return err;
    }
    response.resize(rlen);
    _log("PCSC: received %s", toHex(response).c_str());
    return err;
}

PCSC::~PCSC() {
    if (connected) {
        SCard(Disconnect, card, SCARD_LEAVE_CARD);
    }
    if (established) {
        SCard(ReleaseContext, context);
    }
}

// TODO: get rid of this
std::vector<std::vector<unsigned char>> PCSC::atrList() {
    std::vector<std::vector<unsigned char>> result;
    for (const auto &reader: readerList()) {
        if (!reader.exclusive && !reader.atr.empty()) {
            result.push_back(reader.atr);
        }
    }
    return result;
}


std::vector<PCSCReader> PCSC::readerList(SCARDCONTEXT ctx) {
    std::vector<PCSCReader> result;
    SCARDCONTEXT hContext;
    LONG err = SCARD_S_SUCCESS;
    if (ctx) {
        hContext = ctx;
    } else {
        err = SCard(EstablishContext, SCARD_SCOPE_USER, nullptr, nullptr, &hContext);
        if (err != SCARD_S_SUCCESS) {
            return result;
        }
    }

    DWORD size;
    err = SCard(ListReaders, hContext, nullptr, nullptr, &size);
    if (err != SCARD_S_SUCCESS || !size) {
        _log("SCardListReaders: %s %d", errorName(err), size);
        if (!ctx)
            SCard(ReleaseContext, hContext);
        return result;
    }

    std::string readers(size, 0);
    err = SCard(ListReaders, hContext, nullptr, &readers[0], &size);
    readers.resize(size);
    if (err != SCARD_S_SUCCESS) {
        if (!ctx)
            SCard(ReleaseContext, hContext);
        return result;
    }

    // Extract reader names
    std::vector<std::string> readernames;
    for (std::string::const_iterator i = readers.begin(); i != readers.end(); ++i) {
        std::string name(&*i);
        i += name.size();
        if (name.empty())
            continue;
        readernames.push_back(name);
    }

    // Construct status query vector
    std::vector<SCARD_READERSTATE> statuses(readernames.size());
    for (size_t i = 0; i < readernames.size(); i++) {
        statuses[i].szReader = readernames[i].c_str();
        statuses[i].dwCurrentState = SCARD_STATE_UNAWARE;
        statuses[i].dwEventState = SCARD_STATE_UNAWARE;
    }

    // Query statuses
    SCard(GetStatusChange, hContext, 0, &statuses[0], DWORD(statuses.size()));
    if (err != SCARD_S_SUCCESS) {
        if (!ctx)
            SCard(ReleaseContext, hContext);
        return result;
    }

    // Construct reader names list
    for (auto &i: statuses) {
        std::string reader(i.szReader);
        bool inuse = i.dwEventState & SCARD_STATE_INUSE;
        bool exclusive = i.dwEventState & SCARD_STATE_EXCLUSIVE;
        bool mute = i.dwEventState & SCARD_STATE_MUTE;
        _log("found reader: %s", reader.c_str());
        _log("  exclusive:%s inuse:%s mute:%s", exclusive?"true":"false", inuse?"true":"false", mute?"true":"false");
        std::vector<unsigned char> atr(i.rgbAtr, i.rgbAtr + i.cbAtr);
        if (!atr.empty()) {
            _log("  atr:%s", toHex(atr).c_str());
        }
        result.push_back({reader, atr, inuse, exclusive, i});
    }
    if (!ctx)
        SCard(ReleaseContext, hContext);
    return result;
}

// List taken from pcsc-lite source
const char *PCSC::errorName(LONG err) {
      switch (err)
      {
      case LONG(SCARD_S_SUCCESS):
            return "SCARD_S_SUCCESS";
      case LONG(SCARD_E_CANCELLED):
            return "SCARD_E_CANCELLED";
      case LONG(SCARD_E_CANT_DISPOSE):
            return "SCARD_E_CANT_DISPOSE";
      case LONG(SCARD_E_INSUFFICIENT_BUFFER):
            return "SCARD_E_INSUFFICIENT_BUFFER";
      case LONG(SCARD_E_INVALID_ATR):
            return "SCARD_E_INVALID_ATR";
      case LONG(SCARD_E_INVALID_HANDLE):
            return "SCARD_E_INVALID_HANDLE";
      case LONG(SCARD_E_INVALID_PARAMETER):
            return "SCARD_E_INVALID_PARAMETER";
      case LONG(SCARD_E_INVALID_TARGET):
            return "SCARD_E_INVALID_TARGET";
      case LONG(SCARD_E_INVALID_VALUE):
            return "SCARD_E_INVALID_VALUE";
      case LONG(SCARD_E_NO_MEMORY):
            return "SCARD_E_NO_MEMORY";
      case LONG(SCARD_F_COMM_ERROR):
            return "SCARD_F_COMM_ERROR";
      case LONG(SCARD_F_INTERNAL_ERROR):
            return "SCARD_F_INTERNAL_ERROR";
      case LONG(SCARD_F_UNKNOWN_ERROR):
            return "SCARD_F_UNKNOWN_ERROR";
      case LONG(SCARD_F_WAITED_TOO_LONG):
            return "SCARD_F_WAITED_TOO_LONG";
      case LONG(SCARD_E_UNKNOWN_READER):
            return "SCARD_E_UNKNOWN_READER";
      case LONG(SCARD_E_TIMEOUT):
            return "SCARD_E_TIMEOUT";
      case LONG(SCARD_E_SHARING_VIOLATION):
            return "SCARD_E_SHARING_VIOLATION";
      case LONG(SCARD_E_NO_SMARTCARD):
            return "SCARD_E_NO_SMARTCARD";
      case LONG(SCARD_E_UNKNOWN_CARD):
            return "SCARD_E_UNKNOWN_CARD";
      case LONG(SCARD_E_PROTO_MISMATCH):
            return "SCARD_E_PROTO_MISMATCH";
      case LONG(SCARD_E_NOT_READY):
            return "SCARD_E_NOT_READY";
      case LONG(SCARD_E_SYSTEM_CANCELLED):
            return "SCARD_E_SYSTEM_CANCELLED";
      case LONG(SCARD_E_NOT_TRANSACTED):
            return "SCARD_E_NOT_TRANSACTED";
      case LONG(SCARD_E_READER_UNAVAILABLE):
            return "SCARD_E_READER_UNAVAILABLE";
      case LONG(SCARD_W_UNSUPPORTED_CARD):
            return "SCARD_W_UNSUPPORTED_CARD";
      case LONG(SCARD_W_UNRESPONSIVE_CARD):
            return "SCARD_W_UNRESPONSIVE_CARD";
      case LONG(SCARD_W_UNPOWERED_CARD):
            return "SCARD_W_UNPOWERED_CARD";
      case LONG(SCARD_W_RESET_CARD):
            return "SCARD_W_RESET_CARD";
      case LONG(SCARD_W_REMOVED_CARD):
            return "SCARD_W_REMOVED_CARD";
#ifdef SCARD_W_INSERTED_CARD
      case LONG(SCARD_W_INSERTED_CARD):
            return "SCARD_W_INSERTED_CARD";
#endif
      case LONG(SCARD_E_UNSUPPORTED_FEATURE):
            return "SCARD_E_UNSUPPORTED_FEATURE";
      case LONG(SCARD_E_PCI_TOO_SMALL):
            return "SCARD_E_PCI_TOO_SMALL";
      case LONG(SCARD_E_READER_UNSUPPORTED):
            return "SCARD_E_READER_UNSUPPORTED";
      case LONG(SCARD_E_DUPLICATE_READER):
            return "SCARD_E_DUPLICATE_READER";
      case LONG(SCARD_E_CARD_UNSUPPORTED):
            return "SCARD_E_CARD_UNSUPPORTED";
      case LONG(SCARD_E_NO_SERVICE):
            return "SCARD_E_NO_SERVICE";
      case LONG(SCARD_E_SERVICE_STOPPED):
            return "SCARD_E_SERVICE_STOPPED";
      case LONG(SCARD_E_NO_READERS_AVAILABLE):
            return "SCARD_E_NO_READERS_AVAILABLE";
      default:
            return "UNKNOWN";
      };
}
