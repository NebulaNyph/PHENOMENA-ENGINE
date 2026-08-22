#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "phenomena/runtime.hpp"

#include <chrono>
#include <memory>

struct app_state final {
    std::unique_ptr<phenomena::Runtime> runtime;
    std::chrono::steady_clock::time_point last_tick;
};

SDL_AppResult SDL_AppInit(
    void** appstate,
    [[maybe_unused]] int argc,
    [[maybe_unused]] char* argv[]
)
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION,
            "SDL_Init failed: %s",
            SDL_GetError()
        );

        return SDL_APP_FAILURE;
    }

    auto state = std::make_unique<app_state>();

    state->runtime = std::make_unique<phenomena::Runtime>();
    state->runtime->initialize();
    state->last_tick = std::chrono::steady_clock::now();

    if (!SDL_CreateWindow(
            "PHENOMENA",
            1280,
            720,
            SDL_WINDOW_RESIZABLE
        )) {

        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION,
            "SDL_CreateWindow failed: %s",
            SDL_GetError()
        );

        return SDL_APP_FAILURE;
    }

    SDL_Log(
        "PHENOMENA runtime initialized. World: %s",
        state->runtime->world().id().c_str()
    );

    SDL_Log(
        "Entities: %zu",
        state->runtime->world().entity_count()
    );

    *appstate = state.release();

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate)
{
    auto* state = static_cast<app_state*>(appstate);

    const auto now = std::chrono::steady_clock::now();

    const std::chrono::duration<double> elapsed =
        now - state->last_tick;

    state->last_tick = now;

    state->runtime->update(elapsed.count());

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(
    [[maybe_unused]] void* appstate,
    SDL_Event* event
)
{
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(
    void* appstate,
    [[maybe_unused]] SDL_AppResult result
)
{
    auto* state = static_cast<app_state*>(appstate);

    if (state != nullptr) {
        if (state->runtime) {
            state->runtime->shutdown();
        }

        delete state;
    }
}
