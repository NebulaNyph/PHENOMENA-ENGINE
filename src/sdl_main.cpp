#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <GLES3/gl3.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

struct app_state final
{
    SDL_Window* window{ nullptr };
    SDL_GLContext gl_context{ nullptr };

    GLuint program{ 0 };
    GLuint vao{ 0 };
    GLuint vbo{ 0 };
    GLuint ebo{ 0 };

    GLint mvp_location{ -1 };

    float rotation{ 0.0f };
};

struct mat4 final
{
    float m[16]{};
};

static mat4 identity_matrix()
{
    mat4 result{};

    result.m[0] = 1.0f;
    result.m[5] = 1.0f;
    result.m[10] = 1.0f;
    result.m[15] = 1.0f;

    return result;
}

static mat4 multiply(const mat4& a, const mat4& b)
{
    mat4 result{};

    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            result.m[column * 4 + row] =
                a.m[0 * 4 + row] * b.m[column * 4 + 0] +
                a.m[1 * 4 + row] * b.m[column * 4 + 1] +
                a.m[2 * 4 + row] * b.m[column * 4 + 2] +
                a.m[3 * 4 + row] * b.m[column * 4 + 3];
        }
    }

    return result;
}

static mat4 perspective(
    float fov_y,
    float aspect,
    float near_plane,
    float far_plane)
{
    mat4 result{};

    const float f = 1.0f / std::tan(fov_y * 0.5f);

    result.m[0] = f / aspect;
    result.m[5] = f;
    result.m[10] =
        (far_plane + near_plane) / (near_plane - far_plane);
    result.m[11] = -1.0f;
    result.m[14] =
        (2.0f * far_plane * near_plane) /
        (near_plane - far_plane);

    return result;
}

static mat4 translation(float x, float y, float z)
{
    mat4 result = identity_matrix();

    result.m[12] = x;
    result.m[13] = y;
    result.m[14] = z;

    return result;
}

static mat4 rotation_x(float angle)
{
    mat4 result = identity_matrix();

    const float c = std::cos(angle);
    const float s = std::sin(angle);

    result.m[5] = c;
    result.m[6] = s;
    result.m[9] = -s;
    result.m[10] = c;

    return result;
}

static mat4 rotation_y(float angle)
{
    mat4 result = identity_matrix();

    const float c = std::cos(angle);
    const float s = std::sin(angle);

    result.m[0] = c;
    result.m[2] = -s;
    result.m[8] = s;
    result.m[10] = c;

    return result;
}

static GLuint compile_shader(GLenum type, const char* source)
{
    const GLuint shader = glCreateShader(type);

    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (success != GL_TRUE) {
        char log[2048]{};
        glGetShaderInfoLog(
            shader,
            sizeof(log),
            nullptr,
            log);

        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION,
            "Shader compilation failed:\n%s",
            log);

        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

static GLuint create_program()
{
    constexpr const char* vertex_shader_source = R"(
        #version 300 es

        precision highp float;

        layout(location = 0) in vec3 a_position;
        layout(location = 1) in vec3 a_color;

        uniform mat4 u_mvp;

        out vec3 v_color;

        void main()
        {
            gl_Position = u_mvp * vec4(a_position, 1.0);
            v_color = a_color;
        }
    )";

    constexpr const char* fragment_shader_source = R"(
        #version 300 es

        precision highp float;

        in vec3 v_color;

        out vec4 out_color;

        void main()
        {
            out_color = vec4(v_color, 1.0);
        }
    )";

    const GLuint vertex_shader =
        compile_shader(
            GL_VERTEX_SHADER,
            vertex_shader_source);

    if (vertex_shader == 0) {
        return 0;
    }

    const GLuint fragment_shader =
        compile_shader(
            GL_FRAGMENT_SHADER,
            fragment_shader_source);

    if (fragment_shader == 0) {
        glDeleteShader(vertex_shader);
        return 0;
    }

    const GLuint program = glCreateProgram();

    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);

    glLinkProgram(program);

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    GLint success = GL_FALSE;
    glGetProgramiv(
        program,
        GL_LINK_STATUS,
        &success);

    if (success != GL_TRUE) {
        char log[2048]{};

        glGetProgramInfoLog(
            program,
            sizeof(log),
            nullptr,
            log);

        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION,
            "Program linking failed:\n%s",
            log);

        glDeleteProgram(program);
        return 0;
    }

    return program;
}

static bool create_cube(app_state& state)
{
    constexpr float vertices[] = {
        // Position              // Color

        // Front
        -1.0f, -1.0f,  1.0f,     1.0f, 0.0f, 0.0f,
         1.0f, -1.0f,  1.0f,     0.0f, 1.0f, 0.0f,
         1.0f,  1.0f,  1.0f,     0.0f, 0.0f, 1.0f,
        -1.0f,  1.0f,  1.0f,     1.0f, 1.0f, 0.0f,

        // Back
        -1.0f, -1.0f, -1.0f,     1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, -1.0f,     0.0f, 1.0f, 1.0f,
         1.0f,  1.0f, -1.0f,     1.0f, 1.0f, 1.0f,
        -1.0f,  1.0f, -1.0f,     0.2f, 0.4f, 1.0f,
    };

    constexpr std::uint16_t indices[] = {
        // Front
        0, 1, 2,
        2, 3, 0,

        // Back
        5, 4, 7,
        7, 6, 5,

        // Left
        4, 0, 3,
        3, 7, 4,

        // Right
        1, 5, 6,
        6, 2, 1,

        // Top
        3, 2, 6,
        6, 7, 3,

        // Bottom
        4, 5, 1,
        1, 0, 4
    };

    glGenVertexArrays(1, &state.vao);
    glGenBuffers(1, &state.vbo);
    glGenBuffers(1, &state.ebo);

    glBindVertexArray(state.vao);

    glBindBuffer(GL_ARRAY_BUFFER, state.vbo);

    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(vertices),
        vertices,
        GL_STATIC_DRAW);

    glBindBuffer(
        GL_ELEMENT_ARRAY_BUFFER,
        state.ebo);

    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        sizeof(indices),
        indices,
        GL_STATIC_DRAW);

    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        6 * sizeof(float),
        nullptr);

    glEnableVertexAttribArray(0);

    glVertexAttribPointer(
        1,
        3,
        GL_FLOAT,
        GL_FALSE,
        6 * sizeof(float),
        reinterpret_cast<void*>(3 * sizeof(float)));

    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    return true;
}

