#include "Window.h"
#include <iostream>

int main()
{
    const unsigned int width = 500;
    const unsigned int height = 500;
    
    Window window("Path Tracer", width, height);

    while (!window.ShouldClose())
    {
        window.Update();
    }
}