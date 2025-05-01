/**
 * Hunter Adams (vha3@cornell.edu)
 *
 * HARDWARE CONNECTIONS
 *  - GPIO 16 ---> VGA Hsync
 *  - GPIO 17 ---> VGA Vsync
 *  - GPIO 18 ---> 470 ohm resistor ---> VGA Green
 *  - GPIO 19 ---> 330 ohm resistor ---> VGA Green
 *  - GPIO 20 ---> 330 ohm resistor ---> VGA Blue
 *  - GPIO 21 ---> 330 ohm resistor ---> VGA Red
 *  - RP2040 GND ---> VGA GND
 *
 * RESOURCES USED
 *  - PIO state machines 0, 1, and 2 on PIO instance 0
 *  - DMA channels obtained by claim mechanism
 *  - 153.6 kBytes of RAM (for pixel color data)
 *
 * Protothreads v1.1.3
 * Threads:
 * core 0:
 * Graphics demo
 * blink LED25
 * core 1:
 * Toggle gpio 4
 * Serial i/o
 */
// ==========================================
// === VGA graphics library
// ==========================================
#include "game_state.h"
#include "vga16_graphics.h"
#include <stdio.h>
#include <stdlib.h> // For abs() function
// #include <math.h>
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"
// // Our assembled programs:
// // Each gets the name <pio_filename.pio.h>
// #include "hsync.pio.h"
// #include "vsync.pio.h"
// #include "rgb.pio.h"
#include "game_state.h"

// ==========================================
// === protothreads globals
// ==========================================
#include "hardware/sync.h"
#include "hardware/timer.h"
#include "pico/multicore.h"
#include "string.h"
// protothreads header
#include "pt_cornell_rp2040_v1_3.h"

#define FRAME_RATE 60000 // 60 FPS

GameState game_state;

// ==================================================
// === lumon logo : Pass the center of the logo and dimension (w, h)
// ==================================================
void draw_lumon_logo(int cx, int cy, int logo_w, int logo_h) {
    char fill_color = BLUE; // Dark blue for the globe fill
    char line_color = CYAN; // Bright cyan for outlines and text

    // Calculate radii for the ovals
    short outer_rx = logo_w / 2;
    short ry = logo_h / 2; //
    short middle_rx = (short)(outer_rx * 0.75);
    short inner_rx = (short)(outer_rx * 0.5);

    drawOval(cx, cy, outer_rx, ry, line_color);  // Outer oval
    drawOval(cx, cy, middle_rx, ry, line_color); // Middle oval
    drawOval(cx, cy, inner_rx, ry, line_color);  // Inner oval

    drawHLine(cx - middle_rx, cy - (ry - 5), logo_w * 0.75, line_color);
    drawHLine(cx - middle_rx, cy + (ry - 5), logo_w * 0.75, line_color);

    char text_str[] = "LUMON";
    setCursor(cx - (middle_rx + 2), cy - 5);
    setTextSize(2);
    setTextColor(CYAN);
    writeString(text_str);
}

void draw_boxes(int x, int y, int w, int h, int percentage, int idx) {

    // Top Rect (Index Display)
    char index[3] = {0, 0, 0}; // Increased size for two digits + null terminator
    // Format the index as a two-digit string (e.g., 00, 01, 02, 03)
    index[0] = '0'; // Always start with '0'
    index[1] = '0' + idx;
    drawRect(x, y, w, h, CYAN);
    setCursor(x + (w / 2) - 4,
              y + (h / 2) - 4); // Adjust cursor slightly for two digits
    setTextSize(1);
    setTextColor(WHITE);
    writeString(index);

    // Bottom Rect (Progress Bar)
    int bottom_y = y + h + 2;
    // Draw the background/outline of the progress bar
    drawRect(x, bottom_y, w, h, CYAN); // Outline is CYAN

    int fill_w = (w * percentage) / 100;
    // Ensure fill width doesn't exceed total width
    if (fill_w > w) {
        fill_w = w;
    }
    if (fill_w < 0) {
        fill_w = 0;
    }

    // Draw the filled portion representing the percentage
    if (fill_w > 0) {
        fillRect(x, bottom_y, fill_w, h, WHITE); // Fill is WHITE
    }

    // Draw the percentage text
    char percent_str[5];                      // Buffer for percentage string (e.g., "100%")
    sprintf(percent_str, "%d%%", percentage); // Format the percentage
    int text_width = strlen(percent_str) * 6; // Assuming font width of 6 pixels
    int text_x = x + 5;
    int text_y = bottom_y + (h / 2) - 4; // Adjust vertical position

    setCursor(text_x, text_y);
    setTextSize(1);
    setTextColor(BLACK);
    writeString(percent_str);
}

// ==================================================
// === graphics demo -- RUNNING on core 0
// ==================================================
static PT_THREAD(protothread_graphics(struct pt *pt)) {
    PT_BEGIN(pt);

    // ---- To Start the game; user has to press some button ---- //
    // Write the instructions on the screen
    setCursor(200, 240);
    setTextSize(2);
    setTextColor(WHITE);
    writeString("Press button to start!");

    // Wait for a button press (for now just a delay of 5 seconds)
    PT_YIELD_usec(5000000);

    game_state_init(&game_state, time_us_32());

    static int begin_time;
    static int spare_time;

    // Variables for grid drawing
    static const int cell_width = 40;   // Width of each grid cell
    static const int cell_height = 40;  // Height of each grid cell
    static const int grid_start_x = 10; // Starting X position of the grid
    static int grid_start_y = 60;       // Starting Y position of the grid
    static char num_str[2] = {
        0, 0};                   // String to hold the number (plus null terminator)
    static char percent_str[15]; // For percentage display

    // Clear the screen first
    fillRect(0, 0, 640, 480, BLACK);

    // Progress bar

    int progress_bar_width = (COLS * cell_width);
    int progress_bar_fill_width = progress_bar_width / 2; // For 50%
    int progress_bar_y = 20;
    int progress_bar_height = 30;
    int progress_bar_x = grid_start_x + 10; // x = 20

    drawRect(progress_bar_x, progress_bar_y, progress_bar_width,
             progress_bar_height, CYAN);
    fillRect(progress_bar_x, progress_bar_y, progress_bar_fill_width,
             progress_bar_height, WHITE); // WHITE fill (50%)
    setCursor(progress_bar_x + 10, progress_bar_y + 10);
    setTextColor(LIGHT_BLUE);
    setTextSize(2);
    writeString("Ocula");

    setCursor(progress_bar_x + progress_bar_fill_width + 10, progress_bar_y + 10);
    setTextColor(WHITE);
    setTextSize(1);
    writeString("50% Complete");

    // Lumon Logo - to the right of the progress bar
    int logo_w = 70;
    int logo_h = 40;
    int logo_cx = progress_bar_width - 10;
    int logo_cy = progress_bar_y + (progress_bar_height / 2);

    draw_lumon_logo(logo_cx, logo_cy, logo_w, logo_h);

    // Reset text properties for grid numbers
    setTextSize(1);
    setTextColor2(WHITE, BLACK);

    grid_start_y = progress_bar_y + progress_bar_height + 30; // Start grid below
    // draw straight line at the top
    // drawHLine(grid_start_x, grid_start_y, COLS * cell_width, CYAN);
    // draw straight line at the bottom
    // drawHLine(grid_start_x, grid_start_y + ((ROWS + 1) * cell_height),
    //           COLS * cell_width, CYAN);
    // move grid down

    // Draw final botton box
    fillRect(grid_start_x, 460, COLS * cell_width, 10, CYAN);
    setCursor(((COLS * cell_width) / 2) - 40, 460 + 1);
    setTextColor(BLACK);
    setTextSize(1);
    writeString("0x5D9EA : 0xB57135");

    int wfdm_width = 60;
    int wfdm_height = 10;
    int wfdm_y = 420;
    int wfdm_start_x = 40;

    // Spawn boids
    int random_index = rand() % 4;
    game_state.box_anims[random_index].anim_state = ANIM_GROWING;

    while (true) {
        begin_time = time_us_32();

        // Reset number positions, sizes, and animation flags before collision checks
        for (int row = 0; row < ROWS; row++) {
            for (int col = 0; col < COLS; col++) {
                if (game_state.state[row][col].animated_last_frame == 1) {
                    // Clear the area with the correct size
                    fillRect(game_state.state[row][col].x, game_state.state[row][col].y,
                             CELL_WIDTH, CELL_HEIGHT, BLACK);
                }
                game_state.state[row][col].x = GRID_START_X + (col * CELL_WIDTH);
                game_state.state[row][col].y = GRID_START_Y + (row * CELL_HEIGHT);
                game_state.state[row][col].size = 1;
                game_state.state[row][col].animated_last_frame = 0;
            }
        }

        // Update the boids
        update_boids(&game_state);

        // Check collisions and mark numbers for animation
        check_collisions_and_animate(&game_state);

        // Draw the numbers from the game state
        for (int row = 0; row < ROWS; row++) {
            for (int col = 0; col < COLS; col++) {
                // convert number to string
                num_str[0] = '0' + game_state.state[row][col].number;

                // set text properties
                setCursor(game_state.state[row][col].x + CELL_WIDTH / 2,
                          game_state.state[row][col].y + CELL_HEIGHT / 2); // center text in cell

                if (game_state.state[row][col].is_bad_number) {
                    setTextColor(RED);
                } else {
                    setTextColor(WHITE);
                }

                setTextSize(game_state.state[row][col].size);

                // draw the number
                writeString(num_str);
            }
        }

        //  update the game state
        for (int i = 0; i < 5; i++) {
            game_state_update_boxes(&game_state.boxes[i],
                                    wfdm_start_x + (wfdm_width + 60) * i, wfdm_y,
                                    wfdm_width, wfdm_height, 50);
        }

        // Draw the woe frolic dread and malice boxes
        for (int i = 0; i < 5; i++) {
            draw_boxes(game_state.boxes[i].x, game_state.boxes[i].y,
                       game_state.boxes[i].width, game_state.boxes[i].height,
                       game_state.boxes[i].percentage, i);
        }

        spare_time = FRAME_RATE - (time_us_32() - begin_time);
        PT_YIELD_usec(spare_time);
    }
    PT_END(pt);
} // graphics thread

// ==================================================
// === toggle25 thread on core 0
// ==================================================
// the on-board LED blinks
static PT_THREAD(protothread_graphics_too(struct pt *pt)) {
    PT_BEGIN(pt);
    static int spare_time;
    static int begin_time;
    static int i;
    static BoxAnim *anim;
    static Box *box;

    while (1) {
        begin_time = time_us_32();
        for (i = 0; i < 5; i++) {
            anim = &game_state.box_anims[i];
            box = &game_state.boxes[i];
            switch (anim->anim_state) {
            case ANIM_GROWING:
                drawRect(box->x,
                         box->y - anim->current_anim_height,
                         box->width,
                         BOX_ANIM_INCREMENT,
                         BLACK);
                anim->current_anim_height += BOX_ANIM_INCREMENT;
                if (anim->current_anim_height >= BOX_ANIM_MAX_HEIGHT) {
                    anim->current_anim_height = BOX_ANIM_MAX_HEIGHT;
                    // Wait a 3s before transitioning to shrinking
                    PT_YIELD_usec(3000000);
                    anim->anim_state = ANIM_SHRINKING;
                }
                // Draw the growing box
                drawRect(box->x,
                         box->y - anim->current_anim_height,
                         box->width,
                         anim->current_anim_height,
                         CYAN);
                break;
            case ANIM_SHRINKING:
                // Clear the top strip that's disappearing this frame
                drawRect(box->x,
                         box->y - anim->current_anim_height,
                         box->width,
                         BOX_ANIM_INCREMENT, // Height of the strip to clear
                         BLACK);
                anim->current_anim_height -= BOX_ANIM_INCREMENT;
                if (anim->current_anim_height <= 0) {
                    anim->current_anim_height = 0;
                    anim->anim_state = ANIM_IDLE;
                }
                // Draw the shrinking box
                drawRect(box->x,
                         box->y - anim->current_anim_height,
                         box->width,
                         anim->current_anim_height,
                         CYAN);
                break;

            case ANIM_IDLE:
                break;
            }
        }

        // NEVER exit while
        spare_time = FRAME_RATE - (time_us_32() - begin_time);
        PT_YIELD_usec(spare_time);
    } // END WHILE(1)
    PT_END(pt);
} // blink thread

// ========================================
// === core 1 main -- started in main below
// ========================================
void core1_main() {
    //
    //  === add threads  ====================
    pt_add_thread(protothread_graphics_too);
    pt_schedule_start;
}

// ========================================
// === core 0 main
// ========================================
int main() {
    // set the clock
    stdio_init_all();

    // Initialize the VGA screen
    initVGA();

    // start core 1 threads
    multicore_reset_core1();
    multicore_launch_core1(&core1_main);

    // === config threads ========================
    // for core 0
    pt_add_thread(protothread_graphics);
    //
    // === initalize the scheduler ===============
    pt_schedule_start;
    // NEVER exits
    // ===========================================
} // end main
