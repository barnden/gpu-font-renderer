#pragma once

#include <arpa/inet.h>
#include <cassert>
#include <fstream>
#include <iostream>
#include <memory>
#include <print>
#include <ranges>
#include <vector>

#include "OpenType/Defines.h"
#include "OpenType/Tables/Table.h"

struct EncodingRecord {
    enum Platform : u16 {
        UNICODE = 0,
        MACINTOSH,
        ISO, // Deprecated
        WINDOWS,
        CUSTOM,
        // Platform IDs 240-255 are reserved for user-defined platforms
        RESERVED_240 = 240,
        RESERVED_241,
        RESERVED_242,
        RESERVED_243,
        RESERVED_244,
        RESERVED_245,
        RESERVED_246,
        RESERVED_247,
        RESERVED_248,
        RESERVED_249,
        RESERVED_250,
        RESERVED_251,
        RESERVED_252,
        RESERVED_253,
        RESERVED_254,
        RESERVED_255,
    } platformID {};

    u16 encodingID {};
    u32 subtableOffset {};

    auto read(std::ifstream& file) -> bool
    {
        u16 temp;
        file.read(reinterpret_cast<char*>(&temp), sizeof(u16));
        platformID = static_cast<Platform>(ntohs(temp));
        file.read(reinterpret_cast<char*>(&encodingID), sizeof(u16));
        encodingID = ntohs(encodingID);
        file.read(reinterpret_cast<char*>(&subtableOffset), sizeof(u32));
        subtableOffset = ntohl(subtableOffset);

        return true;
    }

    [[nodiscard]] auto to_string() const noexcept -> std::string
    {
        return std::format("EncodingRecord(platformID: {}, encodingID: {}, subtableOffset: 0x{:08X})",
                           static_cast<u16>(platformID),
                           encodingID,
                           subtableOffset);
    }
};

template <size_t Format>
struct Subtable { };

struct BaseSubtable {
    virtual ~BaseSubtable() { };

    // Perform mapping from character to glyph index;
    [[nodiscard]] virtual auto map(u16) const -> std::optional<u32>
    {
        return std::nullopt;
    }

    virtual auto read(std::ifstream& file) -> bool
    {
        size_t cursor = file.tellg();
        file.seekg(cursor + sizeof(u16));

        return true;
    }
};

template <>
struct Subtable<0> : BaseSubtable {
    // Byte encoding table
    u16 format = 0;
    u16 length;
    u16 language;
    u8 glyphIdArray[256] {};

    [[nodiscard]] virtual auto map(u16 chr) const -> std::optional<u32> override
    {
        if (chr > 255)
            return std::nullopt;

        return glyphIdArray[chr];
    }

    virtual auto read(std::ifstream& file) -> bool override
    {
        BaseSubtable::read(file);

        file.read(reinterpret_cast<char*>(&length), sizeof(u16));
        length = ntohs(length);
        file.read(reinterpret_cast<char*>(&language), sizeof(u16));
        language = ntohs(language);

        u16 n = 3 * sizeof(u16);

        for (; n < length; n++) {
            file.read(reinterpret_cast<char*>(&glyphIdArray[n]), 1);
        }

        return true;
    }
};

template <>
struct Subtable<2> : BaseSubtable {
    struct Subheader {
        u16 firstCode;
        u16 entryCount;
        i16 idDelta;
        u16 idRangeOffset;

        /**
         * The firstCode and entryCount values specify a subrange that begins at
         * firstCode and has a length equal to the value of entryCount.
         * This subrange stays within the 0-255 range of the byte being mapped.
         * Bytes outside of this subrange are mapped to glyph index 0
         * (missing glyph). The offset of the byte within this subrange is then used
         * as index into a corresponding sub-array of glyphIdArray. This sub-array
         * is also of length entryCount. The value of the idRangeOffset is the
         * number of bytes past the actual location of the idRangeOffset word where
         * the glyphIdArray element corresponding to firstCode appears.

         * Finally, if the value obtained from the sub-array is not 0 (which
         * indicates the missing glyph), you should add idDelta to it in order to
         * get the glyphIndex. The value idDelta permits the same sub-array to be
         * used for several different subheaders. The idDelta arithmetic is modulo
         * 65536. If the result after adding idDelta to the value from the sub-array
         * is less than zero, add 65536 to obtain a valid glyph ID.
         */
    };

    // High byte mapping through table
    u16 format = 2;
    u16 length;
    u16 language;
    u16 subHeaderKeys[256];
    std::vector<Subheader> subHeaders;
    std::vector<u16> glyphIdArray;

    [[nodiscard]] virtual auto map(u16) const -> std::optional<u32> override
    {
        // Unsupported "This format is not commonly used today."
        ASSERT_NOT_REACHED;
    }

