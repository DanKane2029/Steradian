#pragma once
#include <GLFW/glfw3.h>

#include <string>
#include <vector>

#include "PixelBuffer.h"
#include "RayTracer/RayTracer.h"

/**
 * wrapper for the GLFW window used to display the rendered image
 */
class Window
{
  public:
    Window(std::string title, int width, int height);
    ~Window();

    Window(const Window &) = delete;
    auto operator=(const Window &) -> Window & = delete;
    Window(Window &&) = delete;
    auto operator=(Window &&) -> Window & = delete;

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
    void setRayTracer(RayTracer *rayTracer);
    void pollEvents();

    /** sets the text shown in the title bar */
    void setTitle(const std::string &title);

    /**
     * \brief Shows this image instead of the raw accumulated buffer.
     *
     * Used to display a denoised version of the render. Passing nullptr goes back to
     * showing the buffer directly. The pointer must stay valid until it is replaced.
     */
    void setDisplayOverride(const float *pixels);

    /**
     * \brief Reports and clears the "the window was resized" flag.
     *
     * Resizing reallocates the pixel buffer, so the caller has to know in order to
     * restart accumulation.
     */
    auto consumeResized() -> bool;

    /** true while the user is holding the key that requests a viewpoint reset */
    auto resetRequested() -> bool;

  private:
    /** uploads the tone mapped image and draws it over the window */
    void present();

    std::string m_Title;

    struct WindowData
    {
        int m_Width = 0, m_Height = 0;
        int m_FBWidth = 0, m_FBHeight = 0;
        bool m_Closed = false;
        bool m_Resized = false;
        bool m_ResetRequested = false;
        // Initialized: the framebuffer-size callback dereferences these, and GLFW can
        // fire it during window creation, before they have been set.
        PixelBuffer *m_PixelBuffer = nullptr;
        RayTracer *m_RayTracer = nullptr;
    };

    WindowData m_Data;

    GLFWwindow *m_Window;

    /** texture the rendered image is uploaded into each frame */
    GLuint m_Texture = 0;
    int m_TextureWidth = 0;
    int m_TextureHeight = 0;

    /** 8-bit display image, tone mapped from the renderer's linear output */
    std::vector<unsigned char> m_DisplayPixels;

    /** optional post-processed image to show in place of the raw buffer */
    const float *m_DisplayOverride = nullptr;

    static void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods);
};
