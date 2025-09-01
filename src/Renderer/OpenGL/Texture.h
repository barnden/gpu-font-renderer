#pragma once

#include "OpenType/Defines.h"
#include <GL/glew.h>

#include "Renderer/OpenGL/Utils.h"

namespace renderer {
class Texture {
    GLuint m_tid;
    GLuint m_type;

public:
    Texture(GLuint type)
        : m_tid(0)
        , m_type(type)
    {
        glGenTextures(1, &m_tid);
    }

    template <typename T, typename... Args>
    auto parameter(Args...) const noexcept -> void
    {
        ASSERT_NOT_REACHED;
    }

    template <int, typename... Args>
    auto parameter(Args... args) const noexcept -> void
    {
        utils::Lock lock(*this);
        glTexParameteri(m_type, std::forward<Args>(args)...);
    }

    auto bind() const -> void
    {
        glBindTexture(m_type, m_tid);
    }

    auto unbind() const -> void
    {
        glBindTexture(m_type, 0);
    }

    template<typename... Args>
    auto load_image(Args... args) const -> void {
        if (m_type != GL_TEXTURE_2D)
            return;

        utils::Lock lock(*this);
        glTexImage2D(m_type, std::forward(args)...);
    }

    auto attach(size_t unit, GLuint uniform) const -> void
    {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(m_type, m_tid);
        glUniform1i(uniform, unit);
    }

    [[nodiscard]] auto id() const noexcept -> GLuint
    {
        return m_tid;
    }

    [[nodiscard]] auto type() const noexcept -> GLuint
    {
        return m_type;
    }
};

}
