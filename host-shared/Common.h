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
