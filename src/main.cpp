// Copyright (c) 2020 Emmanuel Arias
// clang-format off
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
// clang-format on
#include <iostream>
#include <chrono>

#include "include/imgui_layer/imgui_nes_window_manager.h"
#include "include/imgui_layer/imgui_nes_status_window.h"
#include "include/nes.h"
#include "include/cartridge.h"
#include "include/global_nes.h"
#include "helpers/RootDir.h"

using Nes = cpuemulator::Nes;
using Cartridge = cpuemulator::Cartridge;

// Forward declaration
void framebufferSizeCallback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);

int main(int argc, char* argv[]) {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // UiConfig uiConfig;
    int screenWidth = 1250;
    int screenHeight = 800;
    GLFWwindow* window =
        glfwCreateWindow(screenWidth, screenHeight, "NES Emulator", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << '\n';
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << '\n';
        return -1;
    }

    glViewport(0, 0, screenWidth, screenHeight);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

    Nes* nesEmulator = g_GetGlobalNes();

    if (argc > 1) {
        std::string cartridgePath = ROOT_DIR "res/roms/" + std::string(argv[1]);
        Cartridge* cartridge = new Cartridge(cartridgePath);
        if (cartridge->IsLoaded()) {
            nesEmulator->InsertCatridge(cartridge);
        }
    }

    nesEmulator->Reset();

    ImGuiNesWindowManager uiManager;
    uiManager.Initialize(window);
    auto nesStatusWindow = uiManager.m_NesStatusWindow;

    using clock = std::chrono::high_resolution_clock;
    using namespace std::chrono;
    static constexpr nanoseconds frameTime{1000ms / 60};

    nanoseconds residualTime = 0ms;
    auto frameStartTime = clock::now();
    while (!glfwWindowShouldClose(window)) {
        auto deltaTime = clock::now() - frameStartTime;
        frameStartTime = clock::now();

        glfwPollEvents();

        processInput(window);

        if (nesStatusWindow->IsNesPoweredUp()) {
            if (residualTime > 0ns) {
                residualTime -= deltaTime;
            } else {
                residualTime += frameTime - deltaTime;

                nesEmulator->DoFrame();
            }
        }
        uiManager.Update();
        uiManager.Render();

        glfwSwapBuffers(window);
    }

    // Cleanup
    uiManager.Shutdown();
    glfwTerminate();
}

void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void processInput(GLFWwindow* window) {
    Nes* nesEmulator = g_GetGlobalNes();
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }
    nesEmulator->m_Controllers[0] = 0x00;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        nesEmulator->m_Controllers[0] |= 0x80;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        nesEmulator->m_Controllers[0] |= 0x40;
    }
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        nesEmulator->m_Controllers[0] |= 0x20;
    }
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        nesEmulator->m_Controllers[0] |= 0x10;
    }
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
        nesEmulator->m_Controllers[0] |= 0x08;
    }
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
        nesEmulator->m_Controllers[0] |= 0x04;
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
        nesEmulator->m_Controllers[0] |= 0x02;
    }
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
        nesEmulator->m_Controllers[0] |= 0x01;
    }
}
