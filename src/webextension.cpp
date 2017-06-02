/*
 * Copyright (C) 2017 Martin Paljak
 */

#include "webextension.h"
#include <QFile>
#include "debuglog.h"

#ifdef Q_OS_MACOS
#include <CoreFoundation/CoreFoundation.h>
#include <QUrl>
#endif

#include <QDir>
#include <QSettings>
#include <QCoreApplication>
#include <QVariantMap>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSysInfo>

enum Browser {Chrome, Chromium, Firefox};

static QString getManifestPath(const Browser browser) {
    QString path;
#if defined(Q_OS_MACOS)
    switch(browser) {
    case Chrome:
        path = QDir::home().absoluteFilePath("Library/Application Support/Google/Chrome/NativeMessagingHosts");
        break;
    case Chromium:
        path = QDir::home().absoluteFilePath("Library/Application Support/Chromium/NativeMessagingHosts");
        break;
    case Firefox:
        path = QDir::home().absoluteFilePath("Library/Application Support/Mozilla/NativeMessagingHosts");
        break;
    }
#elif defined(Q_OS_LINUX)
    switch(browser) {
    case Chrome:
        path = QDir::home().absoluteFilePath(".config/google-chrome/NativeMessagingHosts");
        break;
    case Chromium:
        path = QDir::home().absoluteFilePath(".config/chromium/NativeMessagingHosts");
        break;
    case Firefox:
        path = QDir::home().absoluteFilePath(".mozilla/native-messaging-hosts");
        break;
    }
#elif defined(Q_OS_WIN)
#else
#error "Unsupported platform"
#endif

    _log("Manifest path: %s", qPrintable(path));
    return path;
}

static QString getManifestFile(const Browser browser) {
    // FIXME: Can use same fiel on Windows?
    return QDir(getManifestPath(browser)).filePath(WebExtensionHelper::nativeName + ".json");
}

static QByteArray getManifestJSON(const QString &name, const QString &path, const QStringList &origins, const QStringList &extensions) {
    QVariantMap json = {
        {"name", name},
        {"description", "Use your eID on the Web"},
        {"path", path},
        {"type", "stdio"}
    };
    if (!origins.isEmpty()) {
        json["allowed_origins"] = origins;
    } else if (!extensions.isEmpty()) {
        json["allowed_extensions"] = extensions;
    }
    return QJsonDocument::fromVariant(json).toJson();
}

static QJsonObject readManifest(QFile &manifest) {
    manifest.open(QIODevice::ReadOnly | QIODevice::Text);
    QByteArray json = manifest.readAll();
    manifest.close();
    return QJsonDocument::fromJson(json).object();
}

static QString getBridgePath() {
    QString path;
#if defined(Q_OS_MACOS)
    path = QDir(QCoreApplication::applicationDirPath()).filePath("web-eid-bridge");
#elif defined(Q_OS_LINUX)
    _log("Product type == %s", qPrintable(QSysInfo::productType()));
    if(QSysInfo::productType() == "debian")
        path = "/usr/lib/web-eid-bridge";
    else if (QSysInfo::productType() == "fedora") {
        path = "/usr/lib64/web-eid-bridge";
    } else {
        path = "/usr/lib/web-eid-bridge";
    }
#elif defined(Q_OS_WIN)
    path = QDir::toNativeSeparators(QDir(QCoreApplication::applicationDirPath()).filePath("web-eid-bridge.exe"));
#else
#error "Unsupported platform"
#endif
    return path;
}

