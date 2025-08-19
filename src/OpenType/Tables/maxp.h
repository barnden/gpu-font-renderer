#pragma once

#include <arpa/inet.h>
#include <fstream>

#include "OpenType/Defines.h"
#include "OpenType/Tables/Table.h"

class MaximumProfile : public Table {
    // https://learn.microsoft.com/en-us/typography/opentype/spec/maxp

public:
    static constexpr TableTag g_identifier { 'm', 'a', 'x', 'p' };

private:
    u32 version;
    u16 numGlyphs;

    u16 maxPoints;
    u16 maxContours;
    u16 maxCompositePoints;
    u16 maxCompositeContours;
    u16 maxZones;
    u16 maxTwilightPoints;
    u16 maxStorage;
    u16 maxFunctionDefs;
    u16 maxInstructionDefs;
    u16 maxStackElements;
    u16 maxSizeOfInstructions;
    u16 maxComponentElements;
    u16 maxComponentDepth;

    friend class OpenType;

public:
    virtual auto read(std::ifstream& file) -> bool override
    {
        file.read(reinterpret_cast<char*>(&version), sizeof(u32));
        file.read(reinterpret_cast<char*>(&numGlyphs), sizeof(u16));

        version = ntohl(version);
        numGlyphs = ntohs(numGlyphs);

        // As of 2021 only versions 0.5 and 1.0 are defined in `maxp` spec.
        if (version == 0x00005000)
            return true;

        if (version != 0x00010000)
            return false;

        for (auto&& member : {
                 &maxPoints,
                 &maxContours,
                 &maxCompositePoints,
                 &maxCompositeContours,
                 &maxZones,
                 &maxTwilightPoints,
                 &maxStorage,
                 &maxFunctionDefs,
                 &maxInstructionDefs,
                 &maxStackElements,
                 &maxSizeOfInstructions,
                 &maxComponentElements,
                 &maxComponentDepth }) {
            file.read(reinterpret_cast<char*>(member), sizeof(u16));
            *member = ntohs(*member);
        }

        return true;
    }

    [[nodiscard]] auto to_string() const noexcept -> std::string
    {
        return std::format("MaximumProfile(version: {}.{}, numGlyphs: 0x{:04X})",
                           version >> 16,
                           version & 0xFFFF,
                           numGlyphs);
    }
};
