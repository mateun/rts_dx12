

/**
 * Defines the overall interface of the engine and
 * the game <-> engine interaction. 
 * 
 * The basic flow works like this:
 * - The engine is the actual application. 
 * - It needs the game to tell it what to do.
 * - There are 2 distinct phases where the engine asks the game for configuration information:
 * 1. Startup (once): engine -> game.getInitData()
 * 2. Each frame: engine -> game.getFrameData()
 * 
 * According to the DX12 (and in our case also DX11) philosophy, the initial data is used to 
 * setup every shader and PSO ever needed during the game. 
 * So the game must know what it needs for the entire game. 
 * We completely avoid any runtime "discovery" of new pipeline states. 
 * This would lead to stuttering during gameplay.
 */

 #pragma once
 #include <string>

 struct Event
 {
    std::string name;
    void* data = nullptr;

 };


/// @brief Must be implemented by ONE game per codebase.
/// @return A pointer to a game object. The engine will call the interface methods
///         on it.
class Game;
Game* getGame();