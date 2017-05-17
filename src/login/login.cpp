/*
 * Copyright (C) 2017 Martin Paljak
 */

// TODO: use plain execve
#include <QProcess>

int main()
{
    QProcess::startDetached("/usr/bin/open", { "/Applications/Web eID.app" });
    return 0;
}
