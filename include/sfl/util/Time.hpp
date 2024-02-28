#pragma once

#include <ctime>
#include <string>

namespace sfl
{

std::string stringify(time_t t, const std::string& str)
{
    tm* _t = gmtime(&t);
    char c[1024];
    strftime(c, 1024, str.c_str(), _t);
    return std::string(c);
}

}