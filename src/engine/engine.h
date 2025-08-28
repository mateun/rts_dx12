#pragma once

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
 * According to the dx12 philosophy, the initial data is used to 
 * setup every shader and PSO ever needed during the game. 
 * So the game must know what it needs for the entire game. 
 * We completely avoid any runtime "discovery" of new pipeline states. 
 * This would lead to stuttering during gameplay.
 */


/// @brief Must be implemented by ONE game per codebase.
/// @return A pointer to a game object. The engine will call the interface methods
///         on it.
class Game;
Game* getGame();