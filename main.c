
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
#include <math.h>
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
// // Our assembled programs:
// // Each gets the name <pio_filename.pio.h>
#include "hsync.pio.h"
#include "vsync.pio.h"
#include "rgb.pio.h"
#include "game_state.h"
#include "audio_samples.h"


// ==========================================
// === protothreads globals
// ==========================================
#include "hardware/sync.h"
#include "hardware/adc.h"
#include "hardware/timer.h"
#include "pico/multicore.h"

#include "string.h"
// protothreads header
#include "pt_cornell_rp2040_v1_3.h"

#define FRAME_RATE 60000

// SPI data
uint16_t DAC_data_1 ; // output value
uint16_t DAC_data_0 ; // output value

// DAC parameters (see the DAC datasheet)
// A-channel, 1x, active
#define DAC_config_chan_A 0b0011000000000000
// B-channel, 1x, active
#define DAC_config_chan_B 0b1011000000000000

//SPI configurations (note these represent GPIO number, NOT pin number)
#define spi0            ((spi_inst_t *)spi0_hw)
#define PIN_MISO        4
#define PIN_CS          5
#define PIN_SCK         6
#define PIN_MOSI        7
#define LDAC            8
#define SPI_PORT        spi0
#define ISR_GPIO        2
#define ALARM_NUM 0
#define ALARM_IRQ TIMER_IRQ_0
#define DELAY 20 // 1/Fs (in microseconds)
#define SAMPLE_COUNT (sizeof(audio_samples)/sizeof(audio_samples[0]))



// Accumulator variables for boids
char user_string[40] = "Type up to 40 characters";
int new_str = 1;

int ADC_GPIO_VX = 28;
int ADC_GPIO_VY = 27;
int BUTTON_PIN = 22;

int X_RIGHT_THRESHOLD = 3000;
int X_LEFT_THRESHOLD = 500;

int Y_DOWN_THRESHOLD = 3000;
int Y_UP_THRESHOLD = 500;

volatile int march_offset = 0;
volatile uint32_t audio_index = 0;

unsigned short DAC_data[SAMPLE_COUNT];


GameState game_state;

// static void alarm_irq(void){
//     //Assert a GPIO whenever we enter an interrupt 
//     gpio_put(ISR_GPIO, 1) ;
//     // Clear the alarm Irq 
//     hw_clear_bits(&timer_hw->intr, 1u << ALARM_NUM);
//     // Reset the alarm register
//     timer_hw->alarm[ALARM_NUM] = timer_hw->timerawl + DELAY ;

//     if(audio_index>SAMPLE_COUNT){
//         audio_index=0;
//     }
//     DAC_data_0 = audio_samples[audio_index];

//     spi_write16_blocking(SPI_PORT, &DAC_data_0, 1);

//     audio_index += 1;

//     gpio_put(ISR_GPIO, 0);


