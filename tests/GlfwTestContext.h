#pragma once
#define GLFW_INCLUDE_NONE
#include "../SimpleGame/Dependencies/GLFW/glfw-3.4/include/GLFW/glfw3.h"

// Benchmarks keep the GL context alive until all local render resources are gone.
struct GlfwTestContext
{
    GLFWwindow* window = nullptr;

    GlfwTestContext(int width, int height, int samples, const char* title)
    {
        if (!glfwInit())
        {
            return;
        }
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        glfwWindowHint(GLFW_SAMPLES, samples);
        window = glfwCreateWindow(width, height, title, nullptr, nullptr);
        if (window)
        {
            glfwMakeContextCurrent(window);
            glfwSwapInterval(0);
        }
    }

    ~GlfwTestContext()
    {
        if (window)
        {
            glfwDestroyWindow(window);
        }
        glfwTerminate();
    }

    GlfwTestContext(const GlfwTestContext&) = delete;
    GlfwTestContext& operator=(const GlfwTestContext&) = delete;
};
