/*
 * Grid cell:
 * ##############
 * #
 * #
 * #
 * # #   # #####
 * # #   # #
 * # #   # #
 * # ##### #####
 * # #   # #
 * # #   # #
 * # #   # #####
 * #
 * #
 * #
 *
 *
 * PORTB = {,           ,           VSYNC,      ,           ,           ,           ,           }
 * PORTG = {,           ,           HSYNC,      ,           ,           ,           ,           }
 * PORTK = {R0,         R1,         R2,         R3,         C0,         C1,         C2,         }
 * PORTL = {D7,         D6,         D5,         D4,         phone_pwr,  ,           RS,         E}
 * PORTA = {,           ,           ,           ,           ,           BLUE,       GREEN,      RED}
 * 
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <string.h>

// Maximum 2 digits after the decimal point. Otherwise frequencies don't line up
#define PIXEL_SIZE 0.70


volatile uint8_t row_num = 0;
volatile uint16_t idx = 0;
volatile uint8_t flags = 0b00000001;
volatile uint8_t random = 0;

#define BUFLEN 7500
#define WIDTH 150
volatile uint8_t pixels[BUFLEN];

volatile uint32_t score = 0;

#define _STR(x) #x
#define STR(x) _STR(x)
#define POS(x, y) (WIDTH*y+x)

#define BLACK   0x00
#define RED     0x11
#define GREEN   0x22
#define YELLOW  0x33
#define BLUE    0x44
#define MAGENTA 0x55
#define CYAN    0x66
#define WHITE   0x77

#define CELLSIZE 7

const uint8_t letters[96][8] PROGMEM = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /*   */
    {0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04}, /* ! */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* " */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* # */
    {0x0e, 0x15, 0x05, 0x0e, 0x14, 0x15, 0x0e}, /* $ */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* % */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* & */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* ' */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* ( */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* ) */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* * */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* + */
    {0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x02}, /* , */
    {0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00}, /* - */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04}, /* . */
    {0x00, 0x10, 0x08, 0x04, 0x02, 0x01, 0x00}, /* / */
    {0x0e, 0x11, 0x19, 0x15, 0x13, 0x11, 0x0e}, /* 0 */
    {0x04, 0x06, 0x05, 0x04, 0x04, 0x04, 0x1f}, /* 1 */
    {0x0e, 0x11, 0x10, 0x08, 0x04, 0x02, 0x1f}, /* 2 */
    {0x0e, 0x11, 0x10, 0x0c, 0x10, 0x11, 0x0e}, /* 3 */
    {0x08, 0x0c, 0x0a, 0x1f, 0x08, 0x08, 0x08}, /* 4 */
    {0x1f, 0x01, 0x0f, 0x10, 0x10, 0x11, 0x0e}, /* 5 */
    {0x0e, 0x11, 0x01, 0x0f, 0x11, 0x11, 0x0e}, /* 6 */
    {0x1f, 0x10, 0x08, 0x04, 0x02, 0x02, 0x02}, /* 7 */
    {0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e}, /* 8 */
    {0x0e, 0x11, 0x11, 0x0e, 0x10, 0x11, 0x0e}, /* 9 */
    {0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00}, /* : */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* ; */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* < */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* = */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* > */
    {0x0e, 0x11, 0x10, 0x08, 0x04, 0x00, 0x04}, /* ? */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* @ */
    {0x0e, 0x11, 0x11, 0x11, 0x1f, 0x11, 0x11}, /* a */
    {0x0f, 0x11, 0x11, 0x0f, 0x11, 0x11, 0x0f}, /* b */
    {0x0e, 0x11, 0x01, 0x01, 0x01, 0x11, 0x0e}, /* c */
    {0x0f, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0f}, /* d */
    {0x1f, 0x01, 0x01, 0x0f, 0x01, 0x01, 0x1f}, /* e */
    {0x1f, 0x01, 0x01, 0x0f, 0x01, 0x01, 0x01}, /* f */
    {0x0e, 0x11, 0x01, 0x01, 0x19, 0x11, 0x0e}, /* g */
    {0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11}, /* h */
    {0x0e, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0e}, /* i */
    {0x1c, 0x08, 0x08, 0x08, 0x08, 0x09, 0x06}, /* j */
    {0x11, 0x09, 0x05, 0x03, 0x05, 0x09, 0x11}, /* k */
    {0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x1f}, /* l */
    {0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11}, /* m */
    {0x11, 0x11, 0x13, 0x15, 0x19, 0x11, 0x11}, /* n */
    {0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e}, /* o */
    {0x0f, 0x11, 0x11, 0x0f, 0x01, 0x01, 0x01}, /* p */
    {0x0e, 0x11, 0x11, 0x11, 0x11, 0x0e, 0x08}, /* q */
    {0x0f, 0x11, 0x11, 0x0f, 0x05, 0x09, 0x11}, /* r */
    {0x0e, 0x11, 0x01, 0x0e, 0x10, 0x11, 0x0e}, /* s */
    {0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}, /* t */
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e}, /* u */
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x0a, 0x04}, /* v */
    {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0a}, /* w */
    {0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11}, /* x */
    {0x11, 0x0a, 0x04, 0x04, 0x04, 0x04, 0x04}, /* y */
    {0x1f, 0x10, 0x08, 0x04, 0x02, 0x01, 0x1f}, /* z */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* [ */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* \ */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* ] */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* ^ */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f}, /* _ */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* ` */
    {0x0e, 0x11, 0x11, 0x11, 0x1f, 0x11, 0x11}, /* a */
    {0x0f, 0x11, 0x11, 0x0f, 0x11, 0x11, 0x0f}, /* b */
    {0x0e, 0x11, 0x01, 0x01, 0x01, 0x11, 0x0e}, /* c */
    {0x0f, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0f}, /* d */
    {0x1f, 0x01, 0x01, 0x0f, 0x01, 0x01, 0x1f}, /* e */
    {0x1f, 0x01, 0x01, 0x0f, 0x01, 0x01, 0x01}, /* f */
    {0x0e, 0x11, 0x01, 0x01, 0x19, 0x11, 0x0e}, /* g */
    {0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11}, /* h */
    {0x0e, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0e}, /* i */
    {0x1c, 0x08, 0x08, 0x08, 0x08, 0x09, 0x06}, /* j */
    {0x11, 0x09, 0x05, 0x03, 0x05, 0x09, 0x11}, /* k */
    {0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x1f}, /* l */
    {0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11}, /* m */
    {0x11, 0x11, 0x13, 0x15, 0x19, 0x11, 0x11}, /* n */
    {0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e}, /* o */
    {0x0f, 0x11, 0x11, 0x0f, 0x01, 0x01, 0x01}, /* p */
    {0x0e, 0x11, 0x11, 0x11, 0x11, 0x0e, 0x08}, /* q */
    {0x0f, 0x11, 0x11, 0x0f, 0x05, 0x09, 0x11}, /* r */
    {0x0e, 0x11, 0x01, 0x0e, 0x10, 0x11, 0x0e}, /* s */
    {0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}, /* t */
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e}, /* u */
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x0a, 0x04}, /* v */
    {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0a}, /* w */
    {0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11}, /* x */
    {0x11, 0x0a, 0x04, 0x04, 0x04, 0x04, 0x04}, /* y */
    {0x1f, 0x10, 0x08, 0x04, 0x02, 0x01, 0x1f}, /* z */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* { */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* | */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* } */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* ~ */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}  /*  */
};

void add_char(uint16_t offset, uint8_t color, char chr) {
    if (chr == ' ') return;
    uint8_t row = 0;
    uint8_t data;
    uint8_t old = (uint8_t)offset;
    uint8_t temp;
    uint8_t ch = color & 0b11110000;
    uint8_t cl = color & 0b00001111;
    if (old & 1) offset -= 1;
    offset = offset >> 1;
    for (; row<7; row++) {
        data = pgm_read_byte(&(letters[chr - 32][row]));
        if (old & 1) data = data << 1;
        
        temp = pixels[offset];
        if (data & 0b00000001) temp = (temp & 0b11110000) | cl;
        if (data & 0b00000010) temp = (temp & 0b00001111) | ch;
        pixels[offset++] = temp;
        
        temp = pixels[offset];
        if (data & 0b00000100) temp = (temp & 0b11110000) | cl;
        if (data & 0b00001000) temp = (temp & 0b00001111) | ch;
        pixels[offset++] = temp;
        
        temp = pixels[offset];
        if (data & 0b00010000) temp = (temp & 0b11110000) | cl;
        if (data & 0b00100000) temp = (temp & 0b00001111) | ch;
        pixels[offset] = temp;
        offset += 73;
    }
}

void add_text(uint16_t offset, uint8_t color, char *string) {
    uint8_t len = strlen(string);
    for (uint8_t idx = 0; idx < len; idx++) {
        add_char(offset, color, string[idx]);
        offset += 6;
    }
}


ISR(TIMER0_OVF_vect, ISR_NAKED) {
    asm volatile (
        "push r0\n\t"
        "in r0, 0x3f\n\t"
        "push r0\n\t"
        "push r24\n\t"
        "push r25\n\t"
        "push r30\n\t"
        "push r31\n\t"
        "lds r24, flags\n\t"
        "sbrs r24, 0\n\t"
        "rjmp done\n\t"
        
        // ########## The following lines must be exactly 28 clock cycles long
        "lds r30, idx\n\t"              // Load the offset into Z
        "lds r31, idx+1\n\t"
        
        
        "lds r24, row_num\n\t"          // Load the row index into r24
        "inc r24\n\t"                   // Increment the row index
        
        "sbrc r24, 2\n\t"               // If the row index is 4...
            "adiw r30, 40\n\t"          // ... add 40 ...
        "sbrc r24, 2\n\t"
            "adiw r30, 35\n\t"          // ... and then add 35 to the Z pointer for a total of 75
        "sbrs r24, 2\n\t"               // If the row index is not 4, pause for 4 instruction cycles. This balances the runtime so that the 4th row does not appear shifted
            "rjmp .+0\n\t"
        "sbrs r24, 2\n\t"
            "rjmp .+0\n\t"
        "andi r24, 3\n\t"               // Truncate the top 6 bits of the row counter so it is never greater than 3
        "sts row_num, r24\n\t"          // Store the final row counter back into row_num
        "sts idx, r30\n\t"
        "sts idx+1, r31\n\t"
        "ldi r24, lo8(pixels)\n\t"      // Load start pointer to pixels into r24 and r25
        "ldi r25, hi8(pixels)\n\t"
        "add r30, r24\n\t"              // Add r24 and r25 to Z
        "adc r31, r25\n\t"
        // ##########
        
        "in r24, 0x26\n\t"
        "in r25, 0x26\n\t"
        //"out 0x05, r24\n\t"
        //"out 0x05, r25\n\t"
        "sbrc r24, 0\n\t"
        "rjmp no_del\n\t"
        "nop\n\tnop\n\t"
        "no_del:\n\t"
        
        "nop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\t"
        "nop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\t"
        "nop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\t"
        "nop\n\tnop\n\tnop\n\t"
        
        
        ".rept 75\n\t"
            "ld r24, Z+\n\t"
            "out 2, r24\n\t"
            "nop\n\t"
            "swap r24\n\t"
            "out 2, r24\n\t"
        ".endr\n\t"
        
        "nop\n\t"
        "ldi r24, 0\n\t"
        "out 2, r24\n\t"
        
        "done:\n\t"
        "pop r30\n\t"
        "pop r31\n\t"
        "pop r25\n\t"
        "pop r24\n\t"
        "pop r0\n\t"
        "out 0x3f, r0\n\t"
        "pop r0\n\t"
        "reti\n\t"
        : 
        : "e" (flags)
        : "r24", "r25"
    );
}

ISR(TIMER1_COMPB_vect) {
    flags |= 0b00000001;
}

const char elements[32] PROGMEM =   "  "        "H "        "He"        "Li"        "Be"        "B "        "C "        "N "        "O "        "F "        "Ne"        "Na"        "Mg"        "Al"        "Si"        "P ";
const uint8_t colors[16] PROGMEM = {WHITE,      WHITE,      WHITE,      RED,        RED,        RED,        RED,        YELLOW,     YELLOW,     YELLOW,     YELLOW,     YELLOW,     MAGENTA,    MAGENTA,    MAGENTA,    MAGENTA};
uint8_t grid[25] = {
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    1, 0, 0, 0, 1
};
uint8_t new_grid[25];
uint8_t delta[25];
uint8_t last_buttons;
uint8_t buttons_pressed;
uint8_t animate_step = 0;

void add_tile(uint8_t x, uint8_t y, int16_t offset, uint8_t num, uint8_t color) {
    uint16_t pos = WIDTH + 1 + CELLSIZE*(x + y*WIDTH) + offset;
    uint16_t idx = pos;
    for (; idx <= pos + WIDTH*CELLSIZE; idx += WIDTH/2) {
        pixels[idx] = 0x07;
        if ((x == 4) || (idx == pos + WIDTH*CELLSIZE && y == 4)) continue;
        pixels[idx + CELLSIZE] = 0x07;
    }
    for (uint8_t i = 0; i < CELLSIZE; i++) pixels[pos + i] = 0x77;
    idx = pos + CELLSIZE*WIDTH;
    for (uint8_t i = 0; i < CELLSIZE; i++) pixels[idx + i] = 0x77;
    
    char c1 = (char)pgm_read_byte(elements + 2*num);
    char c2 = (char)pgm_read_byte((1+elements) + 2*num);
    color = pgm_read_byte(&(colors[num]));
    if (c2 == ' ') {
        add_char(((pos + 2*WIDTH + 1)<<1) + 3, color, c1);
    } else {
        add_char((pos + 2*WIDTH + 1)<<1, color, c1);
        add_char((pos + 2*WIDTH + 4)<<1, color, c2);
    }
}

void draw_tiles() {
    for (uint8_t x=0; x<5; x++) {
        for (uint8_t y=0; y<5; y++) {
            if (grid[x + 5*y] > 0) {
                uint8_t offset = (delta[x + 5*y] > animate_step ? animate_step : delta[x + 5*y]);
                if (buttons_pressed & 0b00000001)      add_tile(x, y, -offset, grid[x + 5*y], WHITE);
                else if (buttons_pressed & 0b00000010) add_tile(x, y, offset, grid[x + 5*y], WHITE);
                else if (buttons_pressed & 0b00000100) add_tile(x, y, offset*WIDTH, grid[x + 5*y], WHITE);
                else add_tile(x, y, 0, grid[x + 5*y], WHITE);
            }
        }
    }
}

void draw_score() {
    uint16_t pos = 6*WIDTH + CELLSIZE*5 + 3;
    for (; pos < 6*WIDTH + CELLSIZE*5 + 3 + WIDTH*3.5; pos += WIDTH/2) {
        memset(pixels + pos, 0, 24);
    }
    uint32_t temp = score;
    pos = (6*WIDTH + CELLSIZE*5 + 24)*2;
    do {
        add_char(pos, BLUE, '0'+(temp%10));
        temp = temp / 10;
        pos -= 6;
    } while (temp > 0);
}

ISR(TIMER1_COMPC_vect) {
    flags &= 0b11111110;
    //PORTB |= 0b10000000;
    //PORTF = idx&255;
    
    // Advance the PRNG
    ADCSRA = 0b11000000;
    while (ADCSRA & 0b01000000);
    uint16_t val = (ADCH << 8) | ADCL;
    for (uint8_t x = 0; x<10; x++) {
        if ((random ^ val) & 0x01) random = (random >> 1) ^ 0x8C;
        else random = random >> 1;
        val = val >> 1;
    }
    
    
    // Prepare the next frame
    for (idx = WIDTH + 1; idx < WIDTH*(CELLSIZE*5 + 1); idx += WIDTH/2) {
        memset(pixels + idx, 0, CELLSIZE*5);
    }
    
    if (buttons_pressed) {
        PORTB |= 0b10000000;
        animate_step+=4;
        draw_tiles();
        if (animate_step > 28) {
            memcpy(grid, new_grid, 25);
            memset(delta, 0, 25);
            animate_step = 0;
            buttons_pressed = 0;
            uint8_t r, c = 0;
            for (r = 0; r < (random & 0b00011111); r++) {
                do c = (c==24?0:c+1);
                while (grid[c]);
            }
            grid[c] = 1;
        }
        last_buttons = PINC;
        PORTB &= 0b01111111;
    } else {
    
        uint8_t diff = last_buttons & (~PINC) & 0b00001111;
        buttons_pressed = diff;
        uint8_t r, c, firstz, prev;
        memcpy(new_grid, grid, 25);
        if (diff & 0b00001111) memset(delta, 0, 25);
        if (diff & 0b00000001) {
            // left button pressed
            for (r = 0; r < 25; r+=5) {
                firstz = 0;
                prev = 0;
                for (c = 0; c < 5; c++) {
                    if (new_grid[r+c]) {
                        if (prev == new_grid[r+c]) {
                            new_grid[r+firstz-1]++;
                            score += (1<<new_grid[r+firstz-1]);
                            new_grid[r+c] = 0;
                            prev = 0;
                            delta[r+c] = (c - firstz + 1)*CELLSIZE;
                        } else {
                            new_grid[r+firstz] = new_grid[r+c];
                            prev = new_grid[r+c];
                            if (c != firstz) new_grid[r+c] = 0;
                            delta[r+c] = (c - firstz)*CELLSIZE;
                            firstz++;
                        }
                    }
                }
            }
        } else if (diff & 0b00000010) {
            // right button pressed
            for (r = 0; r < 25; r+=5) {
                firstz = 4;
                c = 5;
                prev = 0;
                while (c > 0) {
                    c--;
                    if (new_grid[r+c]) {
                        if (prev == new_grid[r+c]) {
                            new_grid[r+firstz+1]++;
                            score += (1<<new_grid[r+firstz+1]);
                            new_grid[r+c] = 0;
                            prev = 0;
                            delta[r+c] = (firstz + 1 - c)*CELLSIZE;
                        } else {
                            new_grid[r+firstz] = new_grid[r+c];
                            prev = new_grid[r+c];
                            if (c != firstz) new_grid[r+c] = 0;
                            delta[r+c] = (firstz - c)*CELLSIZE;
                            firstz--;
                        }
                    }
                }
            }
        } else if (diff & 0b00000100) {
            // down button pressed
            for (c = 0; c < 5; c++) {
                firstz = 20;
                r = 25;
                prev = 0;
                while (r > 0) {
                    r -= 5;
                    if (new_grid[r+c]) {
                        if (prev == new_grid[r+c]) {
                            new_grid[firstz+c+5]++;
                            score += (1<<new_grid[firstz+c+5]);
                            new_grid[r+c] = 0;
                            prev = 0;
                            delta[r+c] = (firstz + 5 - r)*CELLSIZE/5;
                        } else {
                            new_grid[firstz+c] = new_grid[r+c];
                            prev = new_grid[r+c];
                            if (r != firstz) new_grid[r+c] = 0;
                            delta[r+c] = (firstz - r)*CELLSIZE/5;
                            firstz -= 5;
                        }
                    }
                }
            }
        } else if (diff & 0b00001000) {
            for (c = 0; c < 5; c++) {
                firstz = 0;
                prev = 0;
                for (r = 0; r < 25; r += 5) {
                    if (new_grid[r+c]) {
                        if (prev == new_grid[r+c]) {
                            new_grid[firstz+c-5]++;
                            score += (1<<new_grid[firstz+c-5]);
                            new_grid[r+c] = 0;
                            prev = 0;
                        } else {
                            new_grid[firstz+c] = new_grid[r+c];
                            prev = new_grid[r+c];
                            if (r != firstz) new_grid[r+c] = 0;
                            firstz += 5;
                        }
                    }
                }
            }
        }
        draw_tiles();
        draw_score();
        last_buttons = PINC;
    }
    
    idx = 0;
    row_num = 0;
    PORTB &= 0b01111111;
}



int main() {
    uint16_t pos = 0;
    for (pos = 0; pos < BUFLEN; pos+=WIDTH/2) pixels[pos] = 0x77;            // left side
    for (pos = 74; pos < BUFLEN; pos+=WIDTH/2) pixels[pos] = 0x77;           // right side
    for (pos = 0; pos < WIDTH; pos++) pixels[pos] = 0x77;               // top side
    for (pos = BUFLEN-WIDTH; pos < BUFLEN; pos++) pixels[pos] = 0x77;   // bottom side
    for (pos = 5*CELLSIZE+1; pos < (5*CELLSIZE+1)*WIDTH; pos += WIDTH/2) pixels[pos] = 0x77;                                            // grid right
    for (pos = WIDTH*(CELLSIZE*5 + 1); pos < WIDTH*(CELLSIZE*5 + 1) + 2 + CELLSIZE*5; pos++) pixels[pos] = 0x77;                        // grid bottom (top half)
    for (pos = WIDTH*(CELLSIZE*5 + 1) + WIDTH/2; pos <  WIDTH*(CELLSIZE*5 + 1) + 2 + CELLSIZE*5 + WIDTH/2; pos++) pixels[pos] = 0x77;   // grid bottom (bottom half)
    draw_tiles();
    add_text(2*(2*WIDTH + CELLSIZE*5 + 3), RED, "SCORE");
    draw_score();
    
    
    DDRG  |= 0b00100000;
    PORTG |= 0b00100000;
    DDRB  |= 0b10100111;
    DDRA  |= 0b10000111;
    DDRF   = 0b11111111;
    PORTC  = 0b00001111;
    
    TCCR0A = 0b00110011;
    TCCR0B = 0b00001010;
    OCR0A  = 100*PIXEL_SIZE - 1;
    OCR0B  = 12*PIXEL_SIZE - 1;
    TIMSK0 = 0b00000001;
    
    TCCR1A = 0b11000010;
    TCCR1B = 0b00011010;
    TCCR1C = 0b00000000;
    ICR1   = 52500*PIXEL_SIZE - 1; //52500 --> 26250
    OCR1A  = 200*PIXEL_SIZE - 1;
    OCR1B  = 3450*PIXEL_SIZE - 1;
    OCR1C  = 43400*PIXEL_SIZE - 1;
    TIMSK1 = 0b00001100;
    
    ADMUX  = 0b01000000;
    ADCSRA = 0b10000000;
    ADCSRB = 0b00000000;
    
    last_buttons = PINC;
    
    GTCCR  = 0b00000001;
    TCNT0  = 0;
    TCNT1  = 0;
    
    sei();
    
    while(1);
}