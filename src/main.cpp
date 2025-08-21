#include "OpenType/OpenType.h"

#include <cstdlib>
#include <print>

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::println(std::cerr, "Provide a path to a OpenType font file");
        return EXIT_FAILURE;
    }

    auto font = OpenType(std::string { argv[1] });

    if (!font.valid())
        return EXIT_FAILURE;

    // Print out TableDirectory
    std::println("{}", font);

    // Print out relevant tables
    std::println("{}", *font.get<Head>());
    std::println("{}", *font.get<MaximumProfile>());
    std::println("{}", *font.get<IndexToLocation>());

    auto const& glyf = *font.get<GlyphData>();
    std::println("{}", glyf);

    auto const& cmap = *font.get<CharacterMap>();
    std::println("{}", cmap);
    {
        u16 chr = u'\u01FD';
        std::println("Character '{}' maps to glyph {}.", chr, cmap.map(chr));
        auto glyph = glyf[cmap.map(chr)];

        if (glyph != nullptr) {
            std::println("{}", *glyph);

            for (auto&& [i, contour] : std::views::enumerate(glyph->contours()))
                std::println("\tContour {}: {}", i, contour);
        }
    }

    std::println("{}", *font.get<HorizontalHeader>());
}
