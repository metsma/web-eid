/*
 * Copyright (C) 2017 Martin Paljak
 */

#include <QStringList>
#include <QByteArray>

class CardOracle {
public:
    static QStringList atrOracle(const QByteArray &atr);
    static bool isUsable(const QString &token);
};
