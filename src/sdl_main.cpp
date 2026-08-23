#ifdef __ANDROID__
#include <GLES3/gl3.h>
#include <jni.h>
#include <android/native_activity.h>
#else
#include <GL/gl.h>
#endif

#include <SDL3/SDL.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tiny_gltf.h>

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

const char* vertex_shader = R"(
    #version 300 es
    precision highp float;

    layout(location = 0) in vec3 position;
    layout(location = 1) in vec3 normal;

    uniform mat4 projection;
    uniform mat4 view;
    uniform mat4 model;

    out vec3 v_normal;

    void main()
    {
        gl_Position = projection * view * model * vec4(position, 1.0);
        v_normal = normalize(mat3(model) * normal);
    }
)";

const char* fragment_shader = R"(
    #version 300 es
    precision highp float;

    in vec3 v_normal;
    out vec4 fragColor;

    void main()
    {
        vec3 light = normalize(vec3(1.0, 1.0, 1.0));
        float diff = max(dot(v_normal, light), 0.3);
        fragColor = vec4(vec3(0.8) * diff, 1.0);
    }
)";

struct Mesh
{
    GLuint vao{0};
    GLuint vbo{0};
    GLuint ebo{0};
    uint32_t index_count{0};
};

struct Camera
{
    glm::vec3 position{0.0f, 0.0f, 5.0f};
    glm::vec3 target{0.0f, 0.0f, 0.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
};

GLuint compile_shader(const char* source, GLenum type)
{
    GLuint shader = glCreateShader(type);

    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (!success)
    {
        GLchar info_log[1024]{};
        glGetShaderInfoLog(
            shader,
            sizeof(info_log),
            nullptr,
            info_log
        );

        std::cerr
            << "Shader compilation failed: "
            << info_log
            << std::endl;

        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

GLuint create_program()
{
    GLuint vertex = compile_shader(
        vertex_shader,
        GL_VERTEX_SHADER
    );

    GLuint fragment = compile_shader(
        fragment_shader,
        GL_FRAGMENT_SHADER
    );

    if (vertex == 0 || fragment == 0)
    {
        return 0;
    }

    GLuint program = glCreateProgram();

    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);

    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    if (!success)
    {
        GLchar info_log[1024]{};

        glGetProgramInfoLog(
            program,
            sizeof(info_log),
            nullptr,
            info_log
        );

        std::cerr
            << "Program linking failed: "
            << info_log
            << std::endl;

        glDeleteProgram(program);
        program = 0;
    }

    glDeleteShader(vertex);
    glDeleteShader(fragment);

    return program;
}

Mesh load_gltf_mesh(const std::string& filepath)
{
    Mesh mesh;

    tinygltf::Model model;
    tinygltf::TinyGLTF loader;

    std::string error;
    std::string warning;

    bool success = false;

    if (filepath.size() >= 4 &&
        filepath.substr(filepath.size() - 4) == ".glb")
    {
        success = loader.LoadBinaryFromFile(
            &model,
            &error,
            &warning,
            filepath
        );
    }
    else
    {
        success = loader.LoadASCIIFromFile(
            &model,
            &error,
            &warning,
            filepath
        );
    }

    if (!warning.empty())
    {
        std::cerr
            << "glTF warning: "
            << warning
            << std::endl;
    }

    if (!success)
    {
        std::cerr
            << "Failed to load model: "
            << error
            << std::endl;

        return mesh;
    }

    if (model.meshes.empty())
    {
        std::cerr
            << "Model contains no meshes."
            << std::endl;

        return mesh;
    }

    const auto& gltf_mesh = model.meshes[0];

    if (gltf_mesh.primitives.empty())
    {
        std::cerr
            << "Mesh contains no primitives."
            << std::endl;

        return mesh;
    }

    const auto& primitive = gltf_mesh.primitives[0];

    auto position_attribute =
        primitive.attributes.find("POSITION");

    if (position_attribute == primitive.attributes.end())
    {
        std::cerr
            << "Mesh has no POSITION attribute."
            << std::endl;

        return mesh;
    }

    const auto& position_accessor =
        model.accessors[position_attribute->second];

    const auto& position_view =
        model.bufferViews[position_accessor.bufferView];

    const auto& position_buffer =
        model.buffers[position_view.buffer];

    const size_t position_offset =
        position_view.byteOffset +
        position_accessor.byteOffset;

    const float* position_data =
        reinterpret_cast<const float*>(
            position_buffer.data.data() +
            position_offset
        );

    std::vector<float> positions(
        position_data,
        position_data +
            position_accessor.count * 3
    );

    std::vector<float> normals(
        positions.size(),
        0.0f
    );

    auto normal_attribute =
        primitive.attributes.find("NORMAL");

    if (normal_attribute != primitive.attributes.end())
    {
        const auto& normal_accessor =
            model.accessors[normal_attribute->second];

        const auto& normal_view =
            model.bufferViews[normal_accessor.bufferView];

        const auto& normal_buffer =
            model.buffers[normal_view.buffer];

        const size_t normal_offset =
            normal_view.byteOffset +
            normal_accessor.byteOffset;

        const float* normal_data =
            reinterpret_cast<const float*>(
                normal_buffer.data.data() +
                normal_offset
            );

        normals.assign(
            normal_data,
            normal_data +
                normal_accessor.count * 3
        );
    }
    else
    {
        for (size_t i = 0; i + 8 < positions.size(); i += 9)
        {
            glm::vec3 a(
                positions[i + 3] - positions[i],
                positions[i + 4] - positions[i + 1],
                positions[i + 5] - positions[i + 2]
            );

            glm::vec3 b(
                positions[i + 6] - positions[i],
                positions[i + 7] - positions[i + 1],
                positions[i + 8] - positions[i + 2]
            );

            glm::vec3 normal =
                glm::normalize(glm::cross(a, b));

            for (int j = 0; j < 3; ++j)
            {
                normals[i + j * 3 + 0] = normal.x;
                normals[i + j * 3 + 1] = normal.y;
                normals[i + j * 3 + 2] = normal.z;
            }
        }
    }

    std::vector<uint32_t> indices;

    if (primitive.indices >= 0)
    {
        const auto& index_accessor =
            model.accessors[primitive.indices];

        const auto& index_view =
            model.bufferViews[index_accessor.bufferView];

        const auto& index_buffer =
            model.buffers[index_view.buffer];

        const size_t index_offset =
            index_view.byteOffset +
            index_accessor.byteOffset;

        const unsigned char* data =
            index_buffer.data.data() +
            index_offset;

        for (size_t i = 0;
             i < index_accessor.count;
             ++i)
        {
            switch (index_accessor.componentType)
            {
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                    indices.push_back(
                        reinterpret_cast<const uint8_t*>(data)[i]
                    );
                    break;

                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                    indices.push_back(
                        reinterpret_cast<const uint16_t*>(data)[i]
                    );
                    break;

                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                    indices.push_back(
                        reinterpret_cast<const uint32_t*>(data)[i]
                    );
                    break;

                default:
                    std::cerr
                        << "Unsupported index type."
                        << std::endl;

                    return mesh;
            }
        }
    }
    else
    {
        for (uint32_t i = 0;
             i < positions.size() / 3;
             ++i)
        {
            indices.push_back(i);
        }
    }

    std::vector<float> vertex_data;

    vertex_data.reserve(
        (positions.size() / 3) * 6
    );

    for (size_t i = 0; i < positions.size(); i += 3)
    {
        vertex_data.push_back(positions[i + 0]);
        vertex_data.push_back(positions[i + 1]);
        vertex_data.push_back(positions[i + 2]);

        vertex_data.push_back(normals[i + 0]);
        vertex_data.push_back(normals[i + 1]);
        vertex_data.push_back(normals[i + 2]);
    }

    mesh.index_count =
        static_cast<uint32_t>(indices.size());

    glGenVertexArrays(1, &mesh.vao);
    glBindVertexArray(mesh.vao);

    glGenBuffers(1, &mesh.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);

    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(
            vertex_data.size() * sizeof(float)
        ),
        vertex_data.data(),
        GL_STATIC_DRAW
    );

    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        6 * sizeof(float),
        nullptr
    );

    glEnableVertexAttribArray(0);

    glVertexAttribPointer(
        1,
        3,
        GL_FLOAT,
        GL_FALSE,
        6 * sizeof(float),
        reinterpret_cast<void*>(
            3 * sizeof(float)
        )
    );

    glEnableVertexAttribArray(1);

    glGenBuffers(1, &mesh.ebo);
    glBindBuffer(
        GL_ELEMENT_ARRAY_BUFFER,
        mesh.ebo
    );

    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(
            indices.size() * sizeof(uint32_t)
        ),
        indices.data(),
        GL_STATIC_DRAW
    );

    glBindVertexArray(0);

    std::cout
        << "Loaded mesh: "
        << positions.size() / 3
        << " vertices, "
        << indices.size()
        << " indices"
        << std::endl;

    return mesh;
}

int main()
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::cerr
            << "SDL initialization failed: "
            << SDL_GetError()
            << std::endl;

        return 1;
    }

#ifdef __ANDROID__
    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_PROFILE_MASK,
        SDL_GL_CONTEXT_PROFILE_ES
    );

    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_MAJOR_VERSION,
        3
    );

    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_MINOR_VERSION,
        0
    );
#else
    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_PROFILE_MASK,
        SDL_GL_CONTEXT_PROFILE_CORE
    );

    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_MAJOR_VERSION,
        3
    );

    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_MINOR_VERSION,
        3
    );
