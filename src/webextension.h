/*
 * Copyright (C) 2017 Martin Paljak
 */

#pragma once
#include <QStringList>

// Check native messaging registration
namespace WebExtensionHelper {
bool isEnabled();
bool setEnabled(bool enabled);

static QString nativeName = "com.web_eid.app";
static QStringList chromeOrigins = {
    "chrome-extension://ckjefchnfjhjfedoccjbhjpbncimppeg/",
    "chrome-extension://fmpfihjoladdfajbnkdfocnbcehjpogi/",
    "chrome-extension://dpaadmibfooopdmommnjgmpkgdeeoppb/"
};
static QStringList firefoxExtensions = {
    "native@hwcrypto.org",
    "app@web-eid.com",
    "{75323b42-f502-11e6-9fd0-6cf049ee125a}"
};
};

