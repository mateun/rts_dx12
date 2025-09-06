#pragma once
#include "renderer.h"


 struct CommandLine
 {
    int argc;
    char** args;

 };

struct Event;
struct Window;
class Game {

    public:
        virtual RenderInitData getInitData(CommandLine cmdline, Window* window) = 0;
        virtual FrameSubmission getFrameData() = 0;
        void setEvents(std::vector<Event*> events);

    protected:
        std::vector<Event*> frameEvents;

};