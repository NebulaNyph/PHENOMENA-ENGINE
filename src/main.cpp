#include <SDL3/SDL.h>
#include <SDL3/SDL_opengles2.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <tiny_gltf.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

struct Mesh {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLsizei index_count = 0;
};

struct Camera {
    glm::vec3 position{0.0f, 0.0f, 5.0f};
    glm::vec3 target{0.0f, 0.0f, 0.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
};

static const char* vertex_shader_source = R"(
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

static const char* fragment_shader_source = R"(
#version 300 es
precision highp float;

in vec3 v_normal;
out vec4 fragColor;

void main()
{
    vec3 light = normalize(vec3(1.0, 1.0, 1.0));
    float diffuse = max(dot(normalize(v_normal), light), 0.25);

    fragColor = vec4(vec3(0.8) * diffuse, 1.0);
}
)";

static GLuint compile_shader(const char* source, GLenum type)
{
    GLuint shader = glCreateShader(type);

    if (shader == 0) {
        std::cerr << "Failed to create shader.\n";
        return 0;
    }

    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (success != GL_TRUE) {
        GLchar log[2048]{};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);

        std::cerr << "Shader compilation failed:\n"
                  << log << '\n';

        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

static GLuint create_shader_program()
{
    GLuint vertex_shader =
        compile_shader(vertex_shader_source, GL_VERTEX_SHADER);

    if (vertex_shader == 0) {
        return 0;
    }

    GLuint fragment_shader =
        compile_shader(fragment_shader_source, GL_FRAGMENT_SHADER);

    if (fragment_shader == 0) {
        glDeleteShader(vertex_shader);
        return 0;
    }

    GLuint program = glCreateProgram();

    if (program == 0) {
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        return 0;
    }

    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    GLint success = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    if (success != GL_TRUE) {
        GLchar log[2048]{};
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);

        std::cerr << "Shader program linking failed:\n"
                  << log << '\n';

        glDeleteProgram(program);
        program = 0;
    }

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    return program;
}

static bool load_gltf_model(
    const std::string& filepath,
    tinygltf::Model& model)
{
    tinygltf::TinyGLTF loader;

    std::string warning;
    std::string error;

    bool success = false;

    const std::string extension =
        [&filepath]() {
            const auto dot = filepath.find_last_of('.');

            if (dot == std::string::npos) {
                return std::string{};
            }

            std::string ext = filepath.substr(dot);

            std::transform(
                ext.begin(),
                ext.end(),
                ext.begin(),
                [](unsigned char c) {
                    return static_cast<char>(std::tolower(c));
                });

            return ext;
        }();

    if (extension == ".glb") {
        success = loader.LoadBinaryFromFile(
            &model,
            &error,
            &warning,
            filepath);
    }
    else if (extension == ".gltf") {
        success = loader.LoadASCIIFromFile(
            &model,
            &error,
            &warning,
            filepath);
    }
    else {
        std::cerr
            << "Unsupported model format: "
            << filepath << '\n';

        return false;
    }

    if (!warning.empty()) {
        std::cerr << "glTF warning: "
                  << warning << '\n';
    }

    if (!success) {
        std::cerr << "Failed to load model: "
                  << error << '\n';

        return false;
    }

    return true;
}

static bool read_indices(
    const tinygltf::Model& model,
    const tinygltf::Accessor& accessor,
    std::vector<uint32_t>& indices)
{
    if (accessor.bufferView < 0 ||
        accessor.bufferView >=
            static_cast<int>(model.bufferViews.size())) {
        return false;
    }

    const auto& view = model.bufferViews[accessor.bufferView];

    if (view.buffer < 0 ||
        view.buffer >=
            static_cast<int>(model.buffers.size())) {
        return false;
    }

    const auto& buffer = model.buffers[view.buffer];

    const size_t offset =
        view.byteOffset + accessor.byteOffset;

    if (offset > buffer.data.size()) {
        return false;
    }

    const unsigned char* data =
        buffer.data.data() + offset;

    indices.resize(accessor.count);

    switch (accessor.componentType) {
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        for (size_t i = 0; i < accessor.count; ++i) {
            indices[i] =
                static_cast<uint32_t>(
                    reinterpret_cast<const uint8_t*>(data)[i]);
        }
        break;

    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        for (size_t i = 0; i < accessor.count; ++i) {
            indices[i] =
                static_cast<uint32_t>(
                    reinterpret_cast<const uint16_t*>(data)[i]);
        }
        break;

    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
        std::memcpy(
            indices.data(),
            data,
            accessor.count * sizeof(uint32_t));
        break;

    default:
        std::cerr
            << "Unsupported index component type.\n";
        return false;
    }

    return true;
}

