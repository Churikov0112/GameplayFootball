// written by bastiaan konings schuiling 2008 - 2015
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

// Game context: global state shared by the game (main.cpp) and headless tools
// (determinism_runner). Declarations stay in main.hpp so that gameplay code
// (e.g. Match) can call the same getters; definitions live here so that a
// separate tool binary does not have to link main.cpp (which defines main()).

#ifndef _HPP_GAMECONTEXT
#define _HPP_GAMECONTEXT

#include "main.hpp"

bool InitGameSystems(Properties &config);
bool InitGameContext(Properties &config);
void ShutdownGameContext();

#endif
