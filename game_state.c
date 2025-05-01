#include "game_state.h"
#include "vga16_graphics.h"
#include <stdlib.h>
#include <time.h>



void game_state_init(GameState *state) {
    srand(time(NULL));

    for (int row = 0; row < ROWS; row++) {
        for (int col = 0; col < COLS; col++) {
            state->state[row][col].number = rand() % 10;
            state->state[row][col].x = GRID_START_X + (col * CELL_WIDTH);
            state->state[row][col].y = GRID_START_Y + (row * CELL_HEIGHT);
            state->state[row][col].size = 1;
            state->state[row][col].animated_last_frame = 0;
        }
    }
    spawn_boid(&state->boids[0], 0);
    spawn_boid(&state->boids[1], 1);

    /*START OF CODE ADDED ON 4/27*/

    state->selected_row = rand() % ROWS;
    state->selected_col = rand() % COLS;

    /*END OF CODE ADDED ON 4/27*/
}

void game_state_update(GameState *state) {
    srand(time(NULL));

    for (int row = 0; row < ROWS; row++) {
        for (int col = 0; col < COLS; col++) {
            state->state[row][col].number = rand() % 10;
            state->state[row][col].x = GRID_START_X + (col * CELL_WIDTH);
            state->state[row][col].y = GRID_START_Y + (row * CELL_HEIGHT);
        }
    }
}