static bool read_vec3_attribute(
    const tinygltf::Model& model,
    const tinygltf::Accessor& accessor,
    std::vector<float>& output)
{
    if (accessor.bufferView < 0 ||
        accessor.bufferView >=
            static_cast<int>(model.bufferViews.size())) {
        return false;
    }

    const auto& view = model.bufferViews[accessor.bufferView];

    if (view.buffer < 0 ||
        view.buffer >=
            static_cast<int>(model.buffers.size())) {
        return false;
    }

    const auto& buffer = model.buffers[view.buffer];

    const size_t offset =
        view.byteOffset + accessor.byteOffset;

    if (offset > buffer.data.size()) {
        return false;
    }

    const unsigned char* data =
        buffer.data.data() + offset;

    output.resize(accessor.count * 3);

    const size_t stride =
        accessor.ByteStride(view);

    if (stride == 0) {
        return false;
    }

    for (size_t i = 0; i < accessor.count; ++i) {
        const float* values =
            reinterpret_cast<const float*>(
                data + i * stride);

        output[i * 3 + 0] = values[0];
        output[i * 3 + 1] = values[1];
        output[i * 3 + 2] = values[2];
    }

    return true;
}

static Mesh load_mesh_from_model(
    const tinygltf::Model& model)
{
    Mesh mesh;

    if (model.meshes.empty()) {
        std::cerr << "Model contains no meshes.\n";
        return mesh;
    }

    const tinygltf::Mesh& gltf_mesh =
        model.meshes[0];

    if (gltf_mesh.primitives.empty()) {
        std::cerr << "Mesh contains no primitives.\n";
        return mesh;
    }

    const tinygltf::Primitive& primitive =
        gltf_mesh.primitives[0];

    const auto position_it =
        primitive.attributes.find("POSITION");

    if (position_it == primitive.attributes.end()) {
        std::cerr << "Mesh has no POSITION attribute.\n";
        return mesh;
    }

    const auto& position_accessor =
        model.accessors[position_it->second];

    std::vector<float> positions;

    if (!read_vec3_attribute(
            model,
            position_accessor,
            positions)) {
        std::cerr << "Failed to read vertex positions.\n";
        return mesh;
    }

    std::vector<float> normals;

    const auto normal_it =
        primitive.attributes.find("NORMAL");

    if (normal_it != primitive.attributes.end()) {
        const auto& normal_accessor =
            model.accessors[normal_it->second];

        if (!read_vec3_attribute(
                model,
                normal_accessor,
                normals)) {
            std::cerr
                << "Failed to read vertex normals.\n";
            return mesh;
        }
    }
    else {
        normals.resize(positions.size(), 0.0f);

        for (size_t i = 0;
             i + 8 < positions.size();
             i += 9) {

            glm::vec3 a(
                positions[i + 3] - positions[i + 0],
                positions[i + 4] - positions[i + 1],
                positions[i + 5] - positions[i + 2]);

            glm::vec3 b(
                positions[i + 6] - positions[i + 0],
                positions[i + 7] - positions[i + 1],
                positions[i + 8] - positions[i + 2]);

            glm::vec3 n =
                glm::normalize(glm::cross(a, b));

            for (int j = 0; j < 3; ++j) {
                normals[i + j * 3 + 0] = n.x;
                normals[i + j * 3 + 1] = n.y;
                normals[i + j * 3 + 2] = n.z;
            }
        }
    }

    if (normals.size() != positions.size()) {
        std::cerr
            << "Position/normal count mismatch.\n";
        return mesh;
    }

    if (primitive.indices < 0) {
        std::cerr
            << "Model primitive has no indices.\n";
        return mesh;
    }

    const auto& index_accessor =
        model.accessors[primitive.indices];

    std::vector<uint32_t> indices;

    if (!read_indices(
            model,
            index_accessor,
            indices)) {
        std::cerr
            << "Failed to read model indices.\n";
        return mesh;
    }

    std::vector<float> vertex_data;

    vertex_data.reserve(
        (positions.size() / 3) * 6);

    for (size_t i = 0;
         i < positions.size();
         i += 3) {

        vertex_data.push_back(positions[i + 0]);
        vertex_data.push_back(positions[i + 1]);
        vertex_data.push_back(positions[i + 2]);

        vertex_data.push_back(normals[i + 0]);
        vertex_data.push_back(normals[i + 1]);
        vertex_data.push_back(normals[i + 2]);
    }

    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glGenBuffers(1, &mesh.ebo);

    glBindVertexArray(mesh.vao);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);

    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(
            vertex_data.size() * sizeof(float)),
        vertex_data.data(),
        GL_STATIC_DRAW);

    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        6 * sizeof(float),
        reinterpret_cast<void*>(0));

    glEnableVertexAttribArray(0);

    glVertexAttribPointer(
        1,
        3,
        GL_FLOAT,
        GL_FALSE,
        6 * sizeof(float),
        reinterpret_cast<void*>(
            3 * sizeof(float)));

    glEnableVertexAttribArray(1);

    glBindBuffer(
        GL_ELEMENT_ARRAY_BUFFER,
        mesh.ebo);

    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(
            indices.size() * sizeof(uint32_t)),
        indices.data(),
        GL_STATIC_DRAW);

    mesh.index_count =
        static_cast<GLsizei>(indices.size());

    glBindVertexArray(0);

    std::cout
        << "Loaded mesh: "
        << positions.size() / 3
        << " vertices, "
        << indices.size()
        << " indices\n";

    return mesh;
}

