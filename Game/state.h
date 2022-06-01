#ifndef STATE_H
#define STATE_H

typedef void game_state(void);

void PopState(void);
void PushState(game_state *NewState);
void ReplaceState(game_state *NewState);
void ExecuteState();
game_state *GetState(void);

#endif
