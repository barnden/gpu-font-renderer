#pragma once

#include <arpa/inet.h>
#include <cassert>
#include <fstream>
#include <print>
#include <vector>

#include "OpenType/Defines.h"
#include "OpenType/Tables/Table.h"

class HorizontalMetrics : public Table {
public:
    static constexpr TableTag g_identifier { 'h', 'm', 't', 'x' };

private:
    struct LongHorMetric {
        u16 advanceWidth {};
        i16 lsb {};

        auto read(std::ifstream& file) -> bool
        {
            file.read(reinterpret_cast<char*>(&advanceWidth), sizeof(u16));
            file.read(reinterpret_cast<char*>(&lsb), sizeof(i16));

            advanceWidth = ntohs(advanceWidth);
            lsb = ntohs(lsb);

            return true;
        }
    };

    std::vector<LongHorMetric> hMetrics;
    std::vector<i16> leftSideBearings;

public:
    HorizontalMetrics(u16 numGlyphs, u16 numberOfHMetrics)
        : hMetrics(numberOfHMetrics)
        , leftSideBearings(numGlyphs - numberOfHMetrics) { };

    virtual auto read(std::ifstream& file) -> bool override
    {
        for (auto& record : hMetrics) {
            if (!record.read(file))
                return false;
        }

        for (auto& lsb : leftSideBearings) {
            file.read(reinterpret_cast<char*>(&lsb), sizeof(i16));
            lsb = ntohs(lsb);
        }

        return true;
    }
};
