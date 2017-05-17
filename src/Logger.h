/*
 * Copyright (C) 2017 Martin Paljak
 */

#pragma once

#include <cstdarg>
#include <string>

namespace Logger {
void writeLog(const char *functionName, const char *fileName, int lineNumber, const char *message, ...);
void setOutput(const bool value);
void setFile(const std::string &name);
}

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define _log(...) Logger::writeLog(__func__, __FILE__, __LINE__, __VA_ARGS__)
#else
#define _log(...) Logger::writeLog(__FUNCTION__, __FILE__, __LINE__, __VA_ARGS__)
#endif
