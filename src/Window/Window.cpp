#include "Window.h"
#include <iostream>

Window::Window(std::string title, unsigned int width, unsigned int height)
    : m_Title(title)
{
    // Set Error Callback
    glfwSetErrorCallback([] (int error, const char* description) {
        printf("Error: %s\n", description);
    });

    // check if GLFW initialized correctly
    if (!glfwInit())
    {
        exit(EXIT_FAILURE);
    }

    // create GLFW window and context
    m_Window = glfwCreateWindow(width, height, title.c_str(), NULL, NULL);
    
    // Set user pointer for window data
    glfwSetWindowUserPointer(m_Window, &m_Data);

    // check if window create failed
    if (!m_Window)
    {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    m_Data.m_Width = width;
    m_Data.m_Height = height;

    glfwMakeContextCurrent(m_Window);
    glfwSwapInterval(1);

    glfwGetFramebufferSize(m_Window, (int*)&m_Data.m_FBWidth, (int*)&m_Data.m_FBHeight);

    // Set Input Callbacks
    glfwSetWindowCloseCallback(m_Window, [] (GLFWwindow* window) {
        WindowData* data = (WindowData*) glfwGetWindowUserPointer(window);
        data->m_Closed = true;
    });

    glfwSetWindowSizeCallback(m_Window, [] (GLFWwindow* window, int width, int height) {
        WindowData* data = (WindowData*)glfwGetWindowUserPointer(window);

        data->m_Width = width;
        data->m_Height = height;
    });

    glfwSetFramebufferSizeCallback(m_Window, [](GLFWwindow* window, int width, int height) {
        WindowData* data = (WindowData*)glfwGetWindowUserPointer(window);

        data->m_FBWidth = width;
        data->m_FBHeight = height;

        data->m_PixelBuffer->ResizeBuffer(width, height);

        glViewport(0, 0, width, height);
    });
}

Window::~Window()
{
    glfwDestroyWindow(m_Window);
}

std::pair<unsigned int, unsigned int> Window::GetSize()
{
    return std::make_pair(m_Data.m_Width, m_Data.m_Height);
}

std::pair<unsigned int, unsigned int> Window::GetFrameBufferSize()
{
    return std::make_pair(m_Data.m_FBWidth, m_Data.m_FBHeight);
}

void Window::Update()
{
    glDrawPixels(m_Data.m_Width, m_Data.m_Height, GL_RGB, GL_FLOAT, m_Data.m_PixelBuffer->GetPixels());
    glfwSwapBuffers(m_Window);
    glfwPollEvents();
}

void Window::SetPixelBuffer(PixelBuffer* pixelBuffer)
{
    m_Data.m_PixelBuffer = pixelBuffer;
}
