#pragma once

#include <arpa/inet.h>
#include <bitset>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>
#include <print>
#include <stdexcept>
#include <vector>

#include "Defines.h"
#include "TypeArray.h"

class Table {
public:
    u32 checksum;

    virtual ~Table() = default;
    virtual auto read(std::ifstream& file) -> bool = 0;
};

struct Fixed {
    u32 data;

    [[nodiscard]] constexpr auto value() const noexcept -> float
    {
        return static_cast<float>(data >> 16) + (data & 0xFFFF) / static_cast<float>(0xFFFF);
    }
};

class Head : public Table {
    // https://learn.microsoft.com/en-us/typography/opentype/spec/head
public:
    static constexpr TableTag g_identifier { 'h', 'e', 'a', 'd' };

private:
    u16 majorVersion;
    u16 minorVersion;

    Fixed fontRevision;

    u32 checksumAdjustment;
    u32 magicNumber;

    /**
     *  FIXME: Ideally this should be a bitset but...
     *         the spec doesn't give names to the individual bits
     *         and I am too lazy to give them names.
     */

    u16 flags;
    u16 unitsPerEm;

    i64 created;
    i64 modified;

    i16 xMin;
    i16 yMin;
    i16 xMax;
    i16 yMax;

    u16 macStyle;
    u16 lowestRecPPEM;
    i16 fontDirectionHint;
    i16 indexToLocFormat;
    i16 glyphDataFormat;

