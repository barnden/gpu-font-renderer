#ifdef USE_OPENGL
#    include "OpenType/Defines.h"
#    include "OpenType/OpenType.h"

#    include "Renderer/Camera.h"
#    include "Renderer/MatrixStack.h"

#    include "Renderer/OpenGL/Buffer.h"
#    include "Renderer/OpenGL/Program.h"
#    include "Renderer/OpenGL/Window.h"

#    include <GL/glew.h>
#    include <GLFW/glfw3.h>
#    include <glm/glm.hpp>
#    include <glm/gtc/type_ptr.hpp>

#    include <cstdlib>
#    include <numeric>
#    include <print>
#    include <vector>

using namespace renderer;

auto create_buffers(OpenType const& font,
                    Buffer<u32>& index,
                    Buffer<u32>& contours,
                    Buffer<std::pair<float, float>>& points) -> void
{
    auto const units_per_em = static_cast<float>(font.get<Head>()->units());
    auto const& glyphs = *font.get<GlyphData>();
    index.data().reserve(glyphs.size() + 1);

    index.data().push_back(0);
    contours.data().push_back(0);

    for (auto i = 0uz; i < glyphs.size(); i++) {
        auto const& description = glyphs[i];

        if (description == nullptr) {
            index.data().push_back(0);
            continue;
        }

        auto const& glyph_contours = description->contours();
        for (auto&& contour : glyph_contours) {
            contours.data().push_back(contour.size());
            std::copy(contour.begin(), contour.end(), std::back_inserter(points.data()));
        }

        index.data().push_back(glyph_contours.size());
    }

    std::transform(
        points.data().begin(), points.data().end(),
        points.data().begin(),
        [&units_per_em](std::pair<float, float> element) {
            element.first /= units_per_em;
            element.second /= units_per_em;

            return element;
        });

    std::partial_sum(contours.data().begin(), contours.data().end(), contours.data().begin());
    std::partial_sum(index.data().begin(), index.data().end(), index.data().begin());

    index.update();
    contours.update();
    points.update();
}

void add_glyph(u32 glyph_id,
               Buffer<glm::vec3>& positions,
               Buffer<glm::vec2>& texcoords,
               Buffer<u32>& glyphs,
               glm::vec2 min,
               glm::vec2 max,
               glm::vec2 advance = { 0.0, 0.0 },
               bool update = true,
               float epsilon = 1. / 32.)
{
    std::initializer_list<glm::vec3> s_position = {
        glm::vec3 { min.s, 0., min.t } + glm::vec3 { -epsilon, 0, -epsilon },
        glm::vec3 { min.s, 0., max.t } + glm::vec3 { -epsilon, 0, +epsilon },
        glm::vec3 { max.s, 0., min.t } + glm::vec3 { +epsilon, 0, -epsilon },
        glm::vec3 { max.s, 0., min.t } + glm::vec3 { +epsilon, 0, -epsilon },
        glm::vec3 { max.s, 0., max.t } + glm::vec3 { +epsilon, 0, +epsilon },
        glm::vec3 { min.s, 0., max.t } + glm::vec3 { -epsilon, 0, +epsilon },
    };

    std::initializer_list<glm::vec2> s_texcoord = {
        glm::vec2 { min.s, min.t } + glm::vec2 { -epsilon, -epsilon },
        glm::vec2 { min.s, max.t } + glm::vec2 { -epsilon, +epsilon },
        glm::vec2 { max.s, min.t } + glm::vec2 { +epsilon, -epsilon },
        glm::vec2 { max.s, min.t } + glm::vec2 { +epsilon, -epsilon },
        glm::vec2 { max.s, max.t } + glm::vec2 { +epsilon, +epsilon },
        glm::vec2 { min.s, max.t } + glm::vec2 { -epsilon, +epsilon },
    };

    std::copy(s_texcoord.begin(), s_texcoord.end(), std::back_inserter(texcoords.data()));

    for (auto i = 0uz; i < s_position.size(); i++) {
        glyphs.data().push_back(glyph_id);

        glm::vec3 data = *(s_position.begin() + i);
        data += glm::vec3(advance.x, 0., advance.y);
        positions.data().push_back(data);
    }

    if (update) {
        positions.update();
        texcoords.update();
        glyphs.update();
    }
}

void add_glyphs(OpenType const& font,
                std::string const& string,
                Buffer<glm::vec3>& positions,
                Buffer<glm::vec2>& texcoords,
                Buffer<u32>& glyphs)
{
    auto const units_per_em = static_cast<float>(font.get<Head>()->units());
    auto const& cmap = *font.get<CharacterMap>();
    auto const& hmtx = *font.get<HorizontalMetrics>();
    auto const& glyf = *font.get<GlyphData>();

    auto advance = glm::vec2(0.);
    for (auto&& chr : string) {
        auto glyph_id = cmap.map(chr);

        float width = 0.f;

        if (auto metrics = hmtx[glyph_id]) {
            if (metrics->advanceWidth == 0xFFFF) {
                // FIXME: Handle the case where advanceWidth is unset.
                ASSERT_NOT_REACHED;
            } else {
                width = metrics->advanceWidth / units_per_em;
            }
        } else {
            std::println(
                std::cerr,
                "Failed to find glyph \\u{:04X} in hmtx.",
                glyph_id);

            exit(EXIT_FAILURE);
        }

        if (auto glyph = glyf[glyph_id]) {
            auto header = glyph->header();
            add_glyph(glyph_id,
                      positions,
                      texcoords,
                      glyphs,
                      glm::vec2(header.min().first, header.min().second) / units_per_em,
                      glm::vec2(header.max().first, header.max().second) / units_per_em,
                      advance,
                      false,
                      64. / units_per_em);
        }

        advance.x += width;
    }

    positions.update();
    texcoords.update();
    glyphs.update();
}

