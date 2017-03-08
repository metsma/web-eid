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

#pragma once

#include <stdexcept>

// TODO: move to signer interface
#define BINARY_SHA1_LENGTH 20
#define BINARY_SHA224_LENGTH 28
#define BINARY_SHA256_LENGTH 32
#define BINARY_SHA384_LENGTH 48
#define BINARY_SHA512_LENGTH 64


// Exceptions
class UserCanceledError : public std::runtime_error {
    public:
     UserCanceledError() : std::runtime_error("User canceled"){}
};

class PinBlockedException : public std::runtime_error {
public:
    PinBlockedException() : std::runtime_error("Maximum number of PIN entry attempts has been reached") {}
};

class AuthenticationError : public std::runtime_error {
public:
    AuthenticationError() : std::runtime_error("Authentication error"){}
};

class AuthenticationBadInput : public std::runtime_error {
public:
    AuthenticationBadInput() : std::runtime_error("Authentication Bad Input"){}
};

class NoCertificatesException : public std::runtime_error {
public:
    NoCertificatesException() : std::runtime_error("Cert not found") {}
};
