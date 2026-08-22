#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <tiny_gltf.h>

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

#ifdef __ANDROID__
#include <jni.h>
#include <android/native_activity.h>
#endif

// OpenGL ES shader source
const char* vertex_shader = R"(
    #version 300 es
    precision highp float;
    
    layout(location = 0) in vec3 position;
    layout(location = 1) in vec3 normal;
    
    uniform mat4 projection;
    uniform mat4 view;
    uniform mat4 model;
    
    out vec3 v_normal;
    
    void main() {
        gl_Position = projection * view * model * vec4(position, 1.0);
        v_normal = normalize(mat3(model) * normal);
    }
)";

const char* fragment_shader = R"(
    #version 300 es
    precision highp float;
    
    in vec3 v_normal;
    out vec4 fragColor;
    
    void main() {
        vec3 light = normalize(vec3(1.0, 1.0, 1.0));
        float diff = max(dot(v_normal, light), 0.3);
        fragColor = vec4(vec3(0.8) * diff, 1.0);
    }
)";

struct Mesh {
    GLuint vao{0};
    GLuint vbo{0};
    GLuint ebo{0};
    uint32_t index_count{0};
};

struct Camera {
    glm::vec3 position{0.0f, 0.0f, 5.0f};
    glm::vec3 target{0.0f, 0.0f, 0.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    float yaw{0.0f};
    float pitch{0.0f};
};

GLuint compile_shader(const char* source, GLenum type) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar info_log[512];
        glGetShaderInfoLog(shader, sizeof(info_log), nullptr, info_log);
        std::cerr << "Shader compilation failed: " << info_log << std::endl;
    }
    
    return shader;
}

GLuint create_program() {
    GLuint vs = compile_shader(vertex_shader, GL_VERTEX_SHADER);
    GLuint fs = compile_shader(fragment_shader, GL_FRAGMENT_SHADER);
    
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar info_log[512];
        glGetProgramInfoLog(program, sizeof(info_log), nullptr, info_log);
        std::cerr << "Program linking failed: " << info_log << std::endl;
    }
    
    glDeleteShader(vs);
    glDeleteShader(fs);
    
    return program;
}

