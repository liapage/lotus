#include "App.h"

#include <array>
#include <SDL3/SDL.h>

App::App(int argc, char **argv) : m_window(nullptr, &SDL_DestroyWindow), m_gpuDevice(nullptr, &SDL_DestroyGPUDevice) {
}

SDL_AppResult App::Init() {
    if (!SDL_SetAppMetadata("Lotus", "0.0.1", "page.lia.lotus")) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to set app metadata: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // Create window
    m_window.reset(SDL_CreateWindow("Lotus", 800, 600, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN));
    if (!m_window) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to create SDL window: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_Log("Available GPU drivers:");
    for (int i = 0; i < SDL_GetNumGPUDrivers(); ++i) {
        SDL_Log("    %s", SDL_GetGPUDriver(i));
    }

    m_gpuDevice.reset(SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL, true, nullptr));
    if (!m_gpuDevice) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to create GPU device: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_Log("Using GPU driver %s", SDL_GetGPUDeviceDriver(m_gpuDevice.get()));

    if (!SDL_ClaimWindowForGPUDevice(m_gpuDevice.get(), m_window.get())) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to claim SDL window for GPU device: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_GPUPresentMode presentMode = SDL_GPU_PRESENTMODE_VSYNC;
    if (SDL_WindowSupportsGPUPresentMode(m_gpuDevice.get(), m_window.get(), SDL_GPU_PRESENTMODE_MAILBOX)) {
        SDL_Log("Using mailbox present mode");
        presentMode = SDL_GPU_PRESENTMODE_MAILBOX;
    } else {
        SDL_Log("Using vsync present mode");
    }

    if (!SDL_SetGPUSwapchainParameters(m_gpuDevice.get(), m_window.get(), SDL_GPU_SWAPCHAINCOMPOSITION_SDR, presentMode)) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to set swapchain parameters: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // Initialization done, show window
    if (!SDL_ShowWindow(m_window.get())) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to show window: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult App::Iterate() {
    // Update
    if (auto result = OnUpdate(); result != SDL_APP_CONTINUE) {
        return result;
    }

    // Render
    if (auto result = OnRender(); result != SDL_APP_CONTINUE) {
        return result;
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult App::Event(SDL_Event *event) {
    switch (event->type) {
        case SDL_EVENT_QUIT:
            return SDL_APP_SUCCESS;
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            if (SDL_GetWindowID(m_window.get()) == event->window.windowID) {
                return SDL_APP_SUCCESS;
            }
        default:
            return SDL_APP_CONTINUE;
    }
}

void App::Quit(SDL_AppResult result) {
    SDL_WaitForGPUIdle(m_gpuDevice.get());
    SDL_ReleaseWindowFromGPUDevice(m_gpuDevice.get(), m_window.get());
}

SDL_AppResult App::OnQuit() {
    return SDL_APP_SUCCESS;
}

SDL_AppResult App::OnRender() {
    auto commandBuffer = SDL_AcquireGPUCommandBuffer(m_gpuDevice.get());
    if (!commandBuffer) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to acquire GPU command buffer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_GPUTexture *swapchainTexture = nullptr;
    if (!SDL_AcquireGPUSwapchainTexture(commandBuffer, m_window.get(), &swapchainTexture, nullptr, nullptr)) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to acquire swapchain texture: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (swapchainTexture) {
        std::array colorTargetInfos{
            SDL_GPUColorTargetInfo{
                .texture = swapchainTexture,
                .clear_color = SDL_FColor{1.0f, 0.0f, 0.0f, 1.0f},
                .load_op =  SDL_GPU_LOADOP_CLEAR,
                .store_op =  SDL_GPU_STOREOP_STORE,
            },
        };

        auto renderPass = SDL_BeginGPURenderPass(commandBuffer, colorTargetInfos.data(), colorTargetInfos.size(), nullptr);
        SDL_EndGPURenderPass(renderPass);
    }

    if (!SDL_SubmitGPUCommandBuffer(commandBuffer)) {
        SDL_LogError(APP_LOG_CATEGORY_GENERIC, "Failed to submit command buffer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult App::OnUpdate() {
    return SDL_APP_CONTINUE;
}
