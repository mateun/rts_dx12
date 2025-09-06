#include "game.h"
#include "engine.h"

void Game::setEvents(std::vector<Event*> events)
{
    this->frameEvents = events;
}