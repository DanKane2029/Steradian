#include "Window.h"

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

    m_Data.m_Width = width;
    m_Data.m_Height = height;
    
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

    glfwMakeContextCurrent(m_Window);
    glfwSwapInterval(1);

    // Set Input Callbacks
    glfwSetWindowCloseCallback(m_Window, [] (GLFWwindow* window) {
        WindowData* data = (WindowData*) glfwGetWindowUserPointer(window);
        data->m_Closed = true;
    });

    glfwSetWindowSizeCallback(m_Window, [] (GLFWwindow* window, int width, int height) {
        WindowData* data = (WindowData*)glfwGetWindowUserPointer(window);

        data->m_Width = width;
        data->m_Height = height;

        glViewport(0, 0, width, height);
    });
}

Window::~Window()
{
    glfwDestroyWindow(m_Window);
}

void Window::Update()
{
    glfwSwapBuffers(m_Window);
    glfwPollEvents();
}
