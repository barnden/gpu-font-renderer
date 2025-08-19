#pragma once

#include <fstream>
#include <print>
#include <string>
#include <map>

#include "Defines.h"
#include "TableRecord.h"

struct TableDirectory {
    u32 sfntVersion;
    u16 numTables;
    u16 searchRange;
    u16 entrySelector;
    u16 rangeShift;
    mutable std::map<TableTag, TableRecord> tableRecords;

    [[nodiscard]] auto to_string() const noexcept -> std::string
    {
        auto result = std::format("TableDirectory(sfntVersion: 0x{:08X}, numTables: {}, searchRange: {}, entrySelector: {}, rangeShift: {})\n",
                                  sfntVersion,
                                  numTables,
                                  searchRange,
                                  entrySelector,
                                  rangeShift);

        for (auto&& [_tag, record] : tableRecords) {
            (void)_tag;
            result += std::format("\t{}\n", record.to_string());
        }

        return result;
    }
    [[nodiscard]] auto operator[](TableTag const& tag) const noexcept -> TableRecord const&
    {
        return tableRecords[tag];
    }

    void read(std::ifstream& file)
    {
        file.seekg(0, std::ios::end);
        size_t file_size = file.tellg();
        file.seekg(0, std::ios::beg);

        // Must be at least 12 bytes for TableDirectory
        if (file_size < 12) {
            std::println(R"(File size {} is too small to contain OpenType data.)", file_size);
            return;
        }

        file.read(reinterpret_cast<char*>(&sfntVersion), sizeof(u32));
        sfntVersion = ntohl(sfntVersion);

        // sfntVersion must be 0x00010000 or "OTTO"
        if (sfntVersion != 0x00010000 && sfntVersion != 0x4F54544F) {
            std::println(R"(Invalid sfntVersion "0x{:08X}".)", sfntVersion);
            return;
        }

        for (auto&& field : { &numTables,
                              &searchRange,
                              &entrySelector,
                              &rangeShift }) {
            file.read(reinterpret_cast<char*>(field), sizeof(u16));
            *field = ntohs(*field);
        }

        if (file_size < 12 + numTables * sizeof(TableRecord)) {
            std::println(R"(File size {} is too small to contain the specified number of TableRecords.)", file_size);
            return;
        }

        for (auto i = 0; i < numTables; i++) {
            TableRecord record {};

            if (record.read(file))
                tableRecords[record.tableTag] = std::move(record);
        }
    }
};
