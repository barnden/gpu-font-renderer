#pragma once

#include <algorithm>
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

class GlyphHeader {
    i16 numberOfContours;
    i16 xMin;
    i16 yMin;
    i16 xMax;
    i16 yMax;

public:
    auto read(std::ifstream& file) -> bool
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

    [[nodiscard]] auto min() const -> std::pair<i16, i16>
    {
        return std::make_tuple(xMin, yMin);
    }

    [[nodiscard]] auto max() const -> std::pair<i16, i16>
    {
        return std::make_tuple(xMax, yMax);
    }
};

class GlyphData;
class BaseGlyphDescription {
protected:
    friend class GlyphData;
    GlyphHeader m_header;

public:
    virtual ~BaseGlyphDescription() = default;

    auto header() const -> GlyphHeader const&
    {
        return m_header;
    }

    virtual auto process(std::vector<std::shared_ptr<BaseGlyphDescription>> const&) -> void { };

    virtual auto read(std::ifstream& file) -> bool = 0;
    [[nodiscard]] virtual auto contours() const noexcept -> std::vector<std::vector<std::pair<i16, i16>>> const& = 0;
    [[nodiscard]] virtual auto to_string() const noexcept -> std::string = 0;
};

class SimpleGlyphDescription : public BaseGlyphDescription {
    friend class GlyphData;

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

    std::vector<u16> m_contour_ends;
    std::vector<u8> m_instructions;
    std::vector<flag_t> m_flags;
    std::vector<std::vector<std::pair<i16, i16>>> m_contours;

    auto wrap(int v, int delta, int minval, int maxval) -> int
    {
        const int mod = maxval + 1 - minval;
        v += delta - minval;
        v += (1 - v / mod) * mod;
        return v % mod + minval;
    }

    void process_points(std::vector<std::pair<i16, i16>>&& points)
    {
        // A file-size optimization is done with the points array:
        // Points with repeated on- or off-curve characteristics imply
        // a control point with opposite characteristic at the midpoint.
        m_contours.resize(m_contour_ends.size());

        for (auto&& [idx, contour, end] : std::views::zip(std::views::iota(0), m_contours, m_contour_ends)) {
            auto const start = (idx == 0) ? 0 : (m_contour_ends[idx - 1] + 1);
            bool should_shift = false;

            for (auto i = start; i <= end; i++) {
                auto const prev = wrap(i, -1, start, end);

                auto&& [x, y] = points[i];
                auto touching = m_flags[i][Flags::ON_CURVE_POINT];

                auto&& [x_prev, y_prev] = points[prev];
                auto touching_prev = m_flags[prev][Flags::ON_CURVE_POINT];

                if (touching == touching_prev) {
                    auto midpoint = std::make_tuple((x + x_prev) / 2, (y + y_prev) / 2);

                    if (contour.empty())
                        should_shift = touching;

                    contour.push_back(std::move(midpoint));
                }

                if (contour.empty())
                    should_shift = !touching;

                contour.push_back(points[i]);
            }

            // For my purposes, I prefer the first point of the contour to be on-surface
            if (should_shift) {
                std::rotate(contour.begin(), contour.begin() + 1, contour.end());
            }
        }
    }

public:
    [[nodiscard]] virtual auto contours() const noexcept -> std::vector<std::vector<std::pair<i16, i16>>> const& override
    {
        return m_contours;
    }

