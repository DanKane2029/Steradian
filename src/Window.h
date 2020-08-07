#pragma once
#include <GLFW/glfw3.h>
#include <String>

struct WindowData
{
	unsigned int m_Width = 0, m_Height = 0;
	bool m_Closed = false;
};

class Window
{
public:
	Window(std::string title, unsigned int width, unsigned int height);
	~Window();

	GLFWwindow* GetContext() const { return m_Window; };
	bool ShouldClose() const { return m_Data.m_Closed; };

	void Update();

private:
	std::string m_Title;
	
	WindowData m_Data;
	
	GLFWwindow* m_Window;
};