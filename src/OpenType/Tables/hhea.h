#pragma once

#include <arpa/inet.h>
#include <cassert>
#include <fstream>

#include "OpenType/Defines.h"
#include "OpenType/Tables/Table.h"

class HorizontalHeader : public Table {
public:
    static constexpr TableTag g_identifier { 'h', 'h', 'e', 'a' };

private:
    u16 majorVersion {};
    u16 minorVersion {};

    i16 ascender {};
    i16 descender {};
    i16 lineGap {};
    u16 advanceWidthMax {};
    i16 minLeftSideBearing {};
    i16 minRightSideBearing {};
    i16 xMaxExtent {};

    i16 caretSlopeRise {};
    i16 caretSlopeRun {};
    i16 caretOffset {};

    u64 reserved {};
    i16 metricDataFormat {};
    u16 numberOfHMetrics {};

public:
    [[nodiscard]] auto size() const -> u16
    {
        return numberOfHMetrics;
    }

    virtual auto read(std::ifstream& file) -> bool override
    {
        static constexpr auto fields = std::make_tuple(
            &HorizontalHeader::majorVersion,
            &HorizontalHeader::minorVersion,
            &HorizontalHeader::ascender,
            &HorizontalHeader::descender,
            &HorizontalHeader::lineGap,
            &HorizontalHeader::advanceWidthMax,
            &HorizontalHeader::minLeftSideBearing,
            &HorizontalHeader::minRightSideBearing,
            &HorizontalHeader::xMaxExtent,
            &HorizontalHeader::caretSlopeRise,
            &HorizontalHeader::caretSlopeRun,
            &HorizontalHeader::caretOffset,
            &HorizontalHeader::reserved,
            &HorizontalHeader::metricDataFormat,
            &HorizontalHeader::numberOfHMetrics);

        std::apply([&](auto&&... members) {
            ([&](auto member) {
                using MemberType = decltype(this->*member);
                file.read(reinterpret_cast<char*>(&(this->*member)), sizeof(this->*member));

                if constexpr (sizeof(MemberType) == 2) {
                    this->*member = ntohs(this->*member);
                } else if constexpr (sizeof(MemberType) == 4) {
                    this->*member = ntohl(this->*member);
                } else if constexpr (sizeof(MemberType) == 8) {
                    this->*member = be64toh(this->*member);
                } else {
                    ASSERT_NOT_REACHED;
                }
            }(members),
             ...);
        },
                   fields);

        return true;
    }

    [[nodiscard]] auto to_string() const noexcept -> std::string
    {
        return std::format("HorizontalHeader(version: {}.{}, ascender: {}, descender: {}, lineGap: {}, advanceWidthMax: {}, minBearing: {}/{}, xMaxExtent: {}, caret: {}/{}/{}, metricDataFormat: {}, numberOfHMetrics: {})",
                           majorVersion,
                           minorVersion,
                           ascender,
                           descender,
                           lineGap,
                           advanceWidthMax,
                           minLeftSideBearing,
                           minRightSideBearing,
                           xMaxExtent,
                           caretSlopeRise,
                           caretSlopeRun,
                           caretOffset,
                           metricDataFormat,
                           numberOfHMetrics);
    }
};