    virtual auto read(std::ifstream&) -> bool override
    {
        ASSERT_NOT_REACHED;
    }
};

template <>
struct Subtable<4> : BaseSubtable {
    // Segment mapping to delta values
    u16 format = 2;
    u16 length;
    u16 language;
    u16 segCountX2;
    u16 searchRange;
    u16 entrySelector;
    u16 rangeShift;
    std::vector<u16> endCode; // size = segCountX2 / 2
    u16 reservedPad = 0;
    std::vector<u16> startCode;
    std::vector<i16> idDelta;
    std::vector<u16> idRangeOffset;
    std::vector<u16> glyphIdArray;

    [[nodiscard]] virtual auto map(u16 chr) const -> std::optional<u32> override
    {
        using std::views::iota, std::views::zip;
        // (0) endCode, startCode, idDelta, idRangeOffset are parallel arrays
        for (auto&& [idx, start, end, delta, offset] : zip(iota(0), startCode, endCode, idDelta, idRangeOffset)) {
            // (1) Find the first endCode that is greater than or equal to the character code
            if (end < chr)
                continue;

            if (start > chr)
                return std::nullopt;

            // (2) If the corresponding startCode is less than or equal to character code
            //     Then use the corresponding idDelta and idRangeOffset to map character code to a glyph index

            // (2.1) If idRangeOffset is zero, then add idDelta directly
            if (offset == 0) {
                auto glyphId = static_cast<i32>(chr) + static_cast<i32>(delta);
                return static_cast<u16>((glyphId + 65536) % 65536);
            }

            /**
             * (2.2) If idRangeOffset is nonzero, then character code mapping
             *       relies on glyphIDArray.
             *
             * The OpenType spec computes glyphID under the assumption that
             * idRangeOffset and glyphIdArray lie in contiguous memory:
             *      glyphID := *(idRangeOffset[i]/2
             *                 + (c - startCode[i])
             *                 + &idRangeOffset[i])
             *
             * For noncontiguous memory...
             *  - &glyphIdArray[0] = &idRangeOffset[segCount]
             *  - &idRangeOffset[i] = &glyphIdArray[0] - segCount + i
             *  - (glyphIdArray[0] + i - segCount) + ((idRangeOffset[i] / 2) + (c - startCode[i]))
             */

            auto glyphIdx = static_cast<i32>(idx) - static_cast<i32>(segCountX2 / 2) + (static_cast<i32>(offset) / 2) + (static_cast<i32>(chr) - static_cast<i32>(start));

            if (glyphIdx < 0 || static_cast<size_t>(glyphIdx) >= glyphIdArray.size()) {
#ifndef NDEBUG
                std::println(std::cerr,
                             "[cmap] Subtable(type: 4) attempted to map chr \\u{:04X}, but encountered out of bounds memory access (glyphIdx: {}) on glyphIdArray(size: {}).",
                             chr,
                             glyphIdx,
                             glyphIdArray.size());
#endif
                return std::nullopt;
            }

            return glyphIdArray[glyphIdx];
        }

        return std::nullopt;
    }

    virtual auto read(std::ifstream& file) -> bool override
    {
        size_t base = file.tellg();
        BaseSubtable::read(file);
        /**
         * FIXME: Compute searchRange, entrySelector, rangeShift ourselves.
         */
        for (auto&& member : { &length,
                               &language,
                               &segCountX2,
                               &searchRange,
                               &entrySelector,
                               &rangeShift }) {
            file.read(reinterpret_cast<char*>(member), sizeof(u16));
            *member = ntohs(*member);
        }

        endCode.resize(segCountX2 / 2);
        startCode.resize(segCountX2 / 2);
        idDelta.resize(segCountX2 / 2);
        idRangeOffset.resize(segCountX2 / 2);

        for (auto& code : endCode) {
            file.read(reinterpret_cast<char*>(&code), sizeof(u16));
            code = ntohs(code);

            if (code == 0xFFFF)
                break;
        }

        // Skip padding
        file.seekg(static_cast<size_t>(file.tellg()) + sizeof(u16));

        for (auto& code : startCode) {
            file.read(reinterpret_cast<char*>(&code), sizeof(u16));
            code = ntohs(code);

            if (code == 0xFFFF)
                break;
        }

        for (auto& delta : idDelta) {
            file.read(reinterpret_cast<char*>(&delta), sizeof(u16));
            delta = ntohs(delta);
        }

        for (auto& offset : idRangeOffset) {
            file.read(reinterpret_cast<char*>(&offset), sizeof(u16));
            offset = ntohs(offset);
        }

        auto num_bytes_read = static_cast<size_t>(file.tellg()) - base;
        if (num_bytes_read > static_cast<size_t>(length))
            return false;

        auto num_bytes_remaining = static_cast<size_t>(length) - num_bytes_read;

        if (num_bytes_remaining % sizeof(u16)) {
            std::println(
                std::cerr,
                "Subtable<4>.read() num_bytes_remaining: {} expected multiple of sizeof(u16).",
                num_bytes_remaining);

            return false;
        }

        if (num_bytes_remaining == 0)
            return true;

        glyphIdArray.resize(num_bytes_remaining / 2);

        for (auto& glyphID : glyphIdArray) {
            file.read(reinterpret_cast<char*>(&glyphID), sizeof(u16));
            glyphID = ntohs(glyphID);
        }

        return true;
    }
};

