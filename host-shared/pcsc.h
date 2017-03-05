#include <string>
#include <vector>

#ifdef __APPLE__
#include <PCSC/winscard.h>
#include <PCSC/wintypes.h>
#else
#undef UNICODE
#include <winscard.h>
#endif

class PCSC {
public:
    static std::vector<std::vector<unsigned char>> atrList(bool all);
};
