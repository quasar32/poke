#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "misc.h"
#include "state.h"

static int GameStateCount;
static game_state *GameStates[16];

void PopState(void) {
    assert(GameStateCount > 0);
    --GameStateCount;
}

void PushState(game_state *NewState) {
    assert(GameStateCount < countof(GameStates));
    GameStates[GameStateCount++] = NewState; 
}

void ReplaceState(game_state *NewState) {
    GameStates[GameStateCount - 1] = NewState; 
}

game_state *GetState(void) {
    assert(GameStateCount > 0);
    return GameStates[GameStateCount - 1]; 
}

void ExecuteState(void) {
    GetState()();
}
