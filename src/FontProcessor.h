#pragma once

#include "OpenType/Defines.h"
#include "OpenType/OpenType.h"

#include <glm/glm.hpp>

#include <vector>

auto extract_contours(OpenType const& font,
                      std::vector<u32>& index,
                      std::vector<u32>& contours,
                      std::vector<glm::vec2>& points) noexcept -> void
{
    auto const units_per_em = static_cast<float>(font.get<Head>()->units());
    auto const& plyphs = *font.get<GlyphData>();

    index.reserve(plyphs.size() + 1);

    index.push_back(0);
    contours.push_back(0);

    for (auto i = 0uz; i < plyphs.size(); i++) {
        auto const& description = plyphs[i];

        if (description == nullptr) {
            index.push_back(0);
            continue;
        }

        auto const& glyph_contours = description->contours();
        for (auto&& contour : glyph_contours) {
            contours.push_back(contour.size());

            // Probably faster to just reserve() then memcpy from contour to points
            std::transform(
                contour.begin(), contour.end(),
                std::back_inserter(points),
                [](std::pair<float, float> data) {
                    return glm::vec2(data.first, data.second);
                });
        }

        index.push_back(glyph_contours.size());
    }

    std::transform(
        points.begin(), points.end(),
        points.begin(),
        [&units_per_em](glm::vec2 element) {
            return element / units_per_em;
        });

    std::partial_sum(contours.begin(), contours.end(), contours.begin());
    std::partial_sum(index.begin(), index.end(), index.begin());
}