    virtual auto read(std::ifstream& file) -> bool override
    {
        if (m_header.contours() < 0)
            return false;

        /**
         * SPEC: If a glyph has zero contours, no additional glyph data beyond the header is required.
         */
        if (m_header.contours() == 0)
            return true;

        m_contour_ends.resize(m_header.contours());

        for (auto& end_point : m_contour_ends) {
            file.read(reinterpret_cast<char*>(&end_point), sizeof(u16));
            end_point = ntohs(end_point);
        }

        u16 num_instructions = 0;
        file.read(reinterpret_cast<char*>(&num_instructions), sizeof(u16));
        num_instructions = ntohs(num_instructions);

        if (num_instructions > 0) {
            m_instructions.resize(num_instructions);

            for (auto& instruction : m_instructions) {
                file.read(reinterpret_cast<char*>(&instruction), sizeof(u8));
            }
        }

        auto num_points = m_contour_ends.back() + 1;
        m_flags.resize(num_points);

        auto points = std::vector<std::pair<i16, i16>>(num_points);

        {
            auto i = 0;
            while (i < num_points) {
                u8 temp = 0;
                file.read(reinterpret_cast<char*>(&temp), sizeof(u8));

                flag_t point_flags(temp);
                m_flags[i++] = point_flags;

                if (!point_flags[Flags::REPEAT])
                    continue;

                file.read(reinterpret_cast<char*>(&temp), sizeof(u8));

                for (auto j = 0; j < temp; j++) {
                    m_flags[i++] = point_flags;
                }
            }
        }

        auto read_coordinates = [&]<size_t I>(Flags short_vector, Flags same_or_positive) {
            for (auto&& [i, flag] : std::views::enumerate(m_flags)) {
                i16 last = (i == 0) ? 0 : std::get<I>(points[i - 1]);
                bool is_byte = flag[short_vector];
                bool is_same_or_positive = flag[same_or_positive];

                auto& coordinate = std::get<I>(points[i]);

                if (is_byte) {
                    file.read(reinterpret_cast<char*>(&coordinate), sizeof(u8));

                    coordinate = (is_same_or_positive ? coordinate : -coordinate) + last;

                    continue;
                }

                if (is_same_or_positive) {
                    if (i == 0)
                        continue;

                    coordinate = last;
                    continue;
                }

                file.read(reinterpret_cast<char*>(&coordinate), sizeof(i16));
                coordinate = ntohs(coordinate) + last;
            }
        };

        read_coordinates.template operator()<0>(X_SHORT_VECTOR, X_SAME_OR_POSITIVE);
        read_coordinates.template operator()<1>(Y_SHORT_VECTOR, Y_SAME_OR_POSITIVE);

        process_points(std::move(points));

        return true;
    }

    [[nodiscard]] virtual auto to_string() const noexcept -> std::string override
    {
        return std::format("SimpleGlyphDescription(min: {}, max: {}, contours: {}, points: {}, instructions: {})",
                           m_header.min(),
                           m_header.max(),
                           m_header.contours(),
                           m_contour_ends.back(), // SPEC: The number of points is determined by the last entry in the endPtsOfContours array
                           m_instructions.size());
    }
};

class CompositeGlyphDescription : public BaseGlyphDescription {
    /**
     * Composite glyphs may be nested within other composite glyphs—that is,
     * a composite glyph parent may include other composite glyphs as child
     * components. Thus, a composite glyph description is a directed graph.
     * This graph must be acyclic, with every path through the graph leading to
     * a simple glyph as a leaf node
     */

    /**
     * The parent can also specify a scale or other affine transform to be
     * applied to a child glyph as it is incorporated into the parent.
     * The transform can affect an offset vector used to position the child glyph
     */

    class CompositeGlyphRecord {
    public:
        enum Flags {
            ARG_1_AND_2_ARE_WORDS = 0,
            ARGS_ARE_XY_VALUES,
            ROUND_XY_TO_GRID,
            WE_HAVE_A_SCALE,
            RESERVED_4,
            MORE_COMPONENTS,
            WE_HAVE_AN_X_AND_Y_SCALE,
            WE_HAVE_A_TWO_BY_TWO,
            WE_HAVE_INSTRUCTIONS,
            USE_MY_METRICS,
            OVERLAP_COMPOUND,
            SCALED_COMPONENT_OFFSET,
            UNSCALED_COMPONENT_OFFSET,
            RESERVED_13,
            RESERVED_14,
            RESERVED_15,
            Num_FLAGS
        };

        using flag_t = std::bitset<Flags::Num_FLAGS>;

    private:
        flag_t m_flags;
        u16 m_glyphIndex;

        // SPEC: It can be u8, i8, u16, i16. Chose i32 container as all are representable.
        i32 m_argument1;
        i32 m_argument2;

        F2DOT14 x_scale { 1.0 };
        F2DOT14 y_scale { 1.0 };
        F2DOT14 scale01 { 0.0 };
        F2DOT14 scale10 { 0.0 };

    public:
        [[nodiscard]] auto flags() const noexcept -> flag_t const&
        {
            return m_flags;
        }

        [[nodiscard]] auto glyph_id() const noexcept -> u16
        {
            return m_glyphIndex;
        }

        auto apply_transformation(std::pair<i16, i16>& point)
        {
            auto x = point.first;
            auto y = point.second;

            point.first = x_scale.value() * x + scale10.value() * y;
            point.second = scale01.value() * x + y_scale.value() * y;

            if (m_flags[Flags::ARGS_ARE_XY_VALUES]) {
                // ROUND_XY_TO_GRID unsupported
                point.first += m_argument1;
                point.second += m_argument2;
            } else {
                // FIXME: Point alignment is unsupported, and not possible with
                //        the current approach preprocessing the point data.
                ASSERT_NOT_REACHED;
            }
        }

