#pragma once

#include <arpa/inet.h>
#include <fstream>
#include <iostream>
#include <print>

#include "OpenType/Defines.h"
#include "OpenType/Tables/Table.h"

class Head : public Table {
    // https://learn.microsoft.com/en-us/typography/opentype/spec/head

public:
    static constexpr TableTag g_identifier { 'h', 'e', 'a', 'd' };

private:
    u16 majorVersion;
    u16 minorVersion;

    Fixed fontRevision;

    u32 checksumAdjustment;
    u32 magicNumber;

    /**
     *  FIXME: Ideally this should be a bitset but...
     *         the spec doesn't give names to the individual bits
     *         and I am too lazy to give them names.
     */

    u16 flags;
    u16 unitsPerEm;

    i64 created;
    i64 modified;

    i16 xMin;
    i16 yMin;
    i16 xMax;
    i16 yMax;

    u16 macStyle;
    u16 lowestRecPPEM;
    i16 fontDirectionHint;
    i16 indexToLocFormat;
    i16 glyphDataFormat;

    friend class OpenType;

public:
    virtual auto read(std::ifstream& file) -> bool override
    {
        file.read(reinterpret_cast<char*>(&majorVersion), sizeof(u16));
        majorVersion = ntohs(majorVersion);

        file.read(reinterpret_cast<char*>(&minorVersion), sizeof(u16));
        minorVersion = ntohs(minorVersion);

        file.read(reinterpret_cast<char*>(&fontRevision.data), sizeof(u32));
        fontRevision.data = ntohl(fontRevision.data);

        file.read(reinterpret_cast<char*>(&checksumAdjustment), sizeof(u32));
        checksumAdjustment = ntohl(checksumAdjustment);

        file.read(reinterpret_cast<char*>(&magicNumber), sizeof(u32));
        magicNumber = ntohl(magicNumber);

        if (magicNumber != 0x5F0F3CF5) {
            std::println(std::cerr, "Supplied magic number 0x{:08X} does not match expected value.", magicNumber);

            return false;
        }

        // Reflection can't come fast enough
        constexpr auto fields = std::make_tuple(
            &Head::flags,
            &Head::unitsPerEm,
            &Head::created,
            &Head::modified,
            &Head::xMin,
            &Head::yMin,
            &Head::xMax,
            &Head::yMax,
            &Head::macStyle,
            &Head::lowestRecPPEM,
            &Head::fontDirectionHint,
            &Head::indexToLocFormat,
            &Head::glyphDataFormat);

        std::apply([&](auto&&... members) {
            ([&](auto member) {
                using MemberType = decltype(this->*member);
                file.read(reinterpret_cast<char*>(&(this->*member)), sizeof(MemberType));

                if constexpr (sizeof(MemberType) == 2) {
                    this->*member = ntohs(this->*member);
                } else if constexpr (sizeof(MemberType) == 4) {
                    this->*member = ntohl(this->*member);
                } else if constexpr (sizeof(MemberType) == 8) {
                    this->*member = be64toh(this->*member);
                } else {
                    ASSERT_NOT_REACHED;
                }
            }(members),
             ...);
        },
                   fields);

        return true;
    }

    [[nodiscard]] auto to_string() const noexcept -> std::string
    {
        return std::format("Head(version: {}.{}, fontRevision: {}, min: ({}; {}), max: ({}, {}), indexToLocFormat: {})",
                           majorVersion,
                           minorVersion,
                           fontRevision.value(),
                           xMin, yMin,
                           xMax, yMax,
                           indexToLocFormat);
    }
};
