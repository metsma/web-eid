/*
 * Copyright (C) 2017 Martin Paljak
 */

#pragma once

#include <QString>

namespace Logger {
void writeLog(const char *functionName, const char *fileName, int lineNumber, const char *message, ...);
void setFile(const QString &name);
bool isEnabled();
QString getLogFilePath();
}

#define _log(...) Logger::writeLog(__FUNCTION__, __FILE__, __LINE__, __VA_ARGS__)
