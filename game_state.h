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
} Box;
typedef struct {
    int state[ROWS][COLS];
    Box boxes[5];
} GameState;



// Function declarations
void game_state_init(GameState *state);
void game_state_update(GameState *state);
void game_state_draw(GameState *state);
void game_state_update_boxes(Box *state, int x, int y, int w, int h, int percentage);

#endif // GAME_STATE_H