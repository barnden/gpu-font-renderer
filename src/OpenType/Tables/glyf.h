#pragma once

#include <arpa/inet.h>
#include <bitset>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <ranges>
#include <vector>

#include "OpenType/Tables/loca.h"

#include "OpenType/Defines.h"
#include "OpenType/Tables/Table.h"
#include "OpenType/Tables/loca.h"

class GlyphData : public Table {
    // https://learn.microsoft.com/en-us/typography/opentype/spec/glyf

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

        [[nodiscard]] auto contours() const -> i16
        {
            return numberOfContours;
        }

        [[nodiscard]] auto min() const -> std::tuple<i16, i16>
        {
            return std::make_tuple(xMin, yMin);
        }

        [[nodiscard]] auto max() const -> std::tuple<i16, i16>
        {
            return std::make_tuple(xMax, yMax);
        }
    };

    struct BaseGlyphDescription {
        GlyphHeader header;
        virtual ~BaseGlyphDescription() = default;

        virtual auto read(std::ifstream& file) -> bool = 0;
        [[nodiscard]] virtual auto to_string() const noexcept -> std::string = 0;
    };

    struct SimpleGlyphDescription : public BaseGlyphDescription {
        enum Flags {
            ON_CURVE_POINT,
            X_SHORT_VECTOR,
            Y_SHORT_VECTOR,
            REPEAT,
            X_SAME_OR_POSITIVE,
            Y_SAME_OR_POSITIVE,
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

        virtual auto read(std::ifstream& file) -> bool override
        {
            if (header.contours() < 0)
                return false;

            /**
             * SPEC: If a glyph has zero contours, no additional glyph data beyond the header is required.
             */
            if (header.contours() == 0)
                return true;

            endPtsOfContours.resize(header.contours());

            for (auto& end_point : endPtsOfContours) {
                file.read(reinterpret_cast<char*>(&end_point), sizeof(u16));
                end_point = ntohs(end_point);
            }

            file.read(reinterpret_cast<char*>(&instructionLength), sizeof(u16));
            instructionLength = ntohs(instructionLength);

            if (instructionLength > 0) {
                instructions.resize(instructionLength);

                for (auto& instruction : instructions) {
                    file.read(reinterpret_cast<char*>(&instruction), sizeof(u8));
                    instruction = ntohs(instruction);
                }
            }

            auto num_points = endPtsOfContours.back();
            flags.resize(num_points);
            xCoordinates.resize(num_points);
            yCoordinates.resize(num_points);

            {
                auto i = 0;
                while (i < num_points) {
                    u8 temp = 0;
                    file.read(reinterpret_cast<char*>(&temp), sizeof(u8));

                    flag_t point_flags(temp);
                    flags[i] = point_flags;

                    if (point_flags[Flags::REPEAT]) {
                        file.read(reinterpret_cast<char*>(&temp), sizeof(u8));

                        for (auto j = 0; j < temp; j++) {
                            flags[i] = point_flags;
                            i++;
                        }
                    }
                    i++;
                }
            }

            auto read_coordinates = [&](std::vector<i16>& coordinates, Flags short_vector, Flags same_or_positive) {
                for (auto&& [i, flag] : std::views::enumerate(flags)) {
                    bool is_byte = flag[short_vector];
                    bool is_same_or_positive = flag[same_or_positive];

                    auto& coordinate = coordinates[i];

                    if (is_byte) {
                        file.read(reinterpret_cast<char*>(&coordinate), sizeof(u8));

                        coordinate = is_same_or_positive ? coordinate : -coordinate;

                        continue;
                    }

                    if (is_same_or_positive) {
                        if (i == 0)
                            continue;

                        coordinate = xCoordinates[i - 1];
                        continue;
                    }

                    file.read(reinterpret_cast<char*>(&coordinate), sizeof(u16));
                    coordinate = ntohs(coordinate);
                }
            };

            read_coordinates(xCoordinates, X_SHORT_VECTOR, X_SAME_OR_POSITIVE);
            read_coordinates(yCoordinates, Y_SHORT_VECTOR, Y_SAME_OR_POSITIVE);

            return true;
        }

        [[nodiscard]] virtual auto to_string() const noexcept -> std::string override
        {
            return std::format("SimpleGlyphDescription(min: {}, max: {}, contours: {}, points: {}, instructions: {})",
                               header.min(),
                               header.max(),
                               header.contours(),
                               endPtsOfContours.back(), // SPEC: The number of points is determined by the last entry in the endPtsOfContours array
                               instructionLength);
        }
    };

    friend class OpenType;

    std::shared_ptr<IndexToLocation> m_location;
    std::vector<std::shared_ptr<BaseGlyphDescription>> m_glyphs;

public:
    GlyphData(std::shared_ptr<IndexToLocation> location)
        : m_location(location)
        , m_glyphs(location->size() - 1, nullptr) { };

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
             */
            if (size == 0)
                continue;

            GlyphHeader header {};
            header.read(file);

            if (header.contours() < 0) {
                std::println(
                    std::cerr,
                    "Composite glyph description found in font; currently unimplemented.");

                continue;
            }

            m_glyphs[i] = std::make_shared<SimpleGlyphDescription>();
            m_glyphs[i]->header = std::move(header);

            m_glyphs[i]->read(file);
        }

        return true;
    }

    [[nodiscard]] auto to_string() const noexcept -> std::string
    {
        return std::format("GlyphData(glyph_descriptions: {})",
                           m_glyphs.size());
    }

    [[nodiscard]] auto operator[](u16 glyphID) const -> std::shared_ptr<BaseGlyphDescription>
    {
        if (glyphID >= m_glyphs.size())
            return nullptr;

        return m_glyphs[glyphID];
    }
};