template <>
struct Subtable<6> : BaseSubtable {
    // Trimmed table mapping
    u16 format = 6;
    u16 length;
    u16 language;
    u16 firstCode;
    u16 entryCount;
    std::vector<u16> glyphIdArray;

    [[nodiscard]] virtual auto map(u16) const -> std::optional<u32> override
    {
        ASSERT_NOT_REACHED;
    }

    virtual auto read(std::ifstream&) -> bool override
    {
        ASSERT_NOT_REACHED;
    }
};

template <>
struct Subtable<8> : BaseSubtable {
    // Mixed 16-bit and 32-bit coverage

    struct SequentialMapGroup {
        u32 startCharCode;
        u32 endCharCode;
        u32 startGlyphID;
    };

    u16 format;
    u16 reserved;
    u32 length;
    u32 language;
    u8 is32[8192];
    u32 numGroups;
    std::vector<SequentialMapGroup> groups;

    [[nodiscard]] virtual auto map(u16) const -> std::optional<u32> override
    {
        ASSERT_NOT_REACHED;
    }

    virtual auto read(std::ifstream&) -> bool override
    {
        ASSERT_NOT_REACHED;
    }
};

template <>
struct Subtable<10> : BaseSubtable {
    // Trimmed array
    u16 format;
    u16 reserved;
    u32 length;
    u32 language;
    u32 startCharCode;
    u32 numChars;
    std::vector<u16> glyphIdArray;

    [[nodiscard]] virtual auto map(u16) const -> std::optional<u32> override
    {
        ASSERT_NOT_REACHED;
    }

    virtual auto read(std::ifstream&) -> bool override
    {
        ASSERT_NOT_REACHED;
    }
};

template <>
struct Subtable<12> : BaseSubtable {
    // Segmented coverage

    struct SequentialMapGroup {
        u32 startCharCode;
        u32 endCharCode;
        u32 startGlyphID;
    };

    u16 format;
    u16 reserved;
    u32 length;
    u32 language;
    u32 numGroups;
    std::vector<SequentialMapGroup> groups;

    [[nodiscard]] virtual auto map(u16 chr) const -> std::optional<u32> override
    {
        // FIXME: Probably should do some sort of caching and binary search
        // FIXME: Need to accept u32 instead of u16 for chr, on all subtable map() methods

        for (auto&& group : groups) {
            if (group.startCharCode <= chr && group.endCharCode >= chr) {
                return group.startGlyphID + (chr - group.startCharCode);
            }
        }

#ifndef NDEBUG
        std::println(std::cerr,
                     "[cmap] Subtable(type: 12) attempted to map chr \\u{:04X}.",
                     chr);
#endif

        return std::nullopt;
    }

    virtual auto read(std::ifstream& file) -> bool override
    {
        size_t base = file.tellg();
        static constexpr auto fields = std::make_tuple(
            &Subtable<12>::format,
            &Subtable<12>::reserved,
            &Subtable<12>::length,
            &Subtable<12>::language,
            &Subtable<12>::numGroups);

        std::apply([&](auto&&... members) {
            ([&](auto member) {
                using MemberType = decltype(this->*member);
                file.read(reinterpret_cast<char*>(&(this->*member)), sizeof(this->*member));

                if constexpr (sizeof(MemberType) == 2) {
                    this->*member = ntohs(this->*member);
                } else if constexpr (sizeof(MemberType) == 4) {
                    this->*member = ntohl(this->*member);
                } else {
        ASSERT_NOT_REACHED;
                }
            }(members),
             ...);
        },
                   fields);

        groups.resize(numGroups);

        for (auto&& group : groups) {
            if (!group.read(file))
                return false;

            if (static_cast<size_t>(file.tellg()) - base > length)
                return false;
        }

        return true;
    }
};

template <>
struct Subtable<13> : BaseSubtable {
    // Many-to-one range mappings

    struct ConstantMapGroup {
        u32 startCharCode;
        u32 endCharCode;
        u32 glyphID;
    };

    u16 format;
    u16 reserved;
    u32 length;
    u32 language;
    u32 numGroups;
    ConstantMapGroup groups;

    [[nodiscard]] virtual auto map(u16) const -> std::optional<u32> override
    {
        ASSERT_NOT_REACHED;
    }

