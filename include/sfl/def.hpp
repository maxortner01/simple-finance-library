#pragma once

#include <iostream>
#include <sstream>

#include <algorithm>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <unordered_map>
#include <functional>
#include <span>

#include "util/Factory.hpp"

namespace sfl
{
    #define FILE_VERSION 1
    using index_type = uint32_t;
    using id_t = util::id_t;
}