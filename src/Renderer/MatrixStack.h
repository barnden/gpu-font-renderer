#pragma once

#include <cassert>
#include <stack>

#include <glm/fwd.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace renderer {

class MatrixStack {
    std::stack<glm::mat4> m_stack;

public:
    MatrixStack()
    {
        m_stack = std::stack<glm::mat4>();
        m_stack.push(glm::mat4(1.));
    }

    ~MatrixStack() = default;

    void push()
    {
        const auto& top = m_stack.top();
        m_stack.push(top);

        assert(m_stack.size() < 100);
    }

    void pop()
    {
        assert(!m_stack.empty());
        m_stack.pop();
        assert(!m_stack.empty());
    }

    void load_identity()
    {
        m_stack.top() = glm::mat4(1.);
    }

    void multiply(glm::mat4 const& matrix)
    {
        m_stack.top() *= matrix;
    }

    void translate(glm::vec3 const& t) noexcept
    {
        m_stack.top() *= glm::translate(glm::mat4(1.0f), t);
    }

    void translate(float x, float y, float z) noexcept
    {
        translate(glm::vec3(x, y, z));
    }

    void scale(glm::vec3 const& s) noexcept
    {
        m_stack.top() *= glm::scale(glm::mat4(1.), s);
    }

    void scale(float x, float y, float z) noexcept
    {
        scale(glm::vec3(x, y, z));
    }

    void scale(float s) noexcept
    {
        scale(glm::vec3(s, s, s));
    }

    void rotate(float angle, glm::vec3 const& axis) noexcept
    {
        m_stack.top() *= glm::rotate(glm::mat4(1.f), angle, axis);
    }

    void rotate(float theta, float x, float y, float z) noexcept
    {
        rotate(theta, glm::vec3(x, y, z));
    }

    [[nodiscard]] glm::mat4& top() noexcept
    {
        return m_stack.top();
    }
};

}
