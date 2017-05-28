/*
 * Copyright (C) 2017 Martin Paljak
 */

#include "webextension.h"
#include <QFile>
#include "Logger.h"

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

bool WebExtensionHelper::isEnabled() {
    bool enabled = false;
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
    bool chrome = false;
    bool chromium = false;
    bool firefox = false;
    QString nmpath = QDir(QCoreApplication::applicationDirPath()).filePath("web-eid-bridge");

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

    // FIXME: reduce code amount if single json has been verified
    QString nmpath = QDir::toNativeSeparators(QDir(QCoreApplication::applicationDirPath()).filePath("web-eid-bridge.exe"));

    QSettings chrome("HKEY_CURRENT_USER\\SOFTWARE\\Google\\Chrome\\NativeMessagingHosts", QSettings::NativeFormat);
    QString jsonFile = chrome.value(nativeName).toString();
    QFile chromeManifest(jsonFile);
    if (chromeManifest.exists()) {
        auto manifest = readManifest(chromeManifest);
        if (manifest["name"] == nativeName && manifest["path"] == nmpath) {
            chrome = true;
        }
    }

    // FIXME: the WOW thing
    QSettings firefox("HKEY_CURRENT_USER\\SOFTWARE\\Mozilla\\NativeMessagingHosts", QSettings::NativeFormat);
    QString firefoxJsonFile = firefox.value(nativeName).toString();
    QFile firefoxManifest(firefoxJsonFile);
    if (firefoxManifest.exists()) {
        auto manifest = readManifest(firefoxManifest);
        if (manifest["name"] == nativeName && manifest["path"] == nmpath) {
            firefox = true;
        }
    }

    enabled = chrome && firefox;
#else
#error "Unsupported platform"
#endif
    return enabled;
}

bool WebExtensionHelper::setEnabled(bool enabled) {
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
    QString nmpath = QDir(QCoreApplication::applicationDirPath()).filePath("web-eid-bridge");

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
    QSettings chrome("HKEY_CURRENT_USER\\SOFTWARE\\Google\\Chrome\\NativeMessagingHosts", QSettings::NativeFormat);
    QSettings firefox("HKEY_CURRENT_USER\\SOFTWARE\\Mozilla\\NativeMessagingHosts", QSettings::NativeFormat);

    QString nmpath = QDir::toNativeSeparators(QDir(QCoreApplication::applicationDirPath()).filePath("web-eid-bridge.exe"));
    QString jsonPath = QDir(QCoreApplication::applicationDirPath()).filePath(nativeName + ".json");

    if (enabled) {
        // Create unified JSON file
        QFile jsonFile(jsonPath);
        jsonFile.open(QIODevice::WriteOnly | QIODevice::Text);
        jsonFile.write(getManifestJSON(nativeName, nmpath, chromeOrigins, firefoxExtensions));
        jsonFile.flush();
        // Set in registry.
        chrome.setValue(nativeName, QDir::toNativeSeparators(jsonPath));
        firefox.setValue(nativeName, QDir::toNativeSeparators(jsonPath));
    } else {
        chrome.remove(nativeName);
        firefox.remove(nativeName);
        // Do not touch the json File, it is harmless
    }
#else
#error "Unsupported platform"
#endif

    return true;
}
