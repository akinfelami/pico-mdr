#ifndef GAME_STATE_H
#define GAME_STATE_H

#define ROWS 7
#define COLS 15

// Define the game state structure
typedef struct {
  int state[ROWS][COLS];
} GameState;

// Function declarations
void game_state_init(GameState* state);
void game_state_update(GameState* state);
void game_state_draw(GameState* state);

#endif // GAME_STATE_H