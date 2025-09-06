
#pragma once
#include "../engine/engine.h"
#include "../engine/game.h"
#include "../engine/renderer.h"

struct Window;
class RTSGame : public Game {

    public:
        RenderInitData getInitData(CommandLine cmdline, Window* window) override;
        FrameSubmission getFrameData() override;

    protected:
        Window* window = nullptr;
};