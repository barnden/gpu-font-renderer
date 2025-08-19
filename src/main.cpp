#include "OpenType/OpenType.h"
#include "OpenType/Tables/cmap.h"

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
        std::println("Character '{}' maps to glyph {}.", 'a', cmap.map('a'));
        auto glyph = glyf[cmap.map('a')];

        if (glyph != nullptr)
            std::println("\t{}", *glyph);
    }

    for (auto i = 0; i < 10; i++) {
        auto glyph = glyf[i];

        if (glyph != nullptr)
            std::println("\t{}", *glyph);
        else {
            std::println("\tGlyph {} has no data.", i);
        }
    }
}
