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

    u16 m_num_glyphs;
    std::vector<LongHorMetric> hMetrics;
    std::vector<i16> leftSideBearings;

public:
    HorizontalMetrics(u16 num_glyphs, u16 num_h_metrics)
        : m_num_glyphs(num_glyphs)
        , hMetrics(num_h_metrics)
        , leftSideBearings()
    {
        if (num_h_metrics > num_glyphs) {
            leftSideBearings.resize(num_glyphs - num_h_metrics);
        }
    }

    [[nodiscard]] auto operator[](u16 glyphID) const -> std::optional<LongHorMetric>
    {
        if (glyphID < hMetrics.size()) {
            return hMetrics[glyphID];
        }

        if (glyphID < hMetrics.size() + leftSideBearings.size()) {
            return LongHorMetric {
                .advanceWidth = 0xFFFF,
                .lsb = leftSideBearings[glyphID - hMetrics.size()]
            };
        }

        // As an optimization, the number of records can be less than the number
        // of glyphs, in which case the advance width value of the last record
        // applies to all remaining glyph IDs.
        if (hMetrics.size() < m_num_glyphs) {
            return hMetrics.back();
        }

        return std::nullopt;
    }

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