static void destroy_mesh(Mesh& mesh)
{
    if (mesh.ebo != 0) {
        glDeleteBuffers(1, &mesh.ebo);
        mesh.ebo = 0;
    }

    if (mesh.vbo != 0) {
        glDeleteBuffers(1, &mesh.vbo);
        mesh.vbo = 0;
    }

    if (mesh.vao != 0) {
        glDeleteVertexArrays(1, &mesh.vao);
        mesh.vao = 0;
    }

    mesh.index_count = 0;
}

int main()
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr
            << "SDL initialization failed: "
            << SDL_GetError()
            << '\n';

        return 1;
    }

    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_MAJOR_VERSION,
        3);

    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_MINOR_VERSION,
        0);

    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_PROFILE_MASK,
        SDL_GL_CONTEXT_PROFILE_ES);

    SDL_GL_SetAttribute(
        SDL_GL_DOUBLEBUFFER,
        1);

    SDL_GL_SetAttribute(
        SDL_GL_DEPTH_SIZE,
        24);

    SDL_Window* window =
        SDL_CreateWindow(
            "PHENOMENA",
            800,
            600,
            SDL_WINDOW_OPENGL |
            SDL_WINDOW_RESIZABLE);

    if (!window) {
        std::cerr
            << "Failed to create window: "
            << SDL_GetError()
            << '\n';

        SDL_Quit();
        return 1;
    }

    SDL_GLContext context =
        SDL_GL_CreateContext(window);

    if (!context) {
        std::cerr
            << "Failed to create OpenGL ES context: "
            << SDL_GetError()
            << '\n';

        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_GL_MakeCurrent(window, context);
    SDL_GL_SetSwapInterval(1);

    std::cout
        << "OpenGL renderer: "
        << reinterpret_cast<const char*>(
            glGetString(GL_RENDERER))
        << '\n';

    std::cout
        << "OpenGL version: "
        << reinterpret_cast<const char*>(
            glGetString(GL_VERSION))
        << '\n';

    glEnable(GL_DEPTH_TEST);

    glClearColor(
        0.1f,
        0.1f,
        0.15f,
        1.0f);

    GLuint program =
        create_shader_program();

    if (program == 0) {
        SDL_GL_DestroyContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    Mesh mesh;

    /*
     * Temporary test path.
     *
     * This proves that the renderer can load a model.
     * The Android File Picker will be connected here next.
     */
    const std::string model_path =
        "model.glb";

    tinygltf::Model model;

    if (load_gltf_model(
            model_path,
            model)) {

        mesh =
            load_mesh_from_model(model);
    }

    Camera camera;

    bool running = true;

    while (running) {
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        int width = 800;
        int height = 600;

        SDL_GetWindowSize(
            window,
            &width,
            &height);

        if (height <= 0) {
            height = 1;
        }

        glViewport(
            0,
            0,
            width,
            height);

        glClear(
            GL_COLOR_BUFFER_BIT |
            GL_DEPTH_BUFFER_BIT);

        glUseProgram(program);

        const float aspect =
            static_cast<float>(width) /
            static_cast<float>(height);

        glm::mat4 projection =
            glm::perspective(
                glm::radians(45.0f),
                aspect,
                0.1f,
                100.0f);

        glm::mat4 view =
            glm::lookAt(
                camera.position,
                camera.target,
                camera.up);

        glm::mat4 model_matrix =
            glm::mat4(1.0f);

        GLint projection_location =
            glGetUniformLocation(
                program,
                "projection");

        GLint view_location =
            glGetUniformLocation(
                program,
                "view");

        GLint model_location =
            glGetUniformLocation(
                program,
                "model");

        glUniformMatrix4fv(
            projection_location,
            1,
            GL_FALSE,
            glm::value_ptr(projection));

        glUniformMatrix4fv(
            view_location,
            1,
            GL_FALSE,
            glm::value_ptr(view));

        glUniformMatrix4fv(
            model_location,
            1,
            GL_FALSE,
            glm::value_ptr(model_matrix));

        if (mesh.vao != 0 &&
            mesh.index_count > 0) {

            glBindVertexArray(mesh.vao);

            glDrawElements(
                GL_TRIANGLES,
                mesh.index_count,
                GL_UNSIGNED_INT,
                nullptr);

            glBindVertexArray(0);
        }

        SDL_GL_SwapWindow(window);
    }

    destroy_mesh(mesh);

    glDeleteProgram(program);

    SDL_GL_DestroyContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
