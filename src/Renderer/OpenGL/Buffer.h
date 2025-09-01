#pragma once

#include <GL/glew.h>
#include <vector>

#include "Renderer/OpenGL/Utils.h"

namespace renderer {

template <typename T>
class Buffer {
    GLuint m_bid;
    GLuint m_type;
    std::vector<T> m_data;

public:
    Buffer(GLuint type)
        : m_bid(0)
        , m_type(type)
        , m_data({})
    {
        glGenBuffers(1, &m_bid);
    }

    Buffer(GLuint type, std::vector<T>&& data)
        : m_bid(0)
        , m_type(type)
        , m_data(std::move(data))
    {
        glGenBuffers(1, &m_bid);
        update();
    }

    auto data() -> std::vector<T>&
    {
        return m_data;
    }
    auto update(GLuint draw = GL_STATIC_DRAW) const -> void
    {
        utils::Lock lock(*this);
        glBufferData(m_type, m_data.size() * sizeof(T), m_data.data(), draw);
    }

    auto bind() const -> void
    {
        glBindBuffer(m_type, m_bid);
    }

    auto unbind() const -> void
    {
        glBindBuffer(m_type, 0);
    }

    [[nodiscard]] auto get() const noexcept -> GLuint
    {
        return m_bid;
    }
};

}
