/*
 * Copyright (C) 2017 Martin Paljak
 */

#pragma once

#include <cstdarg>
#include <QString>

namespace Logger {
    void writeLog(const char *functionName, const char *fileName, int lineNumber, const char *message, ...);
    void setFile(const QString &name);
    bool isEnabled();
    QString getLogFilePath();
}

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define _log(...) Logger::writeLog(__func__, __FILE__, __LINE__, __VA_ARGS__)
#else
#define _log(...) Logger::writeLog(__FUNCTION__, __FILE__, __LINE__, __VA_ARGS__)
#endif
