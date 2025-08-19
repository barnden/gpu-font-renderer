#include "OpenType/OpenType.h"
#include "OpenType/Tables.h"

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

    std::println("{}", *font.get<GlyphData>());
    for (auto i = 0uz; i < 10uz; i++) {
        std::println("\t{}", font.get<GlyphData>()->headers()[i]);
    }
}
