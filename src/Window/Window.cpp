#include "Window.h"

#include "Utils/ImageIO.h"

#include <array>
#include <iostream>

/**
 * creates a GLFW window
 * boiler plate code from the GLFW website
 * https://www.glfw.org/docs/3.3/quick.html
 *
 * \param title - the title of the application
 * \param width - the initial width of the wimdow
 * \param height - the initial height of the window
 */
Window::Window(std::string title, int width, int height) : m_Title(std::move(title))
{
    // Set Error Callback
    glfwSetErrorCallback([](int error, const char *description) {
        std::cerr << "GLFW error " << error << ": " << description << std::endl;
    });

    // check if GLFW initialized correctly
    if (!glfwInit())
    {
        throw std::runtime_error("Could not initialize GLFW");
    }

    // create GLFW window and context
    m_Window = glfwCreateWindow(width, height, m_Title.c_str(), nullptr, nullptr);

    // check if window create failed
    if (m_Window == nullptr)
    {
        glfwTerminate();
        throw std::runtime_error("Could not create a window. Is a display available?");
    }

    // Set user pointer for window data
    glfwSetWindowUserPointer(m_Window, &m_Data);

    m_Data.m_Width = width;
    m_Data.m_Height = height;

    glfwMakeContextCurrent(m_Window);

    // The renderer, not the display, decides how fast frames arrive, so there is nothing
    // to gain from waiting on vertical blank and it only adds latency to each pass.
    glfwSwapInterval(0);

    glfwGetFramebufferSize(m_Window, &m_Data.m_FBWidth, &m_Data.m_FBHeight);

    // Set Input Callbacks
    glfwSetWindowCloseCallback(m_Window, [](GLFWwindow *window) {
        auto *data = static_cast<WindowData *>(glfwGetWindowUserPointer(window));
        data->m_Closed = true;
    });

    glfwSetWindowSizeCallback(m_Window, [](GLFWwindow *window, int newWidth, int newHeight) {
        auto *data = static_cast<WindowData *>(glfwGetWindowUserPointer(window));

        data->m_Width = newWidth;
        data->m_Height = newHeight;
    });

    glfwSetKeyCallback(m_Window, keyCallback);

    glfwSetFramebufferSizeCallback(m_Window, [](GLFWwindow *window, int newWidth, int newHeight) {
        auto *data = static_cast<WindowData *>(glfwGetWindowUserPointer(window));

        if (newWidth <= 0 || newHeight <= 0)
        {
            return;
        }

        data->m_FBWidth = newWidth;
        data->m_FBHeight = newHeight;

        if (data->m_PixelBuffer != nullptr)
        {
            data->m_PixelBuffer->resizeBuffer(newWidth, newHeight);
        }
        if (data->m_RayTracer != nullptr)
        {
            data->m_RayTracer->updateAspectRatio(static_cast<float>(newWidth) / static_cast<float>(newHeight));
        }

        data->m_Resized = true;

        glViewport(0, 0, newWidth, newHeight);
    });

    glGenTextures(1, &m_Texture);
    glBindTexture(GL_TEXTURE_2D, m_Texture);

    // Nearest filtering: the texture is displayed at exactly its own size, so any
    // interpolation would only blur the rendered pixels.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

/**
 * destructor for the GLFW window
 */
Window::~Window()
{
    if (m_Texture != 0)
    {
        glDeleteTextures(1, &m_Texture);
    }

    glfwDestroyWindow(m_Window);
    glfwTerminate();
}

/**
 * returns the current size of the GLFW window
 *
 * \return - the <width, height> window size pair
 */
auto Window::getSize() -> std::pair<int, int>
{
    return std::make_pair(m_Data.m_Width, m_Data.m_Height);
}

/**
 * returns the current size of the GLFW window framebuffer
 * the framebuffer size and window size are not always the same
 *
 * \return - the <width, height> window framebuffer size pair
 */
auto Window::getFrameBufferSize() -> std::pair<int, int>
{
    return std::make_pair(m_Data.m_FBWidth, m_Data.m_FBHeight);
}

void Window::setTitle(const std::string &title)
{
    glfwSetWindowTitle(m_Window, title.c_str());
}

auto Window::consumeResized() -> bool
{
    const bool resized = m_Data.m_Resized;
    m_Data.m_Resized = false;
    return resized;
}

auto Window::resetRequested() -> bool
{
    const bool requested = m_Data.m_ResetRequested;
    m_Data.m_ResetRequested = false;
    return requested;
}

void Window::present()
{
    if (m_Data.m_PixelBuffer == nullptr)
    {
        return;
    }

    const auto size = m_Data.m_PixelBuffer->getSize();
    const int width = size.first;
    const int height = size.second;

    if (width <= 0 || height <= 0)
    {
        return;
    }

    const float *linear = m_Data.m_PixelBuffer->getPixels();
    const auto pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);

    m_DisplayPixels.resize(pixelCount * 3);

    // Tone map and encode exactly as the file writer does. Without this the window shows
    // raw linear radiance, which is both too dark and hard-clipped, and looks nothing
    // like the image the same scene produces when saved.
    for (size_t p = 0; p < pixelCount; p++)
    {
        const std::array<float, 3> mapped = ImageIO::toneMap(&linear[p * 3]);

        for (size_t c = 0; c < 3; c++)
        {
            m_DisplayPixels[(p * 3) + c] =
                static_cast<unsigned char>((ImageIO::linearToSrgb(mapped[c]) * 255.0f) + 0.5f);
        }
    }

    glBindTexture(GL_TEXTURE_2D, m_Texture);

    // Reallocate only when the size changes; every other frame just overwrites the
    // existing storage.
    if (width != m_TextureWidth || height != m_TextureHeight)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, m_DisplayPixels.data());
        m_TextureWidth = width;
        m_TextureHeight = height;
    }
    else
    {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, m_DisplayPixels.data());
    }

    // Draw the texture over the whole window. This replaces glDrawPixels, which is both
    // deprecated and, on most drivers, a slow path that bypasses the texture hardware.
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_TEXTURE_2D);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, 1.0, 0.0, 1.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Row 0 of the render buffer is the bottom of the image, which is why the texture
    // coordinates run upwards here.
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(0.0f, 0.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(1.0f, 0.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(1.0f, 1.0f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(0.0f, 1.0f);
    glEnd();

    glDisable(GL_TEXTURE_2D);
}

/**
 * the window update function run every frame of the application
 */
void Window::update()
{
    present();

    glfwSwapBuffers(m_Window);
    glfwPollEvents();
}

/**
 * sets the pixel buffer the window will use to read what pixel to draw
 *
 * \param pixelBuffer - the pixel buffer object to copy from
 */
void Window::setPixelBuffer(PixelBuffer *pixelBuffer)
{
    m_Data.m_PixelBuffer = pixelBuffer;
}

/**
 * sets the ray tracer that will color the pixels of the window
 *
 * \param rayTracer - the ray tracer object that will color the pixels
 */
void Window::setRayTracer(RayTracer *rayTracer)
{
    m_Data.m_RayTracer = rayTracer;
}

void Window::pollEvents()
{
    glfwPollEvents();
}

void Window::keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    (void)scancode;
    (void)mods;

    auto *data = static_cast<WindowData *>(glfwGetWindowUserPointer(window));

    if (action != GLFW_PRESS)
    {
        return;
    }

    if (key == GLFW_KEY_ESCAPE)
    {
        data->m_Closed = true;
    }
    else if (key == GLFW_KEY_R)
    {
        data->m_ResetRequested = true;
    }
}