bool WebExtensionHelper::isEnabled() {
    bool enabled = false;
    QString nmpath = getBridgePath();

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
    bool chrome = false;
    bool chromium = false;
    bool firefox = false;

    QFile chromeManifest(getManifestFile(Chrome));
    if (chromeManifest.exists()) {
        auto manifest = readManifest(chromeManifest);
        if (manifest["name"] == nativeName && manifest["path"] == nmpath) {
            chrome = true;
        }
    }
    QFile chromiumManifest(getManifestFile(Chromium));
    if (chromiumManifest.exists()) {
        auto manifest = readManifest(chromiumManifest);
        if (manifest["name"] == nativeName && manifest["path"] == nmpath) {
            chromium = true;
        }
    }
    QFile firefoxManifest(getManifestFile(Firefox));
    if (firefoxManifest.exists()) {
        auto manifest = readManifest(firefoxManifest);
        if (manifest["name"] == nativeName && manifest["path"] == nmpath) {
            firefox = true;
        }
    }
    enabled = chrome && chromium && firefox;
#elif defined(Q_OS_WIN)
    bool chrome = false;
    bool firefox = false;
    bool firefox64 = false;

    QSettings chromeReg("HKEY_CURRENT_USER\\SOFTWARE\\Google\\Chrome\\NativeMessagingHosts\\" + nativeName, QSettings::NativeFormat);
    QString jsonFile = chromeReg.value("Default").toString();
    QFile chromeManifest(jsonFile);
    if (chromeManifest.exists()) {
        auto manifest = readManifest(chromeManifest);
        if (manifest["name"] == nativeName && manifest["path"] == nmpath) {
            chrome = true;
        }
    }

    // FIXME: the WOW thing
    QSettings firefoxReg("HKEY_CURRENT_USER\\SOFTWARE\\Mozilla\\NativeMessagingHosts\\" + nativeName, QSettings::NativeFormat);
    QString firefoxJsonFile = firefoxReg.value("Default").toString();
    QFile firefoxManifest(firefoxJsonFile);
    if (firefoxManifest.exists()) {
        auto manifest = readManifest(firefoxManifest);
        if (manifest["name"] == nativeName && manifest["path"] == nmpath) {
            firefox = true;
        }
    }

    QSettings firefox64Reg("HKEY_CURRENT_USER\\SOFTWARE\\Mozilla\\NativeMessagingHosts\\" + nativeName, QSettings::Registry64Format);
    QString firefox64JsonFile = firefox64Reg.value("Default").toString();
    QFile firefox64Manifest(firefox64JsonFile);
    if (firefox64Manifest.exists()) {
        auto manifest = readManifest(firefox64Manifest);
        if (manifest["name"] == nativeName && manifest["path"] == nmpath) {
            firefox64 = true;
        }
    }

    enabled = chrome && firefox && firefox64;
#else
#error "Unsupported platform"
#endif
    return enabled;
}

bool WebExtensionHelper::setEnabled(bool enabled) {
    QString nmpath = getBridgePath();
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)

    // All three browsers
    if (enabled) {
        // Write manifests
        QDir::root().mkpath(getManifestPath(Chrome));
        QFile chrome(getManifestFile(Chrome));
        chrome.open(QIODevice::WriteOnly | QIODevice::Text);
        chrome.write(getManifestJSON(nativeName, nmpath, chromeOrigins, {}));
        chrome.flush();

        QDir::root().mkpath(getManifestPath(Chromium));
        QFile chromium(getManifestFile(Chromium));
        chromium.open(QIODevice::WriteOnly | QIODevice::Text);
        chromium.write(getManifestJSON(nativeName, nmpath, chromeOrigins, {}));
        chromium.flush();

        QDir::root().mkpath(getManifestPath(Firefox));
        QFile firefox(getManifestFile(Firefox));
        firefox.open(QIODevice::WriteOnly | QIODevice::Text);
        firefox.write(getManifestJSON(nativeName, nmpath, {}, firefoxExtensions));
        firefox.flush();
    } else {
        // Delete manifests
        QFile chrome(getManifestFile(Chrome));
        if (chrome.exists()) {
            chrome.remove();
        }
        QFile chromium(getManifestFile(Chromium));
        if (chromium.exists()) {
            chromium.remove();
        }
        QFile firefox(getManifestFile(Firefox));
        if (firefox.exists()) {
            firefox.remove();
        }
    }
#elif defined(Q_OS_WIN32)
    // Add registry entry.
    QSettings chrome("HKEY_CURRENT_USER\\SOFTWARE\\Google\\Chrome\\NativeMessagingHosts\\" + nativeName, QSettings::NativeFormat);
    QSettings firefox("HKEY_CURRENT_USER\\SOFTWARE\\Mozilla\\NativeMessagingHosts\\" + nativeName, QSettings::NativeFormat);
    QSettings firefox64("HKEY_CURRENT_USER\\SOFTWARE\\Mozilla\\NativeMessagingHosts\\" + nativeName, QSettings::Registry64Format);

    QString jsonPath = QDir(QCoreApplication::applicationDirPath()).filePath(nativeName + ".json");

    if (enabled) {
        // Create unified JSON file
        QFile jsonFile(jsonPath);
        jsonFile.open(QIODevice::WriteOnly | QIODevice::Text);
        jsonFile.write(getManifestJSON(nativeName, nmpath, chromeOrigins, firefoxExtensions));
        jsonFile.flush();
        // Set in registry.
        chrome.setValue("Default", QDir::toNativeSeparators(jsonPath));
        firefox.setValue("Default", QDir::toNativeSeparators(jsonPath));
        firefox64.setValue("Default", QDir::toNativeSeparators(jsonPath));
    } else {
        chrome.remove("Default");
        firefox.remove("Default");
        firefox64.remove("Default");
        // Also remove the JSON file
        QFile jsonFile(jsonPath);
        jsonFile.remove();
    }
#else
#error "Unsupported platform"
#endif

    return true;
}
