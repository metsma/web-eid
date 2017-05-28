/*
 * Copyright (C) 2017 Martin Paljak
 */

#include "Logger.h"

#include <QStandardPaths>
#include <QDir>
#include <QFile>

#include <cstdio>
#include <string>
#ifndef _WIN32
#include <unistd.h>
#include <pthread.h>
#include <iostream>
#else
#include <time.h>
#endif

static QString logfile = "web-eid.log";

static void printCurrentDateTime(FILE *log) {
    time_t now = time(0);
    tm *ltm = localtime(&now);
    //Date format yyyy-MM-dd hh:mm:ss
    fprintf(log, "%i-%.2i-%.2i %.2i:%.2i:%.2i ",
            1900 + ltm->tm_year,
            ltm->tm_mon + 1,
            ltm->tm_mday,
            ltm->tm_hour,
            ltm->tm_min,
            ltm->tm_sec);
}

QString Logger::getLogFilePath() {
    return QDir(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)).filePath(logfile);
}

static bool logFileExists() {
    return QFile(Logger::getLogFilePath()).exists();
}

void Logger::setFile(const QString &name) {
    logfile = name;
}

bool Logger::isEnabled() {
    return logFileExists();
}
 
void Logger::writeLog(const char *functionName, const char *fileName, int lineNumber, const char *message, ...) {
    if (!logFileExists()) {
            return;
    }
    FILE *log = fopen(getLogFilePath().toStdString().c_str(), "a");
    if (!log) {
        return;
    }
    printCurrentDateTime(log);
#ifndef _WIN32
    fprintf(log, "[%i %lu] ", getpid(), pthread_self());
#endif
    fprintf(log, "%s() [%s:%i] ", functionName, fileName, lineNumber);
    va_list args;
    va_start(args, message);
    vfprintf(log, message, args);
    va_end(args);
    fprintf(log, "\n");
    fflush(log);
    fclose(log);
}