    friend class OpenType;

public:
    virtual auto read(std::ifstream& file) -> bool override
    {
        file.read(reinterpret_cast<char*>(&majorVersion), sizeof(u16));
        majorVersion = ntohs(majorVersion);

        file.read(reinterpret_cast<char*>(&minorVersion), sizeof(u16));
        minorVersion = ntohs(minorVersion);

        file.read(reinterpret_cast<char*>(&fontRevision.data), sizeof(u32));
        fontRevision.data = ntohl(fontRevision.data);

        file.read(reinterpret_cast<char*>(&checksumAdjustment), sizeof(u32));
        checksumAdjustment = ntohl(checksumAdjustment);

        file.read(reinterpret_cast<char*>(&magicNumber), sizeof(u32));
        magicNumber = ntohl(magicNumber);

        if (magicNumber != 0x5F0F3CF5) {
            std::println("Supplied magic number 0x{:08X} does not match expected value.", magicNumber);

            return false;
        }

        // Reflection can't come fast enough
        constexpr auto fields = std::make_tuple(
            &Head::flags,
            &Head::unitsPerEm,
            &Head::created,
            &Head::modified,
            &Head::xMin,
            &Head::yMin,
            &Head::xMax,
            &Head::yMax,
            &Head::macStyle,
            &Head::lowestRecPPEM,
            &Head::fontDirectionHint,
            &Head::indexToLocFormat,
            &Head::glyphDataFormat);

        std::apply([&](auto&&... members) {
            ([&](auto member) {
                using MemberType = decltype(this->*member);
                file.read(reinterpret_cast<char*>(&(this->*member)), sizeof(MemberType));

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
        return std::format("Head(version: {}.{}, fontRevision: {}, indexToLocFormat: {})",
                           majorVersion,
                           minorVersion,
                           fontRevision.value(),
                           indexToLocFormat);
    }
};

class IndexToLocation : public Table {
    // https://learn.microsoft.com/en-us/typography/opentype/spec/loca
public:
    static constexpr TableTag g_identifier { 'l', 'o', 'c', 'a' };

private:
    size_t m_stride;
    size_t m_size;
    u32 m_numGlyphs;
    std::shared_ptr<char[]> m_data;

public:
    IndexToLocation(size_t stride, u32 numGlyphs)
        : m_stride(stride)
        , m_size(numGlyphs + 1)
        , m_data(std::make_shared<char[]>(m_size * stride)) { };

    virtual auto read(std::ifstream& file) -> bool override
    {
        for (auto i = 0uz; i < m_size; i++) {
            // FIXME: DRY and don't do this comparison every iteration
            if (m_stride == 16) {
                u16* data = reinterpret_cast<u16*>(m_data.get());
                file.read(reinterpret_cast<char*>(&data[i]), sizeof(u16));
                *data = ntohs(*data);
            } else {
                u32* data = reinterpret_cast<u32*>(m_data.get());
                file.read(reinterpret_cast<char*>(&data[i]), sizeof(u32));
                *data = ntohs(*data);
            }
        }

        return true;
    }

    [[nodiscard]] auto operator[](size_t idx) const -> u32
    {
        /**
         * Offsets must be in ascending order, with loca[n] <= loca[n+1].
         * The length of each glyph description is determined by the difference
         * between two consecutive entries: loca[n+1] - loca[n]. To compute the
         * length of the last glyph description, there is an extra entry in the
         * offsets array after the entry for the last valid glyph ID. Thus, the
         * number of elements in the offsets array is numGlyphs + 1.
         */
        if (idx > size())
            throw std::runtime_error("OOB access in IndexToLocation");

        if (m_stride == 16) {
            // The local offset divided by 2 is stored
            return 2 * reinterpret_cast<u16*>(m_data.get())[idx];
        } else {
            // The actual local offset is stored.
            return reinterpret_cast<u32*>(m_data.get())[idx];
        }
    }

    [[nodiscard]] constexpr auto size() const -> size_t
    {
        return m_size;
    }

    [[nodiscard]] auto to_string() const noexcept -> std::string
    {
        return std::format("IndexToLocation(stride: {}, size: {})",
                           m_stride,
                           m_size);
    }
};

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

class GlyphData : public Table {
public:
    static constexpr TableTag g_identifier { 'g', 'l', 'y', 'f' };

private:
    class GlyphHeader : public Table {
        i16 numberOfContours;
        i16 xMin;
        i16 yMin;
        i16 xMax;
        i16 yMax;

    public:
        virtual auto read(std::ifstream& file) -> bool override
        {
            for (auto&& member : {
                     &numberOfContours,
                     &xMin,
                     &yMin,
                     &xMax,
                     &yMax }) {
                file.read(reinterpret_cast<char*>(member), sizeof(i16));
                *member = ntohs(*member);
            }

            return true;
        }

        [[nodiscard]] auto to_string() const noexcept -> std::string
        {
            return std::format("GlyphHeader(numberOfContours: {}, min: ({}; {}), max: ({}; {}))",
                               numberOfContours,
                               xMin, yMin,
                               xMax, yMax);
        }
    };

    struct SimpleGlyphDescription {
        enum Flags {
            ON_CURVE_POINT,
            X_SHORT_VECTOR,
            Y_SHORT_VECTOR,
            REPEAT,
            X_IS_SAME_OR_POSITIVE_X_SHORT_VECTOR,
            Y_IS_SAME_OR_POSITIVE_Y_SHORT_VECTOR,
            OVERLAP_SIMPLE,
            RESERVED,
            Num_FLAGS
        };
        using flag_t = std::bitset<Flags::Num_FLAGS>;

        std::vector<u16> endPtsOfContours;
        u16 instructionLength;
        std::vector<u8> instructions;
        std::vector<flag_t> flags;
        std::vector<i16> xCoordinates;
        std::vector<i16> yCoordinates;
    };

    friend class OpenType;

    std::shared_ptr<IndexToLocation> m_location;
    std::vector<GlyphHeader> m_headers;

public:
    GlyphData(std::shared_ptr<IndexToLocation> location)
        : m_location(location)
        , m_headers(location->size() - 1) { };

    virtual auto read(std::ifstream& file) -> bool override
    {
        auto const& loca = *m_location;
        size_t base = file.tellg();

        for (auto i = 0uz; i < loca.size() - 1; i++) {
            auto size = loca[i + 1] - loca[i];
            file.seekg(base + loca[i]);

            /**
             * SPEC: This also applies to any other glyphs without an outline, 
             *       such as the glyph for the space character: if a glyph has 
             *       no outline or instructions, then loca[n] = loca[n+1].
             *
             * FIXME: Properly handle when size is 0.
             */
            if (size == 0)
                continue;

            m_headers[i].read(file);
        }

        return true;
    }

    [[nodiscard]] auto to_string() const noexcept -> std::string
    {
        return std::format("GlyphData(headers: {})",
                           m_headers.size());
    }

    [[nodiscard]] auto headers() const noexcept -> std::vector<GlyphHeader> const&
    {
        return m_headers;
    }
};