auto main(int argc, char** argv) -> int
{
    if (argc < 2) {
        std::println(std::cerr, "Provide a path to a OpenType font file");
        return EXIT_FAILURE;
    }

    std::string string = "Hello, World!";
    if (argc == 3) {
        string = std::string { argv[2] };
    }

    auto font = OpenType(std::string { argv[1] });

    if (!font.valid())
        return EXIT_FAILURE;

    auto window = Window("Glyph");

    auto index = Buffer<u32>(GL_SHADER_STORAGE_BUFFER);
    auto contours = Buffer<u32>(GL_SHADER_STORAGE_BUFFER);
    auto points = Buffer<std::pair<float, float>>(GL_SHADER_STORAGE_BUFFER);

    create_buffers(font, index, contours, points);

    auto positions = Buffer<glm::vec3>(GL_ARRAY_BUFFER);
    auto texcoords = Buffer<glm::vec2>(GL_ARRAY_BUFFER);
    auto glyphs = Buffer<u32>(GL_ARRAY_BUFFER);

    add_glyphs(font, string, positions, texcoords, glyphs);

    auto camera = Camera();

    auto mouse_move = [&](GLFWwindow* window, double x, double y) {
        auto state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);

        if (state != GLFW_PRESS)
            return false;

        camera.mouse_moved(x, y);
        return true;
    };

    auto mouse_button = [&](GLFWwindow* window,
                            int button,
                            int action,
                            int mods) {
        (void)button;

        if (action != GLFW_PRESS)
            return false;

        double x;
        double y;
        glfwGetCursorPos(window, &x, &y);

        int width;
        int height;
        glfwGetWindowSize(window, &width, &height);

        bool shift = (mods & GLFW_MOD_SHIFT) != 0;
        bool ctrl = (mods & GLFW_MOD_CONTROL) != 0;
        bool alt = (mods & GLFW_MOD_ALT) != 0;
        camera.mouse_clicked(static_cast<float>(x),
                             static_cast<float>(y),
                             shift,
                             ctrl,
                             alt);

        return true;
    };

    auto program = Program("../resources/Glyph.vert", "../resources/Glyph.frag");
    program.add_uniform({ "u_Projection",
                          "u_ModelView" });
    program.add_attribute({ "i_Position",
                            "i_TexCoord",
                            "i_Glyph" });

    auto debug = Program("../resources/Debug.vert", "../resources/Debug.frag");
    debug.add_uniform({ "u_Projection",
                        "u_ModelView" });
    debug.add_attribute({ "i_Position",
                          "i_TexCoord" });

    window.on_mouse_move(mouse_move);
    window.on_mouse_button(mouse_button);
    window.on_resize([](auto...) { return true; });

    // auto last = std::chrono::high_resolution_clock::now();
    // std::println();
    window.render(
        [&](Window const& window) {
            if (!window.data().update)
                return;

            static auto P = MatrixStack();
            static auto MV = MatrixStack();
            glEnable(GL_PROGRAM_POINT_SIZE);
            glEnable(GL_POINT_SPRITE);

            int width = 0;
            int height = 0;
            glfwGetFramebufferSize(window.get(), &width, &height);
            camera.set_aspect_ratio(width / static_cast<float>(height));

            P.push();
            camera.apply_projection_matrix(P);

            MV.push();
            camera.apply_view_matrix(MV);

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glClearColor(0.3, 0.3, 0.6, 1.);
            glViewport(0, 0, width, height);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            if (window.keys()[GLFW_KEY_W] || window.toggled_keys()[GLFW_KEY_Q]) {
                utils::Lock prog_lock(debug);

                if (window.keys()[GLFW_KEY_W])
                    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

                glUniformMatrix4fv(debug.get("u_Projection"_u), 1, GL_FALSE, glm::value_ptr(P.top()));
                glUniformMatrix4fv(debug.get("u_ModelView"_u), 1, GL_FALSE, glm::value_ptr(MV.top()));

                {
                    utils::Lock pos_lock(positions);
                    glEnableVertexAttribArray(debug.get("i_Position"_a));
                    glVertexAttribPointer(debug.get("i_Position"_a), 3, GL_FLOAT, GL_FALSE, 0, 0);
                }

                {
                    utils::Lock tex_lock(texcoords);
                    glEnableVertexAttribArray(debug.get("i_TexCoord"_a));
                    glVertexAttribPointer(debug.get("i_TexCoord"_a), 2, GL_FLOAT, GL_FALSE, 0, 0);
                }

                glDrawArrays(GL_TRIANGLES, 0, positions.data().size());
                glBindBuffer(GL_ARRAY_BUFFER, 0);

                if (window.keys()[GLFW_KEY_W])
                    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            }

            {
                utils::Lock prog_lock(program);

                glUniformMatrix4fv(program.get("u_Projection"_u), 1, GL_FALSE, glm::value_ptr(P.top()));
                glUniformMatrix4fv(program.get("u_ModelView"_u), 1, GL_FALSE, glm::value_ptr(MV.top()));

                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, points.get());
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, contours.get());
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, index.get());

                {
                    utils::Lock pos_lock(positions);
                    glEnableVertexAttribArray(program.get("i_Position"_a));
                    glVertexAttribPointer(program.get("i_Position"_a), 3, GL_FLOAT, GL_FALSE, 0, 0);
                }

                {
                    utils::Lock tex_lock(texcoords);
                    glEnableVertexAttribArray(program.get("i_TexCoord"_a));
                    glVertexAttribPointer(program.get("i_TexCoord"_a), 2, GL_FLOAT, GL_FALSE, 0, 0);
                }

                {
                    utils::Lock glyph_lock(glyphs);
                    glEnableVertexAttribArray(program.get("i_Glyph"_a));
                    glVertexAttribIPointer(program.get("i_Glyph"_a), 1, GL_UNSIGNED_INT, 0, 0);
                }

                glDrawArrays(GL_TRIANGLES, 0, positions.data().size());

                glDisableVertexAttribArray(debug.get("i_Position"_a));
                glDisableVertexAttribArray(debug.get("i_TexCoord"_a));
                glDisableVertexAttribArray(program.get("i_Glyph"_a));

                glBindBuffer(GL_ARRAY_BUFFER, 0);
            }

            MV.pop();
            P.pop();

            window.data().update = false;
        });
}
#endif
