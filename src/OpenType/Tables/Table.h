#pragma once

#include "OpenType/Defines.h"

class Table {
public:
    u32 checksum;

    virtual ~Table() = default;
    virtual auto read(std::ifstream& file) -> bool = 0;
};
