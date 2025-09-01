#pragma once

#include <GL/glew.h>
#include <string>
#include <unordered_map>

#include "Renderer/OpenGL/Utils.h"

namespace renderer {
class Program {
    GLuint m_pid;
    std::unordered_map<size_t, GLuint> m_attributes;
    std::unordered_map<size_t, GLuint> m_uniform;

    struct {
        GLuint geometry = 0;
        GLuint vertex = 0;
        GLuint fragment = 0;
    } m_shaders;

public:
    Program()
        : m_pid(0) { };
    Program(std::string const& vertex, std::string const& fragment)
        : m_pid(0)
    {
        load_shaders(vertex, fragment);
    }

    auto load_shaders(std::string const& vertex,
                      std::string const& fragment) -> bool
    {
        if (m_shaders.vertex || m_shaders.fragment)
            return false;

        GLint rc = 0;

        m_shaders.vertex = glCreateShader(GL_VERTEX_SHADER);
        m_shaders.fragment = glCreateShader(GL_FRAGMENT_SHADER);

        auto const* vertex_code = utils::read_file(vertex);
        auto const* fragment_code = utils::read_file(fragment);

        glShaderSource(m_shaders.vertex, 1, &vertex_code, NULL);
        glShaderSource(m_shaders.fragment, 1, &fragment_code, NULL);

        for (auto [shader, name] : {
                 std::make_tuple(m_shaders.vertex, vertex),
                 std::make_tuple(m_shaders.fragment, fragment),
             }) {
            glCompileShader(shader);
            glGetShaderiv(shader, GL_COMPILE_STATUS, &rc);

            if (!rc) {
                static GLchar info[1024];
                glGetShaderInfoLog(shader, sizeof(info) - 1, NULL, info);

                std::println(
                    std::cerr,
                    "Failed to compile shader \"{}\"\n{}",
                    name, info);

                return false;
            }
        }

        m_pid = glCreateProgram();
        glAttachShader(m_pid, m_shaders.vertex);
        glAttachShader(m_pid, m_shaders.fragment);

        glLinkProgram(m_pid);
        glGetProgramiv(m_pid, GL_LINK_STATUS, &rc);

        delete[] vertex_code;
        delete[] fragment_code;

        vertex_code = NULL;
        fragment_code = NULL;

        if (!rc) {
            static GLchar info[1024];
            glGetProgramInfoLog(m_pid, sizeof(info) - 1, NULL, info);
            std::println(
                std::cerr,
                "Failed to link program.\n{}",
                info);

            return false;
        }

        return true;
    }

    auto add_attribute(std::initializer_list<std::string> names) -> void
    {
        for (auto&& name : names) {
            add_attribute(name);
        }
    }

    auto add_attribute(std::string const& name) -> void
    {
        static std::hash<std::string> hash {};
        m_attributes[hash(name)] = glGetAttribLocation(m_pid, name.c_str());
    }

    auto add_uniform(std::initializer_list<std::string> names) -> void
    {
        for (auto&& name : names) {
            add_uniform(name);
        }
    }

    auto add_uniform(std::string const& name) -> void
    {
        static std::hash<std::string> hash {};
        m_uniform[hash(name)] = glGetUniformLocation(m_pid, name.c_str());
    }

    auto bind() const -> void
    {
        glUseProgram(m_pid);
    }

    auto unbind() const -> void
    {
        glUseProgram(0);
    }

    [[nodiscard]] auto get(AttributeLocation location) const noexcept -> GLuint
    {
        return m_attributes.at(*location);
    }

    [[nodiscard]] auto get(UniformLocation location) const noexcept -> GLuint
    {
        return m_uniform.at(*location);
    }
};
}
