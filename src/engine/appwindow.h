#pragma once
#include <Windows.h>
#include <string>
#include <vector>
#include "engine.h"

struct Window 
{
    int width;
    int height;
    HWND hwnd;   // e.g. HWND on win32, X11 window on Linux etc.

};

std::vector<Event*> pollWindowMessages(Window window);

Window createAppWindow(int width, int height, bool fullscreen);
