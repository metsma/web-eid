#include "nm-proxy.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>

#ifdef _WIN32
#include <Windows.h>
// for setting stdio mode
#include <fcntl.h>
#include <io.h>
#endif


int main(int argc, char *argv[])
{
    const char *msg = "This is not a regular program, it is expected to be run from a browser.\n";
    // Check if run as a browser extension
    if (argc < 2) {
        printf(msg);
        exit(1);
    }
    std::string arg1(argv[1]);
    if (arg1.find("chrome-extension://") == 0) {
        // Chrome extension
    } else if (QFile::exists(QString::fromStdString(arg1))) {
        // printf("Probably Firefox\n");
    }
    // Check that input is a pipe (the app is not run from command line)
    bool isPipe = false;
#ifdef _WIN32
    isPipe = GetFileType(GetStdHandle(STD_INPUT_HANDLE)) == FILE_TYPE_PIPE;
#else
    struct stat sb;
    if (fstat(fileno(stdin), &sb) != 0) {
        exit(1);
    }
    isPipe = S_ISFIFO(sb.st_mode);
#endif
    if (!isPipe) {
        printf(msg);
// FIXME        exit(1);
    }
#ifdef _WIN32
    // Set files to binary mode, to be able to read the uint32 msg size
    _setmode(_fileno(stdin), O_BINARY);
    _setmode(_fileno(stdout), O_BINARY);
#endif
    return NMProxy(argc, argv).exec();
}
