#pragma once

#include <arpa/inet.h>
#include <format>
#include <fstream>
#include <map>
#include <memory>
#include <print>
#include <string>

#include "Defines.h"

#include "OpenType/Tables/cmap.h"
#include "OpenType/Tables/maxp.h"
#include "TableDirectory.h"
#include "Tables.h"

class OpenType {
    TableDirectory m_directory;
    std::map<TableTag, std::shared_ptr<Table>> m_tables;
    bool m_valid = false;

    template <class Table>
    auto load_table(std::ifstream& file, auto&&... args) -> std::shared_ptr<Table>
    {
        if (!m_directory.contains(Table::g_identifier))
            return nullptr;

        auto table = std::make_shared<Table>(std::forward<decltype(args)>(args)...);

        file.clear();
        file.seekg(m_directory[Table::g_identifier].offset);
        if (!table->read(file))
            return nullptr;

        m_tables[Table::g_identifier] = table;

        return table;
    };

    auto read(std::string const& path) -> bool
    {
        auto file = std::ifstream(path, std::ios::binary);

        if (!file) {
            std::println(R"(Failed to open file: "{}")", path);
            return false;
        }

        m_directory.read(file);

        auto head = load_table<Head>(file);

        if (head == nullptr)
            return false;

        auto maxp = load_table<MaximumProfile>(file);

        if (maxp == nullptr)
            return false;

        auto loca = load_table<IndexToLocation>(file, head->indexToLocFormat == 0 ? 16 : 32, maxp->numGlyphs);

        if (loca == nullptr)
            return false;

        auto glyf = load_table<GlyphData>(file, loca);

        if (glyf == nullptr)
            return false;

        auto cmap = load_table<CharacterMap>(file);

        if (cmap == nullptr)
            return false;

        auto hhea = load_table<HorizontalHeader>(file);

        if (hhea == nullptr)
            return false;

        auto htmx = load_table<HorizontalMetrics>(file, glyf->m_glyphs.size(), hhea->size());

        if (htmx == nullptr)
            return false;

        return true;
    }

public:
    OpenType(std::string const& path)
        : m_tables()
    {
        m_valid = read(path);
    }

    template <typename T>
    [[nodiscard]] auto get() const noexcept -> std::shared_ptr<T>
    {
        static constexpr auto tag = T::g_identifier;

        if (!m_tables.contains(tag))
            return nullptr;

        return std::static_pointer_cast<T>(m_tables.at(tag));
    }

    [[nodiscard]] auto to_string() const noexcept -> std::string
    {
        return std::format("{}", m_directory.to_string());
    }

    [[nodiscard]] auto directory() const noexcept -> TableDirectory const&
    {
        return m_directory;
    }

    [[nodiscard]] auto valid() const noexcept -> bool
    {
        return m_valid;
    }
};