#endif

    SDL_Window* window =
        SDL_CreateWindow(
            "PHENOMENA",
            800,
            600,
            SDL_WINDOW_OPENGL |
            SDL_WINDOW_RESIZABLE
        );

    if (!window)
    {
        std::cerr
            << "Failed to create window: "
            << SDL_GetError()
            << std::endl;

        SDL_Quit();
        return 1;
    }

    SDL_GLContext context =
        SDL_GL_CreateContext(window);

    if (!context)
    {
        std::cerr
            << "Failed to create OpenGL context: "
            << SDL_GetError()
            << std::endl;

        SDL_DestroyWindow(window);
        SDL_Quit();

        return 1;
    }

    SDL_GL_MakeCurrent(window, context);
    SDL_GL_SetSwapInterval(1);

    glEnable(GL_DEPTH_TEST);

    glClearColor(
        0.1f,
        0.1f,
        0.15f,
        1.0f
    );

    GLuint program = create_program();

    if (program == 0)
    {
        SDL_GL_DestroyContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();

        return 1;
    }

    Camera camera;

    Mesh mesh =
        load_gltf_mesh("model.glb");

    bool running = true;

    while (running)
    {
        SDL_Event event;

        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
            {
                running = false;
            }
        }

        int width = 800;
        int height = 600;

        SDL_GetWindowSize(
            window,
            &width,
            &height
        );

        if (height == 0)
        {
            height = 1;
        }

        glViewport(
            0,
            0,
            width,
            height
        );

        glClear(
            GL_COLOR_BUFFER_BIT |
            GL_DEPTH_BUFFER_BIT
        );

        glUseProgram(program);

        glm::mat4 projection =
            glm::perspective(
                glm::radians(45.0f),
                static_cast<float>(width) /
                    static_cast<float>(height),
                0.1f,
                100.0f
            );

        glm::mat4 view =
            glm::lookAt(
                camera.position,
                camera.target,
                camera.up
            );

        glm::mat4 model =
            glm::mat4(1.0f);

        glUniformMatrix4fv(
            glGetUniformLocation(
                program,
                "projection"
            ),
            1,
            GL_FALSE,
            glm::value_ptr(projection)
        );

        glUniformMatrix4fv(
            glGetUniformLocation(
                program,
                "view"
            ),
            1,
            GL_FALSE,
            glm::value_ptr(view)
        );

        glUniformMatrix4fv(
            glGetUniformLocation(
                program,
                "model"
            ),
            1,
            GL_FALSE,
            glm::value_ptr(model)
        );

        if (mesh.vao != 0 &&
            mesh.index_count > 0)
        {
            glBindVertexArray(mesh.vao);

            glDrawElements(
                GL_TRIANGLES,
                static_cast<GLsizei>(
                    mesh.index_count
                ),
                GL_UNSIGNED_INT,
                nullptr
            );

            glBindVertexArray(0);
        }

        SDL_GL_SwapWindow(window);
    }

    if (mesh.vao != 0)
    {
        glDeleteVertexArrays(
            1,
            &mesh.vao
        );
    }

    if (mesh.vbo != 0)
    {
        glDeleteBuffers(
            1,
            &mesh.vbo
        );
    }

    if (mesh.ebo != 0)
    {
        glDeleteBuffers(
            1,
            &mesh.ebo
        );
    }

    glDeleteProgram(program);

    SDL_GL_DestroyContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