SDL_AppResult SDL_AppInit(
    void** appstate,
    [[maybe_unused]] int argc,
    [[maybe_unused]] char* argv[])
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION,
            "SDL_Init failed: %s",
            SDL_GetError());

        return SDL_APP_FAILURE;
    }

    // Request an OpenGL ES 3 context.
    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_PROFILE_MASK,
        SDL_GL_CONTEXT_PROFILE_ES);

    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_MAJOR_VERSION,
        3);

    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_MINOR_VERSION,
        0);

    SDL_GL_SetAttribute(
        SDL_GL_DOUBLEBUFFER,
        1);

    SDL_GL_SetAttribute(
        SDL_GL_DEPTH_SIZE,
        24);

    auto* state = new (std::nothrow) app_state{};

    if (!state) {
        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION,
            "Failed to allocate application state");

        return SDL_APP_FAILURE;
    }

    state->window = SDL_CreateWindow(
        "PHENOMENA V3",
        1280,
        720,
        SDL_WINDOW_OPENGL |
        SDL_WINDOW_RESIZABLE);

    if (!state->window) {
        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION,
            "SDL_CreateWindow failed: %s",
            SDL_GetError());

        delete state;
        return SDL_APP_FAILURE;
    }

    state->gl_context =
        SDL_GL_CreateContext(state->window);

    if (!state->gl_context) {
        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION,
            "SDL_GL_CreateContext failed: %s",
            SDL_GetError());

        SDL_DestroyWindow(state->window);
        delete state;

        return SDL_APP_FAILURE;
    }

    if (!SDL_GL_MakeCurrent(
            state->window,
            state->gl_context)) {

        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION,
            "SDL_GL_MakeCurrent failed: %s",
            SDL_GetError());

        return SDL_APP_FAILURE;
    }

    SDL_Log(
        "OpenGL Vendor: %s",
        reinterpret_cast<const char*>(
            glGetString(GL_VENDOR)));

    SDL_Log(
        "OpenGL Renderer: %s",
        reinterpret_cast<const char*>(
            glGetString(GL_RENDERER)));

    SDL_Log(
        "OpenGL Version: %s",
        reinterpret_cast<const char*>(
            glGetString(GL_VERSION)));

    SDL_Log(
        "GLSL Version: %s",
        reinterpret_cast<const char*>(
            glGetString(GL_SHADING_LANGUAGE_VERSION)));

    state->program = create_program();

    if (state->program == 0) {
        return SDL_APP_FAILURE;
    }

    state->mvp_location =
        glGetUniformLocation(
            state->program,
            "u_mvp");

    if (!create_cube(*state)) {
        return SDL_APP_FAILURE;
    }

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    *appstate = state;

    SDL_Log("PHENOMENA V3 renderer initialized.");

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate)
{
    auto* state =
        static_cast<app_state*>(appstate);

    int width = 1;
    int height = 1;

    SDL_GetWindowSizeInPixels(
        state->window,
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

    glClearColor(
        0.025f,
        0.035f,
        0.055f,
        1.0f);

    glClear(
        GL_COLOR_BUFFER_BIT |
        GL_DEPTH_BUFFER_BIT);

    state->rotation += 0.01f;

    const float aspect =
        static_cast<float>(width) /
        static_cast<float>(height);

    const mat4 projection =
        perspective(
            1.0471976f,
            aspect,
            0.1f,
            100.0f);

    const mat4 view =
        translation(
            0.0f,
            0.0f,
            -5.0f);

    const mat4 rotation_y =
        rotation_y(state->rotation);

    const mat4 rotation_x_matrix =
        rotation_x(
            state->rotation * 0.65f);

    const mat4 model =
        multiply(
            rotation_y,
            rotation_x_matrix);

    const mat4 view_model =
        multiply(
            view,
            model);

    const mat4 mvp =
        multiply(
            projection,
            view_model);

    glUseProgram(state->program);

    glUniformMatrix4fv(
        state->mvp_location,
        1,
        GL_FALSE,
        mvp.m);

    glBindVertexArray(state->vao);

    glDrawElements(
        GL_TRIANGLES,
        36,
        GL_UNSIGNED_SHORT,
        nullptr);

    glBindVertexArray(0);

    SDL_GL_SwapWindow(state->window);

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(
    void* /*appstate*/,
    SDL_Event* event)
{
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(
    void* appstate,
    [[maybe_unused]] SDL_AppResult result)
{
    auto* state =
        static_cast<app_state*>(appstate);

    if (!state) {
        return;
    }

    if (state->ebo != 0) {
        glDeleteBuffers(1, &state->ebo);
    }

    if (state->vbo != 0) {
        glDeleteBuffers(1, &state->vbo);
    }

    if (state->vao != 0) {
        glDeleteVertexArrays(1, &state->vao);
    }

    if (state->program != 0) {
        glDeleteProgram(state->program);
    }

    if (state->gl_context) {
        SDL_GL_DestroyContext(
            state->gl_context);
    }

    if (state->window) {
        SDL_DestroyWindow(
            state->window);
    }

    delete state;

    SDL_Quit();
}
