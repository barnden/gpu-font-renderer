#pragma once

#include "Defines.h"

#include <arpa/inet.h>
#include <format>
#include <fstream>
#include <memory>
#include <numeric>
#include <print>
#include <string>

struct TableRecord {
    TableTag tableTag {};
    u32 checksum {};
    u32 offset {};
    u32 length {};

    auto read(std::ifstream& file) -> bool
    {
        file.read(reinterpret_cast<char*>(tableTag.data()), sizeof(TableTag));
        file.read(reinterpret_cast<char*>(&checksum), sizeof(u32));
        file.read(reinterpret_cast<char*>(&offset), sizeof(u32));
        file.read(reinterpret_cast<char*>(&length), sizeof(u32));

        checksum = ntohl(checksum);
        offset = ntohl(offset);
        length = ntohl(length);

        size_t cursor = file.tellg();

        /**
         * FIXME: This data is being read anyways, we should use it instead
         *        reading the data again in each table's read().
         */
        file.seekg(offset);
        auto data = std::make_shared<char[]>((length + 3) & ~3);
        file.read(reinterpret_cast<char*>(data.get()), length);
        file.seekg(cursor);

        // FIXME: Handle "head" checksum special case.
        if (tableTag != TableTag { 'h', 'e', 'a', 'd' }) {
            auto computed_checksum = compute_checksum(data);

            if (checksum != computed_checksum) {
                std::println(R"(TableRecord "{:s}" checksum 0x{:08X} did not match computed checksum 0x{:08X}.)",
                             tableTag,
                             checksum,
                             computed_checksum);

                return false;
            }
        }

        return true;
    }

    template <typename T>
    [[nodiscard]] auto compute_checksum(std::shared_ptr<T> const& data) const noexcept -> u32
    {
        char* begin = reinterpret_cast<char*>(data.get());
        char* end = &begin[(length + 3) & ~3];

        return std::transform_reduce(
            reinterpret_cast<u32*>(begin),
            reinterpret_cast<u32*>(end),
            0ul,
            std::plus<u32> {},
            ntohl);
    }

    [[nodiscard]] auto to_string() const noexcept -> std::string
    {
        return std::format("TableRecord(tableTag: {:?s}, checksum: 0x{:08X}, offset: {}, length: {})",
                           tableTag,
                           checksum,
                           offset,
                           length);
    }
};
