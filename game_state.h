#include "pico/divider.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

#ifndef GAME_STATE_H
#define GAME_STATE_H

#define ROWS 7
#define COLS 15

#define NUM_BOIDS 2

// === the fixed point macros ========================================
typedef signed int fix15;
#define multfix15(a, b) ((fix15)((((signed long long)(a)) * ((signed long long)(b))) >> 15))
#define float2fix15(a) ((fix15)((a) * 32768.0)) // 2^15
#define fix2float15(a) ((float)(a) / 32768.0)
#define absfix15(a) abs(a)
#define int2fix15(a) ((fix15)(a << 15))
#define fix2int15(a) ((int)(a >> 15))
#define char2fix15(a) (fix15)(((fix15)(a)) << 15)
#define divfix(a, b) (fix15)(div_s64s64((((signed long long)(a)) << 15), ((signed long long)(b))))

// Boid parameters
#define VISUAL_RANGE float2fix15(40.0)
#define PROTECTED_RANGE float2fix15(8.0)
#define CENTERING_FACTOR float2fix15(0.0005)
#define AVOID_FACTOR float2fix15(0.7)
#define MATCHING_FACTOR float2fix15(0.005)
#define TURN_FACTOR float2fix15(0.02)
#define MAX_SPEED float2fix15(2.0)
#define MIN_SPEED float2fix15(1.0)
#define BIAS_VAL_GROUP1 float2fix15(0.001)
#define BIAS_VAL_GROUP2 float2fix15(0.001)
#define BIAS_INCREMENT float2fix15(0.00004) // For dynamic bias
#define MAX_BIAS float2fix15(0.01)
#define BOID_RADIUS 4

// Screen margins (assuming 640x480 screen)
#define LEFT_MARGIN int2fix15(10)
#define RIGHT_MARGIN int2fix15(620) // 640 - 20
#define TOP_MARGIN int2fix15(200)
#define BOTTOM_MARGIN int2fix15(280) // 480 - 100
#define GRID_START_X 10
#define GRID_START_Y 80
#define CELL_WIDTH 40
#define CELL_HEIGHT 40
#define MAX_PIXEL_SHIFT int2fix15(10)

// Define a maximum possible number of bad groups
#define MAX_BAD_GROUPS (ROWS * COLS)

// Numbers within this radius of the boid are considered surrounding the boid
#define BOID_COLLISION_RADIUS 40

// Scout group definitions (example: first 2 boids are group 1, next 2 are group 2)
#define NUM_SCOUTS_GROUP1 5
#define NUM_SCOUTS_GROUP2 5

// Wall detection
#define hitBottom(b) (b > BOTTOM_MARGIN)
#define hitTop(b) (b < TOP_MARGIN)
#define hitLeft(a) (a < int2fix15(100))
#define hitRight(a) (a > int2fix15(540))

#define BOX_ANIM_INCREMENT 5
#define BOX_ANIM_MAX_HEIGHT 50
#define BOX_ANIM_PERCENTAGE_INCREMENT 5

// Define the game state structure
typedef struct {
    int x;
    int y;
    int width;
    int height;
    int percentage;
} Box;

typedef enum {
    ANIM_IDLE,
    ANIM_GROWING,
    ANIM_SHRINKING
} AnimationState;

typedef struct {
    int x;
    int y;
    int width;
    int height;
    int current_anim_height;
    AnimationState anim_state;
    int woe_percentage;
    int frolic_percentage;
    int dread_percentage;
    int malice_percentage;
} BoxAnim;

typedef struct {
    fix15 x;
    fix15 y;
    fix15 vx;
    fix15 vy;
    fix15 biasval;
    int scout_group; // 0: group 1, 1: group 2
} Boid;

typedef struct {
    int bin_id;
    int bad_group_id;
} BadNumber;

typedef struct {
    int x;
    int y;
    int number;
    int size;
    int animated_last_frame;
    bool is_bad_number;
    BadNumber bad_number;
} Number;

typedef struct {
    int x;
    int y;
    int width;
    int height;
} Cursor;

typedef enum {
    START_SCREEN,
    PLAYING,
    GAME_WON
} PlayState;

typedef struct {
    Number state[ROWS][COLS];
    Box boxes[5];
    Boid boids[NUM_BOIDS];
    BoxAnim box_anims[5];
    Cursor cursor;
    PlayState play_state;
} GameState;

// Function declarations
void game_state_init(GameState *state, int seed);
void game_state_update(GameState *state);
void game_state_draw(GameState *state);
void game_state_update_boxes(Box *state, int x, int y, int w, int h, int percentage);
void spawn_boid(Boid *boid, int group_id);
void update_boids(GameState *state);
void check_collisions_and_animate(GameState *state);
void animate_numbers(Number *num, fix15 dx, fix15 dy, fix15 shift_x, fix15 shift_y);
void group_bad_numbers(GameState *state);
#endif // GAME_STATE_H