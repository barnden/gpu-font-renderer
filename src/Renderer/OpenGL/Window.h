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
    struct WindowState {
        std::bitset<128> keys = 0;
        std::bitset<128> toggled_keys = 0;
        std::function<bool(GLFWwindow*, int, int, int)> mouse_button = nullptr;
        std::function<bool(GLFWwindow*, double, double)> mouse_move = nullptr;
        std::function<bool(GLFWwindow*, int, int)> resize = nullptr;
        mutable bool update = true;
    };

    static inline std::unordered_map<GLFWwindow*, WindowState> s_data;

    GLFWwindow* m_window;

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
            s_data[window].keys.set(key, 1);
            s_data[window].toggled_keys.flip(key);
        }

        if (action == GLFW_RELEASE) {
            s_data[window].keys.set(key, 0);
        }

        s_data[window].update = true;
    }

    static constexpr auto mouse_button_callback(GLFWwindow* window,
                                                int a,
                                                int b,
                                                int c) -> void
    {
        s_data[window].update = s_data[window].mouse_button(window, a, b, c);
    }

    static constexpr auto mouse_move_callback(GLFWwindow* window,
                                              double x,
                                              double y) -> void
    {
        s_data[window].update = s_data[window].mouse_move(window, x, y);
    }

    static constexpr auto window_resize_callback(GLFWwindow* window, int width, int height)
    {
        s_data[window].update = s_data[window].resize(window, width, height);
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

    auto on_resize(std::function<bool(GLFWwindow*, int, int)> callback)
    {
        s_data[m_window].resize = callback;
        glfwSetWindowSizeCallback(m_window, window_resize_callback);
    }

    auto on_mouse_button(std::function<bool(GLFWwindow*, int, int, int)> callback)
    {
        s_data[m_window].mouse_button = callback;
        glfwSetMouseButtonCallback(m_window, mouse_button_callback);
    }

    auto on_mouse_move(std::function<bool(GLFWwindow*, double, double)> callback)
    {
        s_data[m_window].mouse_move = callback;
        glfwSetCursorPosCallback(m_window, mouse_move_callback);
    }

    [[nodiscard]] auto keys() const noexcept -> std::bitset<128> const&
    {
        return s_data[m_window].keys;
    }

    [[nodiscard]] auto toggled_keys() const noexcept -> std::bitset<128> const&
    {
        return s_data[m_window].toggled_keys;
    }

    [[nodiscard]] auto data() const noexcept -> WindowState const&
    {
        return s_data[m_window];
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
