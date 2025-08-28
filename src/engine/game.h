#pragma once
#include "renderer.h"

 struct CommandLine
 {
    int argc;
    char** args;

 };

struct Window;
class Game {

    public:
        virtual RenderInitData getInitData(CommandLine cmdline, Window window) = 0;
        virtual FrameData getFrameData() = 0;

};