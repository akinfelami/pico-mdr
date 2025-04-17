#ifndef GAME_STATE_H
#define GAME_STATE_H

#define ROWS 7
#define COLS 15

// Define the game state structure
typedef struct {
    int x;
    int y;
    int width;
    int height;
    int percentage;
} WoeFrolicDreadAndMalice;
typedef struct {
    int state[ROWS][COLS];
    WoeFrolicDreadAndMalice woe;
    WoeFrolicDreadAndMalice frolic;
    WoeFrolicDreadAndMalice dread;
    WoeFrolicDreadAndMalice malice;
} GameState;



// Function declarations
void game_state_init(GameState *state);
void game_state_update(GameState *state);
void game_state_draw(GameState *state);
void game_state_update_woe_frolic_dread_and_malice(WoeFrolicDreadAndMalice *state, int x, int y, int w, int h, int percentage);

#endif // GAME_STATE_H