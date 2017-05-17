/*
 * Copyright (C) 2017 Martin Paljak
 */

#include "Logger.h"
#include <cstdio>
#include <string>
#ifndef _WIN32
#include <unistd.h>
#include <pthread.h>
#include <iostream>
#else
#include <time.h>
#endif

using namespace std;

static bool output = false;
static string logfile = "web-eid.log";

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

static string getLogFilePath() {
#ifdef _WIN32
    return string(getenv("TEMP"))+ "\\" + logfile;
#else
    return string(getenv("HOME"))+ "/tmp/" + logfile;
#endif
}

static bool logFileExist() {
    FILE *file = fopen(getLogFilePath().c_str(), "r");
    if (!file) {
        return false;
    }
    fclose(file);
    return true;
}

void Logger::setOutput(const bool value) {
    output = value;
}

void Logger::setFile(const string &name) {
    logfile = name;
}

void Logger::writeLog(const char *functionName, const char *fileName, int lineNumber, const char *message, ...) {
    FILE *log = stdout;
    if (!output) {
        if (!logFileExist()) {
            return;
        }
        log = fopen(getLogFilePath().c_str(), "a");
        if (!log) {
            return;
        }
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
    if (!output) {
        fclose(log);
    }
}
