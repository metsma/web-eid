#include <QProcess>

int main()
{
    QProcess::startDetached("/usr/bin/open", { "/Applications/Web eID.app" });
    return 0;
}
