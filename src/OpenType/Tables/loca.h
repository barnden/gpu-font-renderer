#pragma once

#include <arpa/inet.h>
#include <fstream>
#include <memory>

#include "OpenType/Defines.h"
#include "OpenType/Tables/Table.h"

class IndexToLocation : public Table {
    // https://learn.microsoft.com/en-us/typography/opentype/spec/loca

public:
    static constexpr TableTag g_identifier { 'l', 'o', 'c', 'a' };

private:
    size_t m_stride;
    size_t m_size;
    std::shared_ptr<char[]> m_data;

public:
    IndexToLocation(size_t stride, u16 numGlyphs)
        : m_stride(stride)
        , m_size(numGlyphs + 1)
        , m_data(std::make_shared<char[]>(m_size * stride)) { };

    virtual auto read(std::ifstream& file) -> bool override
    {
        u32 last = 0;
        bool error = false;

        [&]<size_t... I>(std::integer_sequence<size_t, I...>) {
            ([&]<size_t i>() {
                if (m_stride != i)
                    return;

                using DataType = std::conditional<i == 16, u16, u32>::type;
                DataType* data = &reinterpret_cast<DataType*>(m_data.get())[i];
                file.read(reinterpret_cast<char*>(data), sizeof(DataType));

                if constexpr (i == 16) {
                    *data = ntohs(*data);
                } else {
                    *data = ntohl(*data);
                }

                // The offsets are monotonically increasing
                if (last > *data) {
                    error = true;
                    return;
                }

                last = *data;
            }.template operator()<I>(),
             ...);
        }(std::integer_sequence<size_t, 16, 32> {});

        return !error;
    }

    [[nodiscard]] auto operator[](size_t idx) const -> u32
    {
        /**
         * Offsets must be in ascending order, with loca[n] <= loca[n+1].
         * The length of each glyph description is determined by the difference
         * between two consecutive entries: loca[n+1] - loca[n]. To compute the
         * length of the last glyph description, there is an extra entry in the
         * offsets array after the entry for the last valid glyph ID. Thus, the
         * number of elements in the offsets array is numGlyphs + 1.
         */
        if (idx > size())
            throw std::runtime_error("OOB access in IndexToLocation");

        if (m_stride == 16) {
            // The local offset divided by 2 is stored
            return 2 * reinterpret_cast<u16*>(m_data.get())[idx];
        } else {
            // The actual local offset is stored.
            return reinterpret_cast<u32*>(m_data.get())[idx];
        }
    }

    [[nodiscard]] constexpr auto size() const -> size_t
    {
        return m_size;
    }

    [[nodiscard]] auto to_string() const noexcept -> std::string
    {
        return std::format("IndexToLocation(stride: {}, size: {})",
                           m_stride,
                           m_size);
    }
};
