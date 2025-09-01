#pragma once

#include <GL/glew.h>
#include <fstream>
#include <functional>
#include <iostream>
#include <print>

namespace renderer {
class BaseLocation {
protected:
    size_t m_location;

public:
    BaseLocation(size_t location)
        : m_location(location) { };

    auto operator*() const -> size_t
    {
        return m_location;
    }
};

struct UniformLocation : public BaseLocation {
    UniformLocation(size_t location)
        : BaseLocation(location) { };
};

struct AttributeLocation : public BaseLocation {
    AttributeLocation(size_t location)
        : BaseLocation(location) { };
};

constexpr auto operator""_a(char const* data, size_t) -> AttributeLocation
{
    static std::hash<std::string> s_hash;
    return { s_hash(std::string(data)) };
}

constexpr auto operator""_u(char const* data, size_t) -> UniformLocation
{
    static std::hash<std::string> s_hash;
    return { s_hash(std::string(data)) };
}

namespace utils {
    [[nodiscard]] auto read_file(std::string const& filepath) -> char const*
    {
        auto stream = std::ifstream(filepath);

        if (!stream) {
            std::cout << "Failed to open " << filepath << std::endl;
            std::println(
                std::cout,
                "Failed to open {}",
                filepath);
        }

        stream.seekg(0, std::ios::end);

        size_t size = stream.tellg();

        auto buffer = new char[size + 1];
        stream.seekg(0, std::ios::beg);
        stream.read(&buffer[0], size);
        buffer[size] = 0;

        return buffer;
    }

    template <typename T>
        requires requires(T x) { x.bind(); } && requires(T x) { x.unbind(); }
    class Lock {
        T const& m_object;

    public:
        Lock(T const& object)
            : m_object(object)
        {
            m_object.bind();
        };

        ~Lock()
        {
            m_object.unbind();
        }
    };
}

}
