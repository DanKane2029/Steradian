#pragma once
#include <GLFW/glfw3.h>

#include <string>

#include "PixelBuffer.h"

/**
 * wrapper for the GLFW window used to display the rendered image
 */
class Window
{
  public:
    Window(std::string title, int width, int height);
    ~Window();

    auto getContext() const -> GLFWwindow *
    {
        return m_Window;
    };
    auto shouldClose() const -> bool
    {
        return m_Data.m_Closed;
    };

    auto getSize() -> std::pair<int, int>;
    auto getFrameBufferSize() -> std::pair<int, int>;

    void update();

    void setPixelBuffer(PixelBuffer *pixelBuffer);
    void pollEvents();

  private:
    std::string m_Title;

    struct WindowData
    {
        int m_Width = 0, m_Height = 0;
        int m_FBWidth = 0, m_FBHeight = 0;
        bool m_Closed = false;
        PixelBuffer *m_PixelBuffer;
    };

    WindowData m_Data;

    GLFWwindow *m_Window;

    static void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods);
};