// }

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
    game_state_init(&game_state);
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
    drawHLine(grid_start_x, grid_start_y, COLS * cell_width, CYAN);
    // draw straight line at the bottom
    drawHLine(grid_start_x, grid_start_y + ((ROWS + 1) * cell_height),
              COLS * cell_width, CYAN);
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

    while (true) {
        begin_time = time_us_32();

        march_offset = (march_offset + 1) % 8;

        // Reset number positions to their grid locations before collision checks
        for (int row = 0; row < ROWS; row++) {
            for (int col = 0; col < COLS; col++) {
                if (game_state.state[row][col].animated_last_frame == 1) {
                    fillRect(game_state.state[row][col].x, game_state.state[row][col].y,
                             CELL_WIDTH, CELL_HEIGHT, BLACK);
                    game_state.state[row][col].animated_last_frame = 0;
                }
                game_state.state[row][col].x = GRID_START_X + (col * CELL_WIDTH);
                game_state.state[row][col].y = GRID_START_Y + (row * CELL_HEIGHT);
                game_state.state[row][col].size = 1;
            }
        }

        // Draw the numbers from the game state
        // for (int row = 0; row < ROWS; row++) {
        //     for (int col = 0; col < COLS; col++) {
        //         // convert number to string
        //         num_str[0] = '0' + game_state.state[row][col].number;

        //         // set text properties
        //         setCursor(game_state.state[row][col].x + CELL_WIDTH / 2,
        //                   game_state.state[row][col].y + CELL_HEIGHT / 2); // center text in cell
        //         setTextColor(WHITE);
        //         setTextSize(game_state.state[row][col].size);

        //         // draw the number
        //         writeString(num_str);
        //     }
        // }

        // Then update the boids
        update_boids(&game_state);

        // check collisions
        check_collisions_and_animate(&game_state);

        // Draw the numbers from the game state
        for (int row = 0; row < ROWS; row++) {
            for (int col = 0; col < COLS; col++) {
                // convert number to string
                num_str[0] = '0' + game_state.state[row][col].number;

                // set text properties
                setCursor(game_state.state[row][col].x + CELL_WIDTH / 2,
                          game_state.state[row][col].y + CELL_HEIGHT / 2); // center text in cell
                setTextColor(WHITE);
                setTextSize(game_state.state[row][col].size);

                // draw the number
                writeString(num_str);
            }
        }

        /*Added on 4/27*/

        int selected_x = game_state.state[game_state.selected_row][game_state.selected_col].x;
        int selected_y = game_state.state[game_state.selected_row][game_state.selected_col].y;

        for (int x = 0; x < CELL_WIDTH; x += DASH_LENGTH * 2) {
            int start_x = selected_x + (x + march_offset) % CELL_WIDTH;
            // Draw on top edge
            drawHLine(start_x, selected_y, DASH_LENGTH, DARK_GREEN);
            // Draw on bottom edge
            drawHLine(start_x, selected_y + CELL_HEIGHT - 1, DASH_LENGTH, DARK_GREEN);
            }       
        
        for (int y = 0; y < CELL_HEIGHT; y += DASH_LENGTH * 2) {
            int start_y = selected_y + (y + march_offset) % CELL_HEIGHT;
            // Left edge
            drawVLine(selected_x, start_y, DASH_LENGTH, DARK_GREEN);
            // Right edge
            drawVLine(selected_x + CELL_WIDTH - 1, start_y, DASH_LENGTH, DARK_GREEN);
        }

        /*End of code added on 4/27*/

       


        // Draw the collision radius
        // for (int i = 0; i < NUM_BOIDS; i++) {
        //     drawCircle(fix2int15(game_state.boids[i].x), fix2int15(game_state.boids[i].y), BOID_COLLISION_RADIUS, WHITE);
        // }

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

    

        // game_state_update(&game_state);

        spare_time = FRAME_RATE - (time_us_32() - begin_time);
        PT_YIELD_usec(spare_time);
    }
    PT_END(pt);
} // graphics thread

// ==================================================
// === toggle25 thread on core 0
// ==================================================
//the on-board LED blinks
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


void setup_joystick() {

    //printf("BOOTED_ADC \n");

    adc_gpio_init(ADC_GPIO_VX);
    adc_gpio_init(ADC_GPIO_VY);

    adc_init();
    adc_select_input(0);
}

void setup_button(){

    //printf("BOOTED_BUTTON \n");

    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

}

bool button_debouncing(){

    //printf("BOOTED_Debouncing \n");

    static enum {
    NOT_PRESSED,
    MAYBE_PRESSED,
    PRESSED,
    MAYBE_NOT_PRESSED
    } debounce_state = NOT_PRESSED;

    static int possible_press = -1;

    while(1){
        bool buttonpress = gpio_get(BUTTON_PIN);
        
        switch(debounce_state){
        case NOT_PRESSED:
            if(buttonpress == 0){
                possible_press = buttonpress;
                debounce_state = MAYBE_PRESSED;
            } 
            break;
        case MAYBE_PRESSED:
            if(buttonpress == 0 && possible_press == 0) debounce_state = PRESSED;
            else debounce_state = NOT_PRESSED;
            break;
        case PRESSED:
            if (buttonpress == 1 ) debounce_state = MAYBE_NOT_PRESSED;
            break;
        case MAYBE_NOT_PRESSED:
            if (buttonpress == 0) debounce_state = PRESSED;
            else debounce_state = NOT_PRESSED;
            break;
        default:
            break;
        } 
    }
    if (debounce_state == PRESSED) return true; 
    else return false;   
}