void game_state_update_boxes(Box *state, int x, int y, int w, int h, int percentage) {
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

void spawn_boid(Boid *boid, int group_id) {
    // Start in center of screen
    boid->x = int2fix15(320);
    boid->y = int2fix15(240);

    boid->vx = ((rand() & 0xFFFF) * 3) - int2fix15(3);
    boid->vy = ((rand() & 0xFFFF) * 3) - int2fix15(3);

    // Assign scout group and initial bias
    boid->scout_group = group_id;
    if (group_id == 0) {
        boid->biasval = BIAS_VAL_GROUP1;
    } else if (group_id == 1) {
        boid->biasval = BIAS_VAL_GROUP2;
    }
}

void update_boids(GameState *state) {
    for (int i = 0; i < NUM_BOIDS; i++) {

        // Zero all accumulators variables
        fix15 xpos_avg = 0;
        fix15 ypos_avg = 0;
        fix15 xvel_avg = 0;
        fix15 yvel_avg = 0;
        fix15 neighboring_boids = 0;
        fix15 close_dx = 0;
        fix15 close_dy = 0;

        for (int j = 0; j < NUM_BOIDS; j++) {
            if (i == j)
                continue;

            // Compute differences in x and y coordinates
            fix15 dx = state->boids[i].x - state->boids[j].x;
            fix15 dy = state->boids[i].y - state->boids[j].y;

            // Approximate distance using Alpha max plus beta min
            fix15 abs_dx = absfix15(dx);
            fix15 abs_dy = absfix15(dy);
            fix15 distance;
            if (abs_dx > abs_dy) {
                distance = abs_dx + (abs_dy >> 2);
            } else {
                distance = abs_dy + (abs_dx >> 2);
            }

            // Is the other boid within the protected range?
            if (distance < PROTECTED_RANGE) {
                // If so, calculate difference in x/y-coordinates for separation
                // Steer away from the other boid
                close_dx += dx;
                close_dy += dy;
            }
            // If not in protected range, is the boid in the visual range ?
            else if (distance < VISUAL_RANGE) {
                // Add other boid's x/y-coord and x/y vel to accumulator variables
                xpos_avg += state->boids[j].x;
                ypos_avg += state->boids[j].y;
                xvel_avg += state->boids[j].vx;
                yvel_avg += state->boids[j].vy;

                // Increment number of boids within visual range
                neighboring_boids += int2fix15(1);
            }
        }

        // If there were any boids in the visual range
        if (neighboring_boids > 0) {
            // Divide accumulator variables by number of boids in visual range
            xpos_avg = divfix(xpos_avg, neighboring_boids);
            ypos_avg = divfix(ypos_avg, neighboring_boids);
            xvel_avg = divfix(xvel_avg, neighboring_boids);
            yvel_avg = divfix(yvel_avg, neighboring_boids);

            // Add the centering/matching contributions to velocity
            state->boids[i].vx += multfix15(xpos_avg - state->boids[i].x, CENTERING_FACTOR) +
                                  multfix15(xvel_avg - state->boids[i].vx, MATCHING_FACTOR);

            state->boids[i].vy += multfix15(ypos_avg - state->boids[i].y, CENTERING_FACTOR) +
                                  multfix15(yvel_avg - state->boids[i].vy, MATCHING_FACTOR);
        }

        // Add the avoidance contribution to velocity
        state->boids[i].vx += multfix15(close_dx, AVOID_FACTOR);
        state->boids[i].vy += multfix15(close_dy, AVOID_FACTOR);

        // If the boid is near an edge, make it turn by turnfactor
        if (hitTop(state->boids[i].y)) {
            state->boids[i].vy += TURN_FACTOR;
        }
        if (hitRight(state->boids[i].x)) {
            state->boids[i].vx -= TURN_FACTOR;
        }
        if (hitLeft(state->boids[i].x)) {
            state->boids[i].vx += TURN_FACTOR;
        }
        if (hitBottom(state->boids[i].y)) {
            state->boids[i].vy -= TURN_FACTOR;
        }

        // Bias for scout groups
        // biased to right of screen
        if (state->boids[i].scout_group == 0) { // Scout group 1 (biased right)
            if (state->boids[i].vx > 0) {       // Moving right, increase bias
                state->boids[i].biasval += BIAS_INCREMENT;
                if (state->boids[i].biasval > MAX_BIAS) {
                    state->boids[i].biasval = MAX_BIAS;
                }
            } else { // Moving left or stationary, decrease bias
                state->boids[i].biasval -= BIAS_INCREMENT;
                if (state->boids[i].biasval < BIAS_INCREMENT) {
                    state->boids[i].biasval = BIAS_INCREMENT;
                }
            }
        } else if (state->boids[i].scout_group == 1) { // Scout group 2 (biased left)
            if (state->boids[i].vx < 0) {              // Moving left, increase bias
                state->boids[i].biasval += BIAS_INCREMENT;
                if (state->boids[i].biasval > MAX_BIAS) {
                    state->boids[i].biasval = MAX_BIAS;
                }
            } else { // Moving right or stationary, decrease bias
                state->boids[i].biasval -= BIAS_INCREMENT;
                if (state->boids[i].biasval < BIAS_INCREMENT) { // Ensure bias doesn't go below minimum increment step
                    state->boids[i].biasval = BIAS_INCREMENT;
                }
            }
        }

        // Apply the bias using the boid's individual biasval
        if (state->boids[i].scout_group == 1) {
            state->boids[i].vx = multfix15(int2fix15(1) - state->boids[i].biasval, state->boids[i].vx) +
                                 multfix15(state->boids[i].biasval, int2fix15(1));
        } else if (state->boids[i].scout_group == 2) {
            state->boids[i].vx = multfix15(int2fix15(1) - state->boids[i].biasval, state->boids[i].vx) +
                                 multfix15(state->boids[i].biasval, int2fix15(-1));
        }

        // Calculate the boid's speed using Alpha max plus beta min
        fix15 abs_vx = absfix15(state->boids[i].vx);
        fix15 abs_vy = absfix15(state->boids[i].vy);
        fix15 speed;
        if (abs_vx > abs_vy) {
            speed = abs_vx + (abs_vy >> 2);
        } else {
            speed = abs_vy + (abs_vx >> 2);
        }

        // Enforce min and max speed
        if (speed > MAX_SPEED) {
            state->boids[i].vx = multfix15(divfix(state->boids[i].vx, speed), MAX_SPEED);
            state->boids[i].vy = multfix15(divfix(state->boids[i].vy, speed), MAX_SPEED);
        }
        if (speed < MIN_SPEED) {
            // Avoid division by zero or very small numbers if speed is close to zero
            if (speed == 0) {
                // Give it a small random velocity if speed is exactly zero
                state->boids[i].vx = ((rand() & 0xFFFF) * 3) - int2fix15(3);
                state->boids[i].vy = ((rand() & 0xFFFF) * 3) - int2fix15(3);
                speed = MIN_SPEED; // Set speed to min speed to normalize
            }
            state->boids[i].vx = multfix15(divfix(state->boids[i].vx, speed), MIN_SPEED);
            state->boids[i].vy = multfix15(divfix(state->boids[i].vy, speed), MIN_SPEED);
        }

        // Update boid's position
        state->boids[i].x += state->boids[i].vx;
        state->boids[i].y += state->boids[i].vy;
    }
}

// Helper function for animation of numbers.
void animate_numbers(Number *num, fix15 dx, fix15 dy, fix15 threshold_x, fix15 threshold_y) {
    // Calculate the distance to move based on the collision
    fix15 shift_x = ((rand() & 0xFFFF) * 3) - int2fix15(3);
    fix15 shift_y = ((rand() & 0xFFFF) * 3) - int2fix15(3);

    // erase numbers first
    fillRect(num->x, num->y, CELL_WIDTH, CELL_HEIGHT, BLACK);
    num->animated_last_frame = 1;

    // Update the number's position
    num->x += fix2int15(shift_x);
    num->y += fix2int15(shift_y);
    num->size = 2;
}

void check_collisions_and_animate(GameState *state) {

    fix15 cell_width = int2fix15(CELL_WIDTH);
    fix15 cell_height = int2fix15(CELL_HEIGHT);
    fix15 half_cell_width = divfix(cell_width, int2fix15(2));
    fix15 half_cell_height = divfix(cell_height, int2fix15(2));
    fix15 boid_radius_fix = int2fix15(BOID_COLLISION_RADIUS);

    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {

            // Calculate cell center coordinates (fixed point)
            fix15 cell_center_x = int2fix15(state->state[i][j].x) + half_cell_width;
            fix15 cell_center_y = int2fix15(state->state[i][j].y) + half_cell_height;

            // Check collision with each boid
            for (int k = 0; k < NUM_BOIDS; k++) {
                // Get boid center coordinates

                fix15 dx = state->boids[k].x - cell_center_x;
                fix15 dy = state->boids[k].y - cell_center_y;

                fix15 abs_dx = absfix15(dx);
                fix15 abs_dy = absfix15(dy);

                fix15 threshold_x = half_cell_width + boid_radius_fix;
                fix15 threshold_y = half_cell_height + boid_radius_fix;

                if ((abs_dx < threshold_x) && (abs_dy < threshold_y)) {
                    // Collision detected! Animate the number.
                    animate_numbers(&state->state[i][j], dx, dy, threshold_x, threshold_y);
                    break;
                }
            }
        }
    }
}