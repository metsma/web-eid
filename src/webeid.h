/*
 * Copyright (C) 2017 Martin Paljak
 */

#pragma once

// TODO: move to signer interface
#define BINARY_SHA224_LENGTH 28
#define BINARY_SHA256_LENGTH 32
#define BINARY_SHA384_LENGTH 48
#define BINARY_SHA512_LENGTH 64

enum CertificatePurpose {
    UnknownPurpose = 0,
    Authentication = 1 << 1,
    Signing = 1 << 2
};