    virtual auto read(std::ifstream&) -> bool override
    {
        ASSERT_NOT_REACHED;
    }
};

template <>
struct Subtable<14> : BaseSubtable {
    // Unicode variation sequences

    struct VariationSelector {
        u32 varSelector : 24; // 24-bit
        u32 defaultUVSOffset;
        u32 nonDefaultUVSOffset;
    };

    struct UnicodeRange {
        u32 varSelector : 24; // 24-bit
        u8 additionalCount;
    };
    struct DefaultUVSTable {
        u32 numUnicodeValueRanges;
        std::vector<UnicodeRange> ranges;
    };

    struct UVSMapping {
        u32 unicodeValue : 24;
        u16 glyphID;
    };
    struct UVSMapTable {
        u32 numUVSMappings;
        std::vector<UVSMapping> uvsMappings;
    };

    u16 format;
    u32 length;
    u32 numVarSelectorRecords;
    std::vector<VariationSelector> varSelector;

    [[nodiscard]] virtual auto map(u16) const -> std::optional<u32> override
    {
        ASSERT_NOT_REACHED;
    }

    virtual auto read(std::ifstream&) -> bool override
    {
        ASSERT_NOT_REACHED;
    }
};

enum class UnicodePlatform : u16 {
    UNICODE_1_0 = 0, // Deprecated
    UNICODE_1_1, // Deprecated
    ISO_IEC_10646, // Deprecated
    UNICODE_2_0_BMP, // Unicode 2.0+ semantics, BMP
    UNICODE_2_0_FULL, // Unicode 2.0+ semantics, full repertoire
    UNICODE_VAR_SEQ, // Unicode variation sequences, subtable 14
    UNICODE_FULL, // Unicode full repertoire, subtable 13
};

enum class MacintoshPlatform : u16 {
    ASCII = 0, // 7-bit ASCII
    ISO_10646,
    ISO_8859_1,
};

enum class WindowsPlatform : u16 {
    SYMBOL = 0,
    UNICODE_BMP,
    SHIFT_JIS,
    PRC,
    BIG_5,
    WANSUNG,
    JOHAB,
    RESERVED_0,
    RESERVED_1,
    RESERVED_2,
    UNICODE_FULL
};

class CharacterMap : public Table {
    // https://learn.microsoft.com/en-us/typography/opentype/spec/cmap
    u16 version {};
    u16 numTables {};
    std::vector<EncodingRecord> encodingRecords {};

    std::vector<std::shared_ptr<BaseSubtable>> subtables {};

public:
    static constexpr TableTag g_identifier { 'c', 'm', 'a', 'p' };

    CharacterMap() = default;

    [[nodiscard]] auto records() const -> std::vector<EncodingRecord> const&
    {
        return encodingRecords;
    }

    virtual auto read(std::ifstream& file) -> bool override
    {
        size_t base = file.tellg();
        file.read(reinterpret_cast<char*>(&version), sizeof(u16));
        version = ntohs(version);

        file.read(reinterpret_cast<char*>(&numTables), sizeof(u16));
        numTables = ntohs(numTables);

        encodingRecords.resize(numTables);
        for (auto& record : encodingRecords) {
            record.read(file);

            size_t cursor = file.tellg();
            file.seekg(base + record.subtableOffset);
            u16 format = 0;
            file.read(reinterpret_cast<char*>(&format), sizeof(u16));
            format = ntohs(format);
            file.seekg(base + record.subtableOffset);

            /**
             * FIXME: Different encodingRecords can map to the same subtable.
             *        We're making a new subtable for each record, even if
             *        they're duplicates.
             */
            auto subtable = std::shared_ptr<BaseSubtable>(nullptr);
            switch (format) {
            case 0:
                subtable = std::make_shared<Subtable<0>>();
                break;
            case 4:
                subtable = std::make_shared<Subtable<4>>();
                break;
            case 12:
                subtable = std::make_shared<Subtable<12>>();
                break;
            case 2:
            case 6:
            case 8:
            case 10:
            case 13:
            case 14:
#ifndef NDEBUG
                std::println(
                    std::cerr,
                    "CharacterMap encounted unsupported Subtable format {}.",
                    format);

#endif
                return false;

            default:
                ASSERT_NOT_REACHED;
            }

            subtable->read(file);

            subtables.push_back(subtable);

            file.seekg(cursor);
        }

        return true;
    }

    [[nodiscard]] auto map(u16 chr) const noexcept -> u16
    {
        // FIXME: Use platform/encoding to choose proper subtable
        return subtables[0]->map(chr).value_or(0);
    }

    [[nodiscard]] auto to_string() const noexcept -> std::string
    {
        return std::format("CharacterMap(version: {}, numTables: {})",
                           version,
                           numTables);
    }
};
