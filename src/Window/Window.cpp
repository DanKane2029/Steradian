#include "Window.h"
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
Window::Window(std::string title, unsigned int width, unsigned int height)
	: m_Title(title)
{
	// Set Error Callback
	glfwSetErrorCallback([](int error, const char *description)
						 { printf("Error: %s\n", description); });

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

	glfwGetFramebufferSize(m_Window, (int *)&m_Data.m_FBWidth, (int *)&m_Data.m_FBHeight);

	// Set Input Callbacks
	glfwSetWindowCloseCallback(m_Window, [](GLFWwindow *window)
							   {
		WindowData* data = (WindowData*)glfwGetWindowUserPointer(window);
		data->m_Closed = true; });

	glfwSetWindowSizeCallback(m_Window, [](GLFWwindow *window, int width, int height)
							  {
		WindowData* data = (WindowData*)glfwGetWindowUserPointer(window);

		data->m_Width = width;
		data->m_Height = height; });

	glfwSetKeyCallback(m_Window, KeyCallback);

	glfwSetFramebufferSizeCallback(m_Window, [](GLFWwindow *window, int width, int height)
								   {
		WindowData* data = (WindowData*)glfwGetWindowUserPointer(window);

		data->m_FBWidth = width;
		data->m_FBHeight = height;

		data->m_PixelBuffer->ResizeBuffer(width, height);

		glViewport(0, 0, width, height); });
}

/**
 * destructor for the GLFW window
 */
Window::~Window()
{
	glfwDestroyWindow(m_Window);
}

/**
 * returns the current size of the GLFW window
 *
 * \return - the <width, height> window size pair
 */
std::pair<unsigned int, unsigned int> Window::GetSize()
{
	return std::make_pair(m_Data.m_Width, m_Data.m_Height);
}

/**
 * returns the current size of the GLFW window framebuffer
 * the framebuffer size and window size are not always the same
 *
 * \return - the <width, height> window framebuffer size pair
 */
std::pair<unsigned int, unsigned int> Window::GetFrameBufferSize()
{
	return std::make_pair(m_Data.m_FBWidth, m_Data.m_FBHeight);
}

/**
 * the window update function run every frame of the application
 * TODO: glDrawPixels is deprecated in OpenGL 4, change to more modern way
 *		by useing a framebuffer object
 */
void Window::Update()
{
	glDrawPixels(m_Data.m_Width, m_Data.m_Height, GL_RGB, GL_FLOAT, m_Data.m_PixelBuffer->GetPixels());
	glfwSwapBuffers(m_Window);
	glfwPollEvents();
}

/**
 * sets the pixel buffer the window will use to read what pixel to draw
 *
 * \param pixelBuffer - the pixel buffer object to copy from
 */
void Window::SetPixelBuffer(PixelBuffer *pixelBuffer)
{
	m_Data.m_PixelBuffer = pixelBuffer;
}

void Window::PollEvents()
{
	glfwPollEvents();
}

void Window::KeyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
	WindowData *data = reinterpret_cast<WindowData *>(glfwGetWindowUserPointer(window));

	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
	{
		data->m_Closed = true;
	}
}
