#pragma once

#include <bitset>
#include <functional>
#include <iostream>
#include <print>
#include <string>
#include <unordered_map>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

namespace renderer {

class Window {
    GLFWwindow* m_window;
    static inline std::unordered_map<GLFWwindow*, std::bitset<128>> s_keys;
    static inline std::unordered_map<GLFWwindow*, std::function<void(GLFWwindow*, int, int, int)>> s_mouse_button;
    static inline std::unordered_map<GLFWwindow*, std::function<void(GLFWwindow*, double, double)>> s_mouse_move;
    static inline std::unordered_map<GLFWwindow*, std::function<void(GLFWwindow*, int, int)>> s_resize;

    static constexpr auto error_callback(auto code, auto desc) -> void
    {
        std::println(std::cerr,
                     "GLFW Error code: {}, desc: {}",
                     code, desc);
    }

    static constexpr auto key_callback(GLFWwindow* window,
                                       int key,
                                       int scancode,
                                       int action,
                                       int mods) -> void
    {
        (void)mods;
        (void)scancode;

        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GL_TRUE);

            return;
        }

        if (key > 127)
            return;

        if (action == GLFW_PRESS) {
            s_keys[window].set(key, 1);
        }

        if (action == GLFW_RELEASE) {
            s_keys[window].set(key, 0);
        }
    }

    static constexpr auto mouse_button_callback(GLFWwindow* window,
                                                int a,
                                                int b,
                                                int c) -> void
    {
        s_mouse_button[window](window, a, b, c);
    }

    static constexpr auto mouse_move_callback(GLFWwindow* window,
                                              double x,
                                              double y) -> void
    {
        s_mouse_move[window](window, x, y);
    }

    static constexpr auto window_resize_callback(GLFWwindow* window, int width, int height)
    {
        s_resize[window](window, width, height);
    }

public:
    Window(std::string const& title,
           int width = 640,
           int height = 480)
    {
        glfwSetErrorCallback(error_callback);

        if (!glfwInit()) {
            std::println(std::cerr, "glfwInit(): error");
            exit(EXIT_FAILURE);
        }

        m_window = glfwCreateWindow(width, height, title.c_str(), NULL, NULL);

        glfwMakeContextCurrent(m_window);
        glewExperimental = GL_TRUE;

        if (glewInit() != GLEW_OK) {
            std::println(std::cerr, "glewInit(): error");
            glfwTerminate();
            exit(EXIT_FAILURE);
        }

        glGetError();

        static_assert(sizeof(GLubyte) == sizeof(char));
        std::println("OpenGL Version: {}", reinterpret_cast<char const*>(glGetString(GL_VERSION)));
        std::println("  GLSL Version: {}", reinterpret_cast<char const*>(glGetString(GL_SHADING_LANGUAGE_VERSION)));

        glfwSwapInterval(1);
        glfwSetKeyCallback(m_window, key_callback);
    }

    ~Window()
    {
        glfwDestroyWindow(m_window);
        glfwTerminate();
    }

    auto on_resize(std::function<void(GLFWwindow*, int, int)> callback)
    {
        s_resize[m_window] = callback;
        glfwSetWindowSizeCallback(m_window, window_resize_callback);
    }

    auto on_mouse_button(std::function<void(GLFWwindow*, int, int, int)> callback)
    {
        s_mouse_button[m_window] = callback;
        glfwSetMouseButtonCallback(m_window, mouse_button_callback);
    }

    auto on_mouse_move(std::function<void(GLFWwindow*, double, double)> callback)
    {
        s_mouse_move[m_window] = callback;
        glfwSetCursorPosCallback(m_window, mouse_move_callback);
    }

    [[nodiscard]] auto keys() const noexcept -> std::bitset<128> const&
    {
        return s_keys[m_window];
    }

    [[nodiscard]] auto get() const noexcept -> GLFWwindow*
    {
        return m_window;
    }

    template <typename F>
    void render(F&& render_callback)
    {
        while (!glfwWindowShouldClose(m_window)) {
            if (!glfwGetWindowAttrib(m_window, GLFW_ICONIFIED)) {
                render_callback(*this);
                glfwSwapBuffers(m_window);
            }

            glfwPollEvents();
        }
    }
};

}
