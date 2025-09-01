#pragma once

#include <GL/glew.h>

#include "Renderer/OpenGL/Texture.h"
#include "Renderer/OpenGL/Utils.h"

namespace renderer {
class Framebuffer {
    GLuint m_fid;

public:
    Framebuffer()
    {
        glGenFramebuffers(1, &m_fid);
    }

    void attach(Texture const& texture, GLuint attachment)
    {
        utils::Lock _self(*this);
        utils::Lock _texture(*this);

        switch (texture.type()) {
        case GL_TEXTURE_2D:
            glFramebufferTexture2D(
                GL_FRAMEBUFFER,
                attachment,
                GL_TEXTURE_2D,
                texture.id(),
                0);
            break;
        default:
            return;
        }
    }

    auto bind() const -> void
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_fid);
    }

    auto unbind() const -> void
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
};
}
