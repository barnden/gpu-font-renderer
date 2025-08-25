#pragma once

#include <memory>

#include "Renderer/MatrixStack.h"
#include <glm/glm.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

namespace renderer {

class Camera {
    float m_aspect;
    float m_fov_y;
    float m_z_near;
    float m_z_far;
    glm::vec2 m_rotation;
    glm::vec3 m_translation;
    glm::vec2 m_mouse_prev;
    int m_state;
    float m_rfactor;
    float m_tfactor;
    float m_sfactor;

public:
    enum {
        ROTATE = 0,
        TRANSLATE,
        SCALE
    };

    Camera()
        : m_aspect(1.0f)
        , m_fov_y(glm::radians(45.f))
        , m_z_near(0.1f)
        , m_z_far(1000.0f)
        , m_rotation(0.0f, -3.14159265f / 2.f)
        , m_translation(0.0f, -2.0f, -5.0f)
        , m_rfactor(0.01f)
        , m_tfactor(0.001f)
        , m_sfactor(0.005f) { };
    ~Camera() = default;

    void set_init_distance(float z) { m_translation.z = -std::abs(z); }
    void set_aspect_ratio(float a) { m_aspect = a; };
    void set_fov_y(float f) { m_fov_y = f; };
    void set_z_near(float z) { m_z_near = z; };
    void set_z_far(float z) { m_z_far = z; };
    void set_rotation(float f) { m_rfactor = f; };
    void set_translation(float f) { m_tfactor = f; };
    void set_scale(float f) { m_sfactor = f; };

    void mouse_clicked(float x, float y, bool shift, bool ctrl, bool alt)
    {
        (void)alt;
        m_mouse_prev.x = x;
        m_mouse_prev.y = y;
        if (shift) {
            m_state = Camera::TRANSLATE;
        } else if (ctrl) {
            m_state = Camera::SCALE;
        } else {
            m_state = Camera::ROTATE;
        }
    }

    void mouse_moved(float x, float y)
    {
        glm::vec2 mouseCurr(x, y);
        glm::vec2 dv = mouseCurr - m_mouse_prev;
        switch (m_state) {
        case Camera::ROTATE:
            m_rotation += m_rfactor * dv;
            break;
        case Camera::TRANSLATE:
            m_translation.x -= m_translation.z * m_tfactor * dv.x;
            m_translation.y += m_translation.z * m_tfactor * dv.y;
            break;
        case Camera::SCALE:
            m_translation.z *= (1.0f - m_sfactor * dv.y);
            break;
        }
        m_mouse_prev = mouseCurr;
    }

    void apply_projection_matrix(MatrixStack& P) const
    {
        P.multiply(glm::perspective(m_fov_y, m_aspect, m_z_near, m_z_far));
    }

    void apply_view_matrix(MatrixStack& MV) const
    {
        MV.translate(m_translation);
        MV.rotate(m_rotation.y, glm::vec3(1.0f, 0.0f, 0.0f));
        MV.rotate(m_rotation.x, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    void apply_projection_matrix(std::shared_ptr<MatrixStack> const& P) const
    {
        P->multiply(glm::perspective(m_fov_y, m_aspect, m_z_near, m_z_far));
    }

    void apply_view_matrix(std::shared_ptr<MatrixStack> const& MV) const
    {
        MV->translate(m_translation);
        MV->rotate(m_rotation.y, glm::vec3(1.0f, 0.0f, 0.0f));
        MV->rotate(m_rotation.x, glm::vec3(0.0f, 1.0f, 0.0f));
    }
};

}
