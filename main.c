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

#define FRAME_RATE 30000

char user_string[40] = "Type up to 40 characters";
int new_str = 1;

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

void draw_woe_frolic_dread_and_malice(int x, int y, int w, int h, int percentage, int idx) {

    // Top Rect
    char index[2] = {0, 0};
    index[1] = '0' + idx;
    drawRect(x, y, w, h, CYAN);
    setCursor(x + (w / 2), y + (h / 2));
    setTextSize(1);
    setTextColor(WHITE);
    writeString(index);

    // Bottom Rect
    drawRect(x, y + (h + 2), w, h, CYAN);
}

// ==================================================
// === graphics demo -- RUNNING on core 0
// ==================================================
static PT_THREAD(protothread_graphics(struct pt *pt)) {
    PT_BEGIN(pt);
    game_state_init(&game_state);
    static int begin_time;
    static int spare_time;

    // Variables for grid drawing
    static const int cell_width = 40;   // Width of each grid cell
    static const int cell_height = 40;  // Height of each grid cell
    static const int grid_start_x = 10; // Starting X position of the grid
    static int grid_start_y = 60;       // Starting Y position of the grid
    static char num_str[2] = {0, 0};    // String to hold the number (plus null terminator)
    static char percent_str[15];        // For percentage display

    // Clear the screen first
    fillRect(0, 0, 640, 480, BLACK);

    // Progress bar

    int progress_bar_width = (COLS * cell_width);
    int progress_bar_fill_width = progress_bar_width / 2; // For 50%
    int progress_bar_y = 10;
    int progress_bar_height = 30;
    int progress_bar_x = grid_start_x + 10; // x = 20

    fillRect(progress_bar_x, progress_bar_y, progress_bar_width, progress_bar_height, WHITE);
    fillRect(progress_bar_x + 5, progress_bar_y + 5, progress_bar_fill_width, progress_bar_height - 10, BLACK); // Black fill (50%)
    setCursor(progress_bar_x + 10, progress_bar_y + 10);
    setTextColor(WHITE);
    setTextSize(2);
    writeString("OCULA");

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

    grid_start_y = progress_bar_y + progress_bar_height + 20; // Start grid below
    // draw straight line at the top
    drawHLine(grid_start_x, grid_start_y, COLS * cell_width, GREEN);
    // draw straight line at the bottom
    drawHLine(grid_start_x, grid_start_y + ((ROWS + 1) * cell_height), COLS * cell_width, GREEN);
    // move grid down

    int wfdm_width = 60;
    int wfdm_height = 10;
    int wfdm_y = 420;
    int wfdm_start_x = 80;

    while (true) {
        begin_time = time_us_32();

        // Draw the numbers from the game state
        for (int row = 0; row < ROWS; row++) {
            for (int col = 0; col < COLS; col++) {
                // calculate position
                int x = grid_start_x + (col * cell_width);
                int y = grid_start_y + (row * cell_height);

                // convert number to string
                num_str[0] = '0' + game_state.state[row][col];

                // set text properties
                setCursor(x + cell_width / 2, y + cell_height / 2); // center text in cell
                setTextColor2(WHITE, BLACK);
                setTextSize(1);

                // draw the number
                writeString(num_str);
            }
        }

        // First update the game state
        game_state_update_woe_frolic_dread_and_malice(&game_state.woe, wfdm_start_x, wfdm_y, wfdm_width, wfdm_height, 50);
        game_state_update_woe_frolic_dread_and_malice(&game_state.frolic, wfdm_start_x + wfdm_width + 80, wfdm_y, wfdm_width, wfdm_height, 50);
        game_state_update_woe_frolic_dread_and_malice(&game_state.dread, wfdm_start_x + (wfdm_width + 80) * 2, wfdm_y, wfdm_width, wfdm_height, 50);
        game_state_update_woe_frolic_dread_and_malice(&game_state.malice, wfdm_start_x + (wfdm_width + 80) * 3, wfdm_y, wfdm_width, wfdm_height, 50);

        // Draw the woe frolic dread and malice boxes
        draw_woe_frolic_dread_and_malice(game_state.woe.x, game_state.woe.y, game_state.woe.width, game_state.woe.height, game_state.woe.percentage, 0);
        draw_woe_frolic_dread_and_malice(game_state.frolic.x, game_state.frolic.y, game_state.frolic.width, game_state.frolic.height, game_state.frolic.percentage, 1);
        draw_woe_frolic_dread_and_malice(game_state.dread.x, game_state.dread.y, game_state.dread.width, game_state.dread.height, game_state.dread.percentage, 2);
        draw_woe_frolic_dread_and_malice(game_state.malice.x, game_state.malice.y, game_state.malice.width, game_state.malice.height, game_state.malice.percentage, 3);

        spare_time = FRAME_RATE - (time_us_32() - begin_time);
        PT_YIELD_usec(spare_time);
    }
    PT_END(pt);
} // graphics thread

// ==================================================
// === toggle25 thread on core 0
// ==================================================
// the on-board LED blinks
static PT_THREAD(protothread_toggle25(struct pt *pt)) {
    PT_BEGIN(pt);
    static bool LED_state = false;

    // set up LED p25 to blink
    gpio_init(25);
    gpio_set_dir(25, GPIO_OUT);
    gpio_put(25, true);
    // data structure for interval timer
    PT_INTERVAL_INIT();

    while (1) {
        // yield time 0.1 second
        // PT_YIELD_usec(100000) ;
        PT_YIELD_INTERVAL(100000);

        // toggle the LED on PICO
        LED_state = LED_state ? false : true;
        gpio_put(25, LED_state);
        //
        // NEVER exit while
    } // END WHILE(1)
    PT_END(pt);
} // blink thread

// ========================================
// === core 1 main -- started in main below
// ========================================
void core1_main() {
    //
    //  === add threads  ====================
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
    pt_add_thread(protothread_toggle25);
    //
    // === initalize the scheduler ===============
    pt_schedule_start;
    // NEVER exits
    // ===========================================
} // end main