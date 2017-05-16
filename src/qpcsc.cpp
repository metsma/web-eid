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

#include "qpcsc.h"

#include "Logger.h"
#include "util.h"

#include <set>
#include <map>

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QJsonObject>
#include <QJsonDocument>
#include <QMutexLocker>
#include <QTime>


/*
 QtPCSC is:
 - a QObject
 - lives in a dedicated thread
 - represents the PC/SC subsystem of a host machine to the app
 - translates PCSC events to Qt signals.

Connecting to a reader will create another QObject, living in a separat thread, that owns the connection
until an error occures or it is closed and provides APDU transport to the reader.

*/

template <typename Func, typename... Args>
LONG SCCall(const char *fun, const char *file, int line, const char *function, Func func, Args... args)
{
    // TODO: log parameters
    LONG err = func(args...);
    Logger::writeLog(fun, file, line, "%s: %s (0x%08x)", function, QtPCSC::errorName(err), err);
    return err;
}
#define SCard(API, ...) SCCall(__FUNCTION__, __FILE__, __LINE__, "SCard" #API, SCard##API, __VA_ARGS__)

// return the rv is not CKR_OK
#define check_SCard(API, ...)                                                                            \
    do                                                                                                   \
    {                                                                                                    \
        LONG _ret = SCCall(__FUNCTION__, __FILE__, __LINE__, "SCard" #API, SCard##API, __VA_ARGS__);     \
        if (_ret != SCARD_S_SUCCESS)                                                                     \
        {                                                                                                \
            Logger::writeLog(__FUNCTION__, __FILE__, __LINE__, "returning %s", QtPCSC::errorName(_ret)); \
            return _ret;                                                                                 \
        }                                                                                                \
    } while (0)

#define PNP_READER_NAME "\\\\?PnP?\\Notification"

// List taken from pcsc-lite source
const char *QtPCSC::errorName(LONG err)
{
#define CASE(X) case LONG(X): return #X
    switch (err) {
        CASE(SCARD_S_SUCCESS);
        CASE(SCARD_E_CANCELLED);
        CASE(SCARD_E_CANT_DISPOSE);
        CASE(SCARD_E_INSUFFICIENT_BUFFER);
        CASE(SCARD_E_INVALID_ATR);
        CASE(SCARD_E_INVALID_HANDLE);
        CASE(SCARD_E_INVALID_PARAMETER);
        CASE(SCARD_E_INVALID_TARGET);
        CASE(SCARD_E_INVALID_VALUE);
        CASE(SCARD_E_NO_MEMORY);
        CASE(SCARD_F_COMM_ERROR);
        CASE(SCARD_F_INTERNAL_ERROR);
        CASE(SCARD_F_UNKNOWN_ERROR);
        CASE(SCARD_F_WAITED_TOO_LONG);
        CASE(SCARD_E_UNKNOWN_READER);
        CASE(SCARD_E_TIMEOUT);
        CASE(SCARD_E_SHARING_VIOLATION);
        CASE(SCARD_E_NO_SMARTCARD);
        CASE(SCARD_E_UNKNOWN_CARD);
        CASE(SCARD_E_PROTO_MISMATCH);
        CASE(SCARD_E_NOT_READY);
        CASE(SCARD_E_SYSTEM_CANCELLED);
        CASE(SCARD_E_NOT_TRANSACTED);
        CASE(SCARD_E_READER_UNAVAILABLE);
        CASE(SCARD_W_UNSUPPORTED_CARD);
        CASE(SCARD_W_UNRESPONSIVE_CARD);
        CASE(SCARD_W_UNPOWERED_CARD);
        CASE(SCARD_W_RESET_CARD);
        CASE(SCARD_W_REMOVED_CARD);
#ifdef SCARD_W_INSERTED_CARD
        CASE(SCARD_W_INSERTED_CARD);
#endif
        CASE(SCARD_E_UNSUPPORTED_FEATURE);
        CASE(SCARD_E_PCI_TOO_SMALL);
        CASE(SCARD_E_READER_UNSUPPORTED);
        CASE(SCARD_E_DUPLICATE_READER);
        CASE(SCARD_E_CARD_UNSUPPORTED);
        CASE(SCARD_E_NO_SERVICE);
        CASE(SCARD_E_SERVICE_STOPPED);
        CASE(SCARD_E_NO_READERS_AVAILABLE);
    default:
        return "UNKNOWN";
    };
}

static QStringList stateNames(DWORD state) {
    QStringList result;
#define STATE(X) if( state & SCARD_STATE_##X ) result << #X
    STATE(IGNORE);
    STATE(CHANGED);
    STATE(UNKNOWN);
    STATE(UNAVAILABLE);
    STATE(EMPTY);
    STATE(PRESENT);
    STATE(ATRMATCH);
    STATE(EXCLUSIVE);
    STATE(INUSE);
    STATE(MUTE);
    return result;
}

static QStringList readerStateNames(DWORD state) {
    QStringList result;
#define RSTATE(X) if( state & SCARD_##X ) result << #X
    RSTATE(ABSENT);
    RSTATE(PRESENT);
    RSTATE(SWALLOWED);
    RSTATE(POWERED);
    RSTATE(NEGOTIABLE);
    RSTATE(SPECIFIC);
    return result;
}
void QtPCSC::run()
{
    LONG rv = SCARD_S_SUCCESS;

    // TODO: handle the case where the resource manager is not running.
    rv = SCard(EstablishContext, SCARD_SCOPE_USER, nullptr, nullptr, &context);
    if (rv != SCARD_S_SUCCESS) {
        return emit error("", rv);
    }

    // Check if PnP is NOT supported
    SCARD_READERSTATE state;
    state.dwCurrentState = SCARD_STATE_UNAWARE;
    state.szReader = PNP_READER_NAME;
    rv = SCard(GetStatusChange, context, 0, &state, DWORD(1));

    if ((rv == LONG(SCARD_E_TIMEOUT)) && (state.dwEventState & SCARD_STATE_UNKNOWN)) {
        _log("No PnP support");
        pnp = false;
    }

    bool list = true;
    bool change = false; // if a list change signal should be emitted
    std::set<std::string> readernames;
    DWORD pnpstate = SCARD_STATE_UNAWARE;
    // Wait for events
    do {
        std::vector<SCARD_READERSTATE> statuses;

        if (list)  {
            // List readers
            readernames.clear();
            DWORD size;
            rv = SCard(ListReaders, context, nullptr, nullptr, &size);
            if (rv != SCARD_S_SUCCESS || !size) {
                _log("SCardListReaders(size): %s %d", errorName(rv), size);
                return emit error("", rv);
            }

            std::string readers(size, 0);
            rv = SCard(ListReaders, context, nullptr, &readers[0], &size);
            if (rv != SCARD_S_SUCCESS) {
                _log("SCardListReaders: %s", errorName(rv));
                return emit error("", rv);
            }
            readers.resize(size);
            // Extract reader names
            for (std::string::const_iterator i = readers.begin(); i != readers.end(); ++i) {
                std::string name(&*i);
                i += name.size();
                if (name.empty())
                    continue;
                readernames.insert(name);
                _log("Listed %s", name.c_str());
            }
            // Remove unknown readers
            for (auto &e: known.keys()) {
                if (readernames.count(e) == 0) {
                    mutex.lock();
                    known.remove(e);
                    mutex.unlock();
                    _log("Emitting remove signal");
                    emit readerRemoved(QString::fromStdString(e));
                    // card removed event was done in previous loop
                    emit readerListChanged(getReaders());
                }
            }
            // Add new readers
            for (auto &e: readernames) {
                if (!known.contains(e)) {
                    // New reader detected
                    mutex.lock();
                    known[e] = SCARD_STATE_UNAWARE;
                    mutex.unlock();
                    emit readerAttached(QString::fromStdString(e));
                    change = true; // after we know the possibly interesting state
                }
            }
            // Do not list on next round, unless necessary
            list = false;
        }

        // Construct status query vector
        statuses.resize(0);
        for (auto &r: readernames) {
            statuses.push_back({r.c_str(), nullptr, known[r], SCARD_STATE_UNAWARE, 0, {0}});
        }
        // Append PnP, if supported
        if (pnp) {
#ifdef Q_OS_MAC
            pnpstate = SCARD_STATE_UNAWARE;
#endif
            statuses.push_back({PNP_READER_NAME, nullptr, pnpstate, SCARD_STATE_UNAWARE, 0, {0}});
        }

        // Debug
        for (auto &r: statuses) {
            _log("Querying %s: %s (0x%x)", r.szReader, stateNames(r.dwCurrentState).join(" ").toStdString().c_str(), r.dwCurrentState);
        }

        // Query statuses
        rv = SCard(GetStatusChange, context, 600000, &statuses[0], DWORD(statuses.size()));
        if (rv == LONG(SCARD_E_UNKNOWN_READER)) {
            // List changed while in air, try again
            list = true;
            continue;
        }
        if (rv == LONG(SCARD_E_TIMEOUT) || rv == LONG(SCARD_S_SUCCESS)) {
            // Check if PnP event, always remove from vector
            if (pnp) {
                if (statuses.back().dwEventState & SCARD_STATE_CHANGED) {
                    _log("PnP event: %s (0x%x)", qPrintable(stateNames(statuses.back().dwEventState).join(" ")), statuses.back().dwEventState);
                    // Windows has the number of connected readers in the high word of the status
                    pnpstate = statuses.back().dwEventState & ~SCARD_STATE_CHANGED;
                    list = true;
                }
                statuses.pop_back();
            }
            for (auto &i: statuses) {
                std::string reader(i.szReader);
                _log("%s: %s (0x%x)", reader.c_str(), qPrintable(stateNames(i.dwEventState).join(" ")), i.dwEventState);
                if (!(i.dwEventState & SCARD_STATE_CHANGED)) {
                    _log("No change: %s", reader.c_str());
                    continue;
                }

                if (i.dwEventState & SCARD_STATE_UNKNOWN) {
                    _log("reader removed: %s", reader.c_str());
                    list = true;
                    // Also emit card removed signal, if card was present
                    if (known[reader] & SCARD_STATE_PRESENT) {
                        emit cardRemoved(QString::fromStdString(reader));
                        // reader list change will be in next loop
                    }
                    continue;
                }
                if ((i.dwEventState & SCARD_STATE_PRESENT) && !(known[reader] & SCARD_STATE_PRESENT)) {
                    if (i.dwEventState & SCARD_STATE_MUTE) {
                        _log("Card in %s is mute", reader.c_str());
                        emit error(QString::fromStdString(reader), SCARD_W_UNRESPONSIVE_CARD);
                    } else {
                        QByteArray atr = QByteArray::fromRawData((const char *)i.rgbAtr, i.cbAtr);
                        if (!atr.isEmpty()) {
                            _log("  atr:%s", atr.toHex().toStdString().c_str());
                        }
                        emit cardInserted(QString::fromStdString(reader), atr);
                        change = true;
                    }
                } else if ((i.dwEventState & SCARD_STATE_EMPTY) && (known[reader] & SCARD_STATE_PRESENT) && !(known[reader] & SCARD_STATE_MUTE)) {
                    emit cardRemoved(QString::fromStdString(reader));
                    change = true;
                }

                mutex.lock();
                known[reader] = i.dwEventState & ~SCARD_STATE_CHANGED;
                mutex.unlock();
            }
            // card related list change
            if (change) {
                emit readerListChanged(getReaders());
                change = false;
            }
        }
    } while ((rv == LONG(SCARD_S_SUCCESS) || rv == LONG(SCARD_E_TIMEOUT)) && !isInterruptionRequested());
    _log("Quitting PCSC thread");
    SCard(ReleaseContext, context);
}

// The rest are called from main thread
void QtPCSC::cancel() {
    SCard(Cancel, getContext());
}

QMap<QString, QStringList> QtPCSC::getReaders() {
    QMutexLocker locker(&mutex);

    QMap<QString, QStringList> result;
    for (const auto &e: known.keys()) {
        result[QString::fromStdString(e)] = stateNames(known[e]);
    }
    return result;
}

QPCSCReader *QtPCSC::connectReader(WebContext *webcontext, const QString &reader, const QString &protocol, bool wait) {
    _log("connecting to %s", qPrintable(reader));
    auto rdrs = getReaders();
    // check if empty and show dialog. wired to open, or call open directly
    if (!rdrs.contains(reader)) {
        _log("%s does not exist", qPrintable(reader));
        return nullptr;
    }

    QPCSCReader *result = new QPCSCReader(webcontext, this, reader, protocol);

    connect(this, &QtPCSC::readerRemoved, result, &QPCSCReader::readerRemoved, Qt::QueuedConnection);

    if (!rdrs[reader].contains("PRESENT") && wait) {
        _log("Showing insert reader dialog");
        result->insertCardDialog.showIt(webcontext->friendlyOrigin(), reader);
        connect(this, &QtPCSC::cardInserted, result, &QPCSCReader::cardInserted, Qt::QueuedConnection);
        connect(&result->insertCardDialog, &QDialog::rejected, result, &QPCSCReader::disconnect);
        // Close if event. TODO: handle MUTE
        connect(result, &QPCSCReader::connected, &result->insertCardDialog, &QDialog::accept);
        connect(result, &QPCSCReader::disconnected, &result->insertCardDialog, &QDialog::accept);
    } else {
        result->open();
    }
    return result;
}

// Reader
void QPCSCReader::open() {
    // Start the thread
    thread.start();
    worker.moveToThread(&thread);

    // control signals
    connect(this, &QPCSCReader::connectCard, &worker, &QPCSCReaderWorker::connectCard, Qt::QueuedConnection);
    connect(this, &QPCSCReader::disconnectCard, &worker, &QPCSCReaderWorker::disconnectCard, Qt::QueuedConnection);
    connect(this, &QPCSCReader::transmitBytes, &worker, &QPCSCReaderWorker::transmit, Qt::QueuedConnection);

    // proxy signals
    connect(&worker, &QPCSCReaderWorker::disconnected, this, &QPCSCReader::disconnected, Qt::QueuedConnection);
    connect(&worker, &QPCSCReaderWorker::connected, this, &QPCSCReader::connected, Qt::QueuedConnection);
    connect(&worker, &QPCSCReaderWorker::received, this, &QPCSCReader::received, Qt::QueuedConnection);

    // Open the "in use"" dialog. Separate slot because lambda would be in the worker thread.
    connect(&worker, &QPCSCReaderWorker::connected, this, &QPCSCReader::showDialog, Qt::QueuedConnection);

    // connect in thread
    emit connectCard(reader, protocol);
}

void QPCSCReader::showDialog() {
    isOpen = true;
    inUseDialog.showIt(static_cast<WebContext *>(parent())->friendlyOrigin(), reader); // XXX
    // UX XXX: communicate SCARD_E_CANCELLED to the process if the next signal is triggered
    // Disconnect the reader by canceling dialog
    connect(&inUseDialog, &QDialog::rejected, &worker, &QPCSCReaderWorker::disconnectCard, Qt::QueuedConnection);
    // And close the dialog if reader is disconnected
    connect(&worker, &QPCSCReaderWorker::disconnected, &inUseDialog, &QDialog::accept, Qt::QueuedConnection);
}

void QPCSCReader::cardInserted(const QString &reader, const QByteArray &atr) {
    if ((this->reader == reader) && (!atr.isEmpty())) {
        open();
    }
}

void QPCSCReader::readerRemoved(const QString &reader) {
    if (this->reader == reader) {
        disconnect();
    }
}

void QPCSCReader::disconnect() {
    // If not yet connected, the slot is activated by
    // insert card dialog or reader removal during that dialog
    if (!isOpen) {
        return emit(disconnected(SCARD_E_CANCELLED));
    }
    emit disconnectCard();
}

void QPCSCReader::transmit(const QByteArray &apdu) {
    emit transmitBytes(apdu);
}

// Worker
QPCSCReaderWorker::~QPCSCReaderWorker() {
    if (card) {
        SCard(Disconnect, card, SCARD_LEAVE_CARD);
    }
    // pcsc-lite requirements
    if (context) {
        SCard(ReleaseContext, context);
    }
}

// FIXME: have the "connect" function as part of QtPCSC so that access to existing context
// would be there. Hide the QPCSCReader constructor
// Handle the empty reader + UI case
void QPCSCReaderWorker::connectCard(const QString &reader, const QString &protocol) {
    LONG rv = SCARD_S_SUCCESS;

    // Context per thread, required by pcsc-lite
    rv = SCard(EstablishContext, SCARD_SCOPE_USER, nullptr, nullptr, &context);
    if (rv != SCARD_S_SUCCESS) {
        return emit disconnected(rv);
    }

    // protocol
    DWORD proto = SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1;
    if (protocol == "T=0") {
        proto = SCARD_PROTOCOL_T0;
    } else if (protocol == "T=1") {
        proto = SCARD_PROTOCOL_T1;
    } else if (protocol == "*") {
        // do nothing, default
    } else {
        return emit disconnected(SCARD_E_INVALID_PARAMETER);
    }

    // By default we want to have a reliable connection, which means exclusive connection
    DWORD mode = SCARD_SHARE_EXCLUSIVE;
    // Try to connect multiple times, a freshly inserted card is often probed by other software as well
    rv = SCard(Connect, context, reader.toLatin1().data(), mode, proto, &card, &this->protocol);
    if (rv == LONG(SCARD_E_SHARING_VIOLATION)) {
#ifndef Q_OS_WIN
        // On Unix, we are happy with a shared connection + transaction
        mode = SCARD_SHARE_SHARED;
        rv = SCard(Connect, context, reader.toLatin1().data(), mode, proto, &card, &this->protocol);
#else
        // On Windows we need to have a exclusive connection to defeat the 5sec rule
        // Try several times before giving up
        int i = 0;
        qsrand((uint)QTime::currentTime().msec());
        do {
            rv = SCard(Connect, context, reader.toLatin1().data(), mode, proto, &card, &this->protocol);
            int ms = (qrand() % 500) + 100;
            _log("Sleeping for %d", ms);
            QTime a = QTime::currentTime();
            QThread::currentThread()->msleep(ms);
            _log("Slept %d", a.msecsTo(QTime::currentTime()));
            i++;
        } while ((i < 10) && (rv == LONG(SCARD_E_SHARING_VIOLATION)));
#endif
    }

    // Check
    if (rv != SCARD_S_SUCCESS) {
        return emit disconnected(rv);
    }

    // Get fresh information
    QByteArray tmpname(reader.toLatin1().size() + 2, 0); // XXX: Windows requires 2, for the extra \0 ?
    DWORD tmplen = tmpname.size();
    DWORD tmpstate = 0;
    DWORD tmpproto = 0;
    QByteArray atr(33, 0);
    DWORD atrlen = atr.size();
    rv = SCard(Status, card, tmpname.data(), &tmplen, &tmpstate, &tmpproto, (unsigned char *) atr.data(), &atrlen);

    if (rv != SCARD_S_SUCCESS) {
        return emit disconnected(rv);
    }

    atr.resize(atrlen);
    tmpname.resize(tmplen);
    _log("Current state of %s: %s, protocol %d, atr %s", tmpname.data(), qPrintable(readerStateNames(tmpstate).join(",")), tmpproto, qPrintable(atr.toHex()));

#ifndef Q_OS_WIN
    // Transactions on non-windows machines
    rv = SCard(BeginTransaction, card);
    if (rv != SCARD_S_SUCCESS) {
        return emit disconnected(rv);
    }
#endif

    _log("Connected to %s in %s mode, protocol %s", qPrintable(reader), mode == SCARD_SHARE_EXCLUSIVE ? "exclusive" : "shared", this->protocol == SCARD_PROTOCOL_T0 ? "T=0" : "T=1");
    emit connected(atr, this->protocol == SCARD_PROTOCOL_T0 ? "T=0" : "T=1");
}

void QPCSCReaderWorker::disconnectCard() {
    LONG rv = SCARD_S_SUCCESS;
    if (card) {
#ifndef Q_OS_WIN
        // No transactions on Windows due to the "5 second rule"
        SCard(EndTransaction, card, SCARD_LEAVE_CARD);
#endif
        rv = SCard(Disconnect, card, SCARD_RESET_CARD);
        card = 0;
    }
    emit disconnected(rv);
}

void QPCSCReaderWorker::transmit(const QByteArray &apdu) {
    SCARD_IO_REQUEST req;
    std::vector<unsigned char> response(4096, 0); // Should be enough
    req.dwProtocol = protocol;
    req.cbPciLength = sizeof(req);
    DWORD rlen = response.size();
    LONG err = SCard(Transmit, card, &req, (const unsigned char *)apdu.data(), DWORD(apdu.size()), &req, response.data(), &rlen);
    if (err != SCARD_S_SUCCESS) {
        response.resize(0);
        SCard(Disconnect, card, SCARD_RESET_CARD);
        card = 0;
        return emit disconnected(err);
    }
    response.resize(rlen);
    emit received(QByteArray((const char*)response.data(), int(response.size())));
}