        auto read(std::ifstream& file) -> bool
        {
            u16 temp = 0;
            file.read(reinterpret_cast<char*>(&temp), sizeof(u16));
            temp = ntohs(temp);
            m_flags = flag_t(temp);

            file.read(reinterpret_cast<char*>(&m_glyphIndex), sizeof(u16));
            m_glyphIndex = ntohs(m_glyphIndex);

            if (m_flags[Flags::ARG_1_AND_2_ARE_WORDS]) {
                file.read(reinterpret_cast<char*>(&m_argument1), sizeof(u16));
                m_argument1 = ntohs(m_argument1);

                file.read(reinterpret_cast<char*>(&m_argument2), sizeof(u16));
                m_argument2 = ntohs(m_argument2);
            } else {
                file.read(reinterpret_cast<char*>(&temp), sizeof(u16));
                temp = ntohs(temp);

                m_argument1 = (temp >> 8) & 0xFF;
                m_argument2 = temp & 0xFF;
            }

            if (m_flags[Flags::WE_HAVE_A_SCALE]) {
                file.read(reinterpret_cast<char*>(&temp), sizeof(u16));
                temp = ntohs(temp);

                x_scale.data = temp;
                y_scale.data = temp;
            } else if (m_flags[Flags::WE_HAVE_AN_X_AND_Y_SCALE]) {
                file.read(reinterpret_cast<char*>(&x_scale.data), sizeof(u16));
                x_scale.data = ntohs(x_scale.data);

                file.read(reinterpret_cast<char*>(&y_scale.data), sizeof(u16));
                y_scale.data = ntohs(y_scale.data);
            } else if (m_flags[Flags::WE_HAVE_A_TWO_BY_TWO]) {
                file.read(reinterpret_cast<char*>(&x_scale.data), sizeof(u16));
                x_scale.data = ntohs(x_scale.data);

                file.read(reinterpret_cast<char*>(&scale01.data), sizeof(u16));
                scale01.data = ntohs(scale01.data);

                file.read(reinterpret_cast<char*>(&scale10.data), sizeof(u16));
                scale10.data = ntohs(scale10.data);

                file.read(reinterpret_cast<char*>(&y_scale.data), sizeof(u16));
                y_scale.data = ntohs(y_scale.data);
            }

            return true;
        }
    }; // class CompositeGlyphRecord

    friend class GlyphData;

    std::vector<CompositeGlyphRecord> m_glyphs {};
    std::vector<std::vector<std::pair<i16, i16>>> m_contours {};

public:
    virtual auto read(std::ifstream& file) -> bool override
    {
        do {
            auto record = CompositeGlyphRecord {};

            if (!record.read(file))
                return false;

            m_glyphs.push_back(std::move(record));
        } while (m_glyphs.back().flags()[CompositeGlyphRecord::Flags::MORE_COMPONENTS]);

        return true;
    }

    virtual auto process(std::vector<std::shared_ptr<BaseGlyphDescription>> const& glyphData) -> void override
    {
        for (auto&& record : m_glyphs) {
            auto glyph = glyphData[record.glyph_id()];

            if (glyph->contours().empty())
                glyph->process(glyphData);

            for (auto&& contour : glyph->contours()) {
                m_contours.push_back(contour);

                std::transform(
                    m_contours.back().begin(), m_contours.back().end(),
                    m_contours.back().begin(),
                    [&](std::pair<i16, i16> pair) {
                        record.apply_transformation(pair);

                        return pair;
                    });
            }
        }
    }

    [[nodiscard]] virtual auto contours() const noexcept -> std::vector<std::vector<std::pair<i16, i16>>> const& override
    {
        return m_contours;
    }

    [[nodiscard]] virtual auto to_string() const noexcept -> std::string override
    {
        return std::format("CompositeGlyphDescription(min: {}, max: {})",
                           m_header.min(),
                           m_header.max());
    }
};

class GlyphData : public Table {
    // https://learn.microsoft.com/en-us/typography/opentype/spec/glyf

public:
    static constexpr TableTag g_identifier { 'g', 'l', 'y', 'f' };

private:
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

            if (header.contours() >= 0) {
                m_glyphs[i] = std::make_shared<SimpleGlyphDescription>();
            } else {
                m_glyphs[i] = std::make_shared<CompositeGlyphDescription>();
            }

            m_glyphs[i]->m_header = std::move(header);
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

        if (m_glyphs[glyphID] == nullptr)
            return nullptr;

        if (m_glyphs[glyphID]->contours().empty()) {
            m_glyphs[glyphID]->process(m_glyphs);
        }

        return m_glyphs[glyphID];
    }


    [[nodiscard]] auto size() const noexcept -> size_t {
        return m_glyphs.size();
    }
};