int get_VX_ADC(){
    //printf("BOOTED_get_vx \n");
    adc_select_input(2);
    return adc_read();
}

int get_VY_ADC(){
    //printf("BOOTED_get_vy \n");
    adc_select_input(1);
    return adc_read();
}

bool get_button_press(){
    //printf("BOOTED_get_buttonpress \n");
    bool is_button_pressed = button_debouncing();
    if(is_button_pressed == true) return true;
    else return false; 
}

static PT_THREAD(protothread_joystick(struct pt *pt)){
    PT_BEGIN(pt);
     static enum {
    NOT_PRESSED,
    MAYBE_PRESSED,
    PRESSED,
    MAYBE_NOT_PRESSED
    } debounce_state = NOT_PRESSED;

    static uint32_t last_move_time = 0;
    

    static int possible_press = -1;

    while(1){
        uint32_t current_move_time = time_us_32();
        bool buttonpress = gpio_get(BUTTON_PIN);
        
        switch(debounce_state){
        case NOT_PRESSED:
            if(buttonpress == 0){
                possible_press = buttonpress;
                debounce_state = MAYBE_PRESSED;
            } 
            break;
        case MAYBE_PRESSED:
            if(buttonpress == 0 && possible_press == 0) debounce_state = PRESSED;
            else debounce_state = NOT_PRESSED;
            break;
        case PRESSED:
            if (buttonpress == 1 ) debounce_state = MAYBE_NOT_PRESSED;
            break;
        case MAYBE_NOT_PRESSED:
            if (buttonpress == 0) debounce_state = PRESSED;
            else debounce_state = NOT_PRESSED;
            break;
        default:
            break;
        } 
    
        
        int x_value = get_VX_ADC();
        // printf("The X Value is: %d\n", x_value);
        int y_value = get_VY_ADC();
        //printf("The Y Value is : %d\n", y_value);

        if((current_move_time - last_move_time) > 250000){
        
        if (x_value > X_RIGHT_THRESHOLD){
             printf("COMMAND RIGHT\n ");
             if(game_state.selected_col != COLS-1) game_state.selected_col += 1;
        }
       //else printf("The X Value is: %d\n", x_value);
        else if (x_value < X_LEFT_THRESHOLD){
            printf("COMMAND LEFT\n ");
            if (game_state.selected_col != 0) game_state.selected_col -= 1;
        }
        else if (y_value > Y_DOWN_THRESHOLD) {
            printf("COMMAND DOWN\n ");
            if (game_state.selected_row != 0) game_state.selected_row -= 1;
        }
        else if (y_value < Y_UP_THRESHOLD){
            printf("COMMAND UP\n ");
            if (game_state.selected_row != ROWS-1) game_state.selected_row += 1;
        }
        last_move_time = current_move_time;
        if (debounce_state == PRESSED) printf("Button was Pressed \n");
        }

        
    }
    
    PT_YIELD_usec(100000);

    PT_END(pt);
}
// ========================================
// === core 0 main
// ========================================
int main() {
    // set the clock
    stdio_init_all();

    sleep_ms(3000);
    printf("BOOTED\n");
    
    setup_button();
    setup_joystick();

    // Initialize the VGA screen
    initVGA();

    // Initialize SPI channel (channel, baud rate set to 20MHz)
    spi_init(SPI_PORT, 20000000);
    // Format (channel, data bits per transfer, polarity, phase, order)
    spi_set_format(SPI_PORT, 16, 0, 0, 0);

    // Map SPI signals to GPIO ports
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_CS, GPIO_FUNC_SPI) ;


    // for(int i = 0; i<= SAMPLE_COUNT; i++){
    //     DAC_data[i] = audio_samples
    // }

    // // Map LDAC pin to GPIO port, hold it low (could alternatively tie to GND)
    // gpio_init(LDAC) ;
    // gpio_set_dir(LDAC, GPIO_OUT) ;
    // gpio_put(LDAC, 0) ;

    // // Setup the ISR-timing GPIO
    // gpio_init(ISR_GPIO) ;
    // gpio_set_dir(ISR_GPIO, GPIO_OUT);
    // gpio_put(ISR_GPIO, 0) ;

    //  // Enable the interrupt for the alarm (we're using Alarm 0)
    // hw_set_bits(&timer_hw->inte, 1u << ALARM_NUM) ;
    // //Associate an interrupt handler with the ALARM_IRQ
    // irq_set_exclusive_handler(ALARM_IRQ, alarm_irq) ;
    // // Enable the alarm interrupt
    // irq_set_enabled(ALARM_IRQ, true) ;
    // // Write the lower 32 bits of the target time to the alarm register, arming it.
    // timer_hw->alarm[ALARM_NUM] = timer_hw->timerawl + DELAY;


    // Select DMA channels
    int data_chan = 0;
    int ctrl_chan = 1;

    // Setup the control channel
    dma_channel_config c = dma_channel_get_default_config(ctrl_chan);   // default configs
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);             // 32-bit txfers
    channel_config_set_read_increment(&c, false);                       // no read incrementing
    channel_config_set_write_increment(&c, false);                      // no write incrementing
    channel_config_set_chain_to(&c, data_chan);                         // chain to data channel

    dma_channel_configure(
        ctrl_chan,                          // Channel to be configured
        &c,                                 // The configuration we just created
        &dma_hw->ch[data_chan].read_addr,   // Write address (data channel read address)
        &audio_samples,                   // Read address (POINTER TO AN ADDRESS)
        1,                                  // Number of transfers
        false                               // Don't start immediately
    );

    // Setup the data channel
    dma_channel_config c2 = dma_channel_get_default_config(data_chan);  // Default configs
    channel_config_set_transfer_data_size(&c2, DMA_SIZE_16);            // 16-bit txfers
    channel_config_set_read_increment(&c2, true);                       // yes read incrementing
    channel_config_set_write_increment(&c2, false);                     // no write incrementing
    // (X/Y)*sys_clk, where X is the first 16 bytes and Y is the second
    // sys_clk is 125 MHz unless changed in code. Configured to ~44 kHz
    dma_timer_set_fraction(0, 0x0017, 0xffff) ;
    // 0x3b means timer0 (see SDK manual)
    channel_config_set_dreq(&c2, 0x3b);                                 // DREQ paced by timer 0
    // chain to the controller DMA channel
    channel_config_set_chain_to(&c2, ctrl_chan);                        // Chain to control channel


    dma_channel_configure(
        data_chan,                  // Channel to be configured
        &c2,                        // The configuration we just created
        &spi_get_hw(SPI_PORT)->dr,  // write address (SPI data register)
        audio_samples,                   // The initial read address
        SAMPLE_COUNT,            // Number of transfers
        false                       // Don't start immediately.
    );


    // start the control channel
    dma_start_channel_mask(1u << ctrl_chan) ;

    // start core 1 threads
    multicore_reset_core1();
    multicore_launch_core1(&core1_main);

    // === config threads ========================
    // for core 0
    pt_add_thread(protothread_graphics);
    pt_add_thread(protothread_toggle25);
    pt_add_thread(protothread_joystick);
    //
    // === initalize the scheduler ===============
    pt_schedule_start;
    // NEVER exits
    // ===========================================
} // end main