Mesh load_gltf_mesh(const std::string& filepath) {
    Mesh mesh;
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string error, warning;
    
    bool success = loader.LoadASCIIFromFile(&model, &error, &warning, filepath);
    if (!success) {
        std::cerr << "Failed to load glTF: " << error << std::endl;
        return mesh;
    }
    
    if (model.meshes.empty()) {
        std::cerr << "No meshes in glTF file" << std::endl;
        return mesh;
    }
    
    const auto& gltf_mesh = model.meshes[0];
    if (gltf_mesh.primitives.empty()) {
        std::cerr << "No primitives in mesh" << std::endl;
        return mesh;
    }
    
    const auto& primitive = gltf_mesh.primitives[0];
    
    // Get vertex positions
    std::vector<float> positions;
    {
        const auto& accessor = model.accessors[primitive.attributes.at("POSITION")];
        const auto& buffer_view = model.bufferViews[accessor.bufferView];
        const auto& buffer = model.buffers[buffer_view.buffer];
        
        const float* pos_data = reinterpret_cast<const float*>(
            buffer.data.data() + buffer_view.byteOffset + accessor.byteOffset);
        
        positions.resize(accessor.count * 3);
        std::memcpy(positions.data(), pos_data, positions.size() * sizeof(float));
    }
    
    // Get vertex normals (or generate them if not present)
    std::vector<float> normals;
    if (primitive.attributes.count("NORMAL")) {
        const auto& accessor = model.accessors[primitive.attributes.at("NORMAL")];
        const auto& buffer_view = model.bufferViews[accessor.bufferView];
        const auto& buffer = model.buffers[buffer_view.buffer];
        
        const float* norm_data = reinterpret_cast<const float*>(
            buffer.data.data() + buffer_view.byteOffset + accessor.byteOffset);
        
        normals.resize(accessor.count * 3);
        std::memcpy(normals.data(), norm_data, normals.size() * sizeof(float));
    } else {
        // Generate flat normals
        normals.resize(positions.size(), 0.0f);
        for (size_t i = 0; i < positions.size(); i += 9) {
            glm::vec3 n(0.0f, 0.0f, 1.0f);
            for (int j = 0; j < 3; ++j) {
                normals[i + j * 3 + 0] = n.x;
                normals[i + j * 3 + 1] = n.y;
                normals[i + j * 3 + 2] = n.z;
            }
        }
    }
    
    // Get indices
    std::vector<uint32_t> indices;
    if (primitive.indices >= 0) {
        const auto& accessor = model.accessors[primitive.indices];
        const auto& buffer_view = model.bufferViews[accessor.bufferView];
        const auto& buffer = model.buffers[buffer_view.buffer];
        
        mesh.index_count = accessor.count;
        
        if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
            const uint32_t* idx_data = reinterpret_cast<const uint32_t*>(
                buffer.data.data() + buffer_view.byteOffset + accessor.byteOffset);
            indices.assign(idx_data, idx_data + accessor.count);
        } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
            const uint16_t* idx_data = reinterpret_cast<const uint16_t*>(
                buffer.data.data() + buffer_view.byteOffset + accessor.byteOffset);
            indices.assign(idx_data, idx_data + accessor.count);
        }
    }
    
    // Create VAO/VBO/EBO
    glGenVertexArrays(1, &mesh.vao);
    glBindVertexArray(mesh.vao);
    
    glGenBuffers(1, &mesh.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    
    // Interleave position + normal
    std::vector<float> vertex_data;
    for (size_t i = 0; i < positions.size(); i += 3) {
        vertex_data.push_back(positions[i]);
        vertex_data.push_back(positions[i + 1]);
        vertex_data.push_back(positions[i + 2]);
        vertex_data.push_back(normals[i]);
        vertex_data.push_back(normals[i + 1]);
        vertex_data.push_back(normals[i + 2]);
    }
    
    glBufferData(GL_ARRAY_BUFFER, vertex_data.size() * sizeof(float), vertex_data.data(), GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glGenBuffers(1, &mesh.ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);
    
    glBindVertexArray(0);
    
    std::cout << "Loaded mesh: " << positions.size() / 3 << " vertices, " << indices.size() << " indices" << std::endl;
    
    return mesh;
}

#ifdef __ANDROID__
std::string get_file_from_picker(ANativeActivity* activity) {
    // This is a placeholder — you'll need to implement JNI calls to Android's file picker
    // For now, assume the file is in the app's external files directory
    return std::string(activity->externalDataPath) + "/model.glb";
}
#endif

int main() {
    SDL_InitSubSystem(SDL_INIT_VIDEO);
    
    SDL_Window* window = SDL_CreateWindow(
        "PHENOMENA",
        800, 600,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );
    
    if (!window) {
        std::cerr << "Failed to create window" << std::endl;
        return 1;
    }
    
    SDL_GLContext context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, context);
    SDL_GL_SetSwapInterval(1);
    
    // Load extensions (using GLEW or similar)
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    
    GLuint program = create_program();
    Mesh mesh = load_gltf_mesh("model.glb"); // Replace with actual file path from picker
    
    Camera camera;
    bool running = true;
    
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }
        
        // Render
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        glUseProgram(program);
        
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(camera.position, camera.target, camera.up);
        glm::mat4 model = glm::mat4(1.0f);
        
        glUniformMatrix4fv(glGetUniformLocation(program, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(program, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(program, "model"), 1, GL_FALSE, glm::value_ptr(model));
        
        glBindVertexArray(mesh.vao);
        glDrawElements(GL_TRIANGLES, mesh.index_count, GL_UNSIGNED_INT, nullptr);
        
        SDL_GL_SwapWindow(window);
    }
    
    glDeleteProgram(program);
    SDL_GL_DestroyContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}
