#include "game_state.h"
#include <stdlib.h>
#include <time.h>

void game_state_init(GameState *state) {
    srand(time(NULL));

    for (int row = 0; row < ROWS; row++) {
        for (int col = 0; col < COLS; col++) {
            state->state[row][col] = rand() % 10;
        }
    }
}

void game_state_update(GameState *state) {
    // This function can be used to update the game state
    // For now, just randomly change a few cells
    for (int i = 0; i < 5; i++) {
        int row = rand() % ROWS;
        int col = rand() % COLS;
        state->state[row][col] = rand() % 10;
    }
}

void game_state_update_woe_frolic_dread_and_malice(WoeFrolicDreadAndMalice *state, int x, int y, int w, int h, int percentage) {
    state->x = x;
    state->y = y;
    state->width = w;
    state->height = h;
    state->percentage = percentage;
}

void game_state_draw(GameState *state) {
    // This function could be used to draw the game state
    // But for now, we're handling the drawing in the protothread_graphics function
    // So this is just a placeholder
}