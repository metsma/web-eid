/*
 * Copyright (C) 2017 Martin Paljak
 */

#include <string>
#include <vector>

class P11Modules {
public:
    static std::vector<std::string> getPaths(const std::vector<std::vector<unsigned char> > &atrs);
};
