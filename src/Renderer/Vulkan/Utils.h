#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#ifdef __APPLE__
#    include <vulkan/vulkan_beta.h>
#endif

#include <algorithm>
#include <fstream>
#include <string>
#include <vector>

static constexpr auto read_file(std::string const& filename) -> std::vector<char>
{
    auto file = std::ifstream(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open())
        throw std::runtime_error("Failed to open file.");

    size_t fileSize = file.tellg();
    auto buffer = std::vector<char>(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    return buffer;
}

template <class F>
struct return_type;

template <class R, class... A>
struct return_type<R (*)(A...)> {
    using type = R;
};

template <size_t N>
struct CompileTimeString {
    char data[N];

    consteval CompileTimeString(char const (&str)[N]) { std::copy_n(str, N, data); }
};
