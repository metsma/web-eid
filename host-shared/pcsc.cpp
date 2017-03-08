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

#ifdef __APPLE__
#include <PCSC/winscard.h>
#include <PCSC/wintypes.h>
#else
#undef UNICODE
#include <winscard.h>
#endif

#include <stdexcept>

#define MAX_ATR_SIZE 33	/**< Maximum ATR size */

std::vector<std::vector<unsigned char>> PCSC::atrList(bool all = false) {
	SCARDCONTEXT hContext;
	std::vector<std::vector<unsigned char>> result;
	LONG err = SCardEstablishContext(SCARD_SCOPE_USER, NULL, NULL, &hContext);
	if (err != SCARD_S_SUCCESS) {
		_log("SCardEstablishContext ERROR: %x", err);
		return result;
	}

	DWORD size;
	err = SCardListReaders(hContext, NULL, NULL, &size);
	if (err != SCARD_S_SUCCESS || !size) {
		_log("SCardListReaders || !size ERROR: %x", err);
		SCardReleaseContext(hContext);
		return result;
	}

	std::string readers(size, 0);
	err = SCardListReaders(hContext, NULL, &readers[0], &size);
	readers.resize(size);
	if (err != SCARD_S_SUCCESS) {
		_log("SCardListReaders ERROR: %x", err);
		SCardReleaseContext(hContext);
		return result;
	}

	for (std::string::const_iterator i = readers.begin(); i != readers.end(); ++i) {
		std::string name(&*i);
		i += name.size();
		if (name.empty())
			continue;

		_log("found reader: %s", name.c_str());

		SCARDHANDLE cardHandle = 0;
		DWORD dwProtocol = 0;
		// Use direct mode if "all readers" is set
		LONG err = SCardConnect(hContext, name.c_str(), all ? SCARD_SHARE_DIRECT : SCARD_SHARE_SHARED, SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, &cardHandle, &dwProtocol);
        switch(LONG(err))
        {
        case LONG(SCARD_S_SUCCESS):
            break;
        case LONG(SCARD_E_SHARING_VIOLATION):
			_log("SCardConnect: %s is used exclusively by another application, ignoring", name.c_str());
			continue;
        case LONG(SCARD_E_NO_SMARTCARD):
			_log("SCardConnect: ignoring empty reader %s", name.c_str());
			continue;
        default:
			_log("SCardConnect ERROR for %s: %x", name.c_str(), err);
			continue;
		}

		std::vector<unsigned char> bAtr(MAX_ATR_SIZE, 0);
		DWORD atrSize = DWORD(bAtr.size());
		err = SCardStatus(cardHandle, nullptr, nullptr, nullptr, nullptr, bAtr.data(), &atrSize);
		if (err == SCARD_S_SUCCESS) {
			bAtr.resize(atrSize);
			result.push_back(bAtr);
			_log("%s: %s", name.c_str(), toHex(bAtr).c_str());
		}
		else {
			_log("SCardStatus ERROR for %s: %x", name.c_str(), err);
		}
		SCardDisconnect(cardHandle, SCARD_LEAVE_CARD);
	}

	SCardReleaseContext(hContext);
	return result;
}
