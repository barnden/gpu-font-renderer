#pragma once

#include <arpa/inet.h>
#include <array>
#include <bitset>
#include <cstdint>
#include <cstring>
#include <format>
#include <fstream>
#include <map>
#include <memory>
#include <numeric>
#include <print>
#include <string>
#include <vector>

#include "Defines.h"

#include "TableDirectory.h"
#include "TableRecord.h"
#include "Tables.h"

class OpenType {
    TableDirectory m_directory;
    std::map<TableTag, std::shared_ptr<Table>> m_tables;
    bool m_valid = false;

    auto read(std::string const& path) -> bool
    {
        auto file = std::ifstream(path, std::ios::binary);

        if (!file) {
            std::println(R"(Failed to open file: "{}")", path);
            return false;
        }

        m_directory.read(file);

        auto head = std::make_shared<Head>();

        {
            file.seekg(m_directory[Head::g_identifier].offset);
            if (!head->read(file))
                return false;

            m_tables[Head::g_identifier] = head;
        }

        auto maxp = std::make_shared<MaximumProfile>();
        {
            file.seekg(m_directory[MaximumProfile::g_identifier].offset);
            if (!maxp->read(file))
                return false;

            m_tables[MaximumProfile::g_identifier] = maxp;
        }

        auto loca = std::make_shared<IndexToLocation>(head->indexToLocFormat == 0 ? 16 : 32, maxp->numGlyphs);

        {
            file.seekg(m_directory[IndexToLocation::g_identifier].offset);
            if (!loca->read(file))
                return false;
            m_tables[IndexToLocation::g_identifier] = loca;
        }

        auto glyf = std::make_shared<GlyphData>(loca);

        {
            file.seekg(m_directory[GlyphData::g_identifier].offset);
            if (!glyf->read(file))
                return false;
            m_tables[GlyphData::g_identifier] = glyf;
        }

        return true;
    }

public:
    OpenType(std::string const& path)
        : m_tables()
    {
        m_valid = read(path);
    }

    template <typename T>
    [[nodiscard]] auto get() noexcept -> std::shared_ptr<T>
    {
        static constexpr auto tag = T::g_identifier;

        if (!m_tables.contains(tag))
            return nullptr;

        return std::static_pointer_cast<T>(m_tables[tag]);
    }

    [[nodiscard]] auto to_string() const noexcept -> std::string
    {
        return std::format("{}", m_directory.to_string());
    }

    [[nodiscard]] auto directory() const noexcept -> TableDirectory const&
    {
        return m_directory;
    }

    [[nodiscard]] auto valid() const noexcept -> bool {
        return m_valid;
    }
};
