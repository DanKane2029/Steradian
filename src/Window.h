#pragma once
#include "PixelBuffer.h"

#include <GLFW/glfw3.h>
#include <String>

class Window
{
public:
	Window(std::string title, unsigned int width, unsigned int height);
	~Window();

	GLFWwindow* GetContext() const { return m_Window; };
	bool ShouldClose() const { return m_Data.m_Closed; };

	std::pair<unsigned int, unsigned int> GetSize();
	std::pair<unsigned int, unsigned int> GetFrameBufferSize();

	void UpdatePixels(void* pixelData);
	void Update();

	void SetPixelBuffer(PixelBuffer* pixelBuffer);

private:
	std::string m_Title;
	
	struct WindowData
	{
		unsigned int m_Width = 0, m_Height = 0;
		unsigned int m_FBWidth = 0, m_FBHeight = 0;
		bool m_Closed = false;
		std::shared_ptr<PixelBuffer> m_PixelBuffer;
	};

	WindowData m_Data;
	
	GLFWwindow* m_Window;	
};