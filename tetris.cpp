// Tetris for Arduino
// Adam Greig, April 2009

#include <WProgram.h>
#include <avr/interrupt.h>
#include <avr/io.h>

//A listing of registers that need to be set on the LCD
#include "initcode.h"

//Constants that are set at the start of SPI messages to the LCD
#define INDEX 0x00
#define DATA 0x02
#define IDBYTE 0x74

//Store the game world, a 10x20 grid of 5x5 squares of a set colour
char grid[10][20];
//Store the current piece, max 4x4, coloured
char piece[4][4];
//Store whether we've currently got a piece (if false, one is generated)
bool piece_in_play = false;
//Store current piece position, the (0,0) corner of the piece matrix
// compared to the grid coordinates
char piece_position[2];
//Used all the time for various things
volatile unsigned short int x, y;
//Store milliseconds since program start, used to work out when to apply gravity
unsigned long milliseconds;
//Store collision check results
char collision;

//The tetrominoes
char pieceI[4][4] = {{6,6,6,6},{0,0,0,0},{0,0,0,0},{0,0,0,0}};
char pieceJ[4][4] = {{3,0,0,0},{3,3,3,0},{0,0,0,0},{0,0,0,0}};
char pieceL[4][4] = {{0,0,0,7},{0,7,7,7},{0,0,0,0},{0,0,0,0}};
char pieceO[4][4] = {{4,4,0,0},{4,4,0,0},{0,0,0,0},{0,0,0,0}};
char pieceS[4][4] = {{0,2,2,0},{2,2,0,0},{0,0,0,0},{0,0,0,0}};
char pieceT[4][4] = {{0,5,0,0},{5,5,5,0},{0,0,0,0},{0,0,0,0}};
char pieceZ[4][4] = {{1,1,0,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}};

//Send one byte over SPI
void transmit( char data ) {
    SPDR = data;
    while(!(SPSR & (1<<SPIF)));
}

//Write a register on the LCD display
void write_reg( unsigned short index, unsigned short data ) {
    PORTB = PORTB & ~(1<<PB2);
    transmit(IDBYTE|INDEX);
    transmit(0x00);
    transmit(index);
    PORTB = PORTB | (1<<PB2);
    PORTB = PORTB & ~(1<<PB2);
    transmit(IDBYTE|DATA);
    transmit(data>>8);
    transmit(data);
    PORTB = PORTB | (1<<PB2);
}

//Check for collisions with the grid blocks or walls
#define COLLIDE_NONE    0
#define COLLIDE_BLOCK   1
#define COLLIDE_FLOOR   2
#define COLLIDE_LEFT    3
#define COLLIDE_RIGHT   4
#define COLLIDE_TOP     5
char check_collision() {
    unsigned short int a, b;
    for(a=0; a<10; a++) {
        if( grid[a][19] ) {
            return 5;
        }
    }
    for(a=0; a<4; a++) {
        for(b=0; b<4; b++) {
            if( !piece[a][b] ) {
                continue;
            } else if( grid[piece_position[0] + a][piece_position[1] + b] ) {
                return 1;
            } else if( piece_position[1] + b == 0) {
                return 2;
            } else if( piece_position[0] + a == 0) {
                return 3;
            } else if( piece_position[0] + a == 9) {
                return 4;
            }
        }
    }
    return 0;
}

//Copy a piece into the grid
void blit_piece_to_grid() {
    unsigned short int a, b;
    for(a=0; a<4; a++) {
        for(b=0; b<4; b++) {
            if(piece[a][b])
                grid[piece_position[0] + a][piece_position[1] + b] = piece[a][b];
        }
    }
}

//Rotate a 4x4 matrix clockwise
// This is hardcoded for speed
void rotate() {
    char dst[4][4];

    dst[0][0] = piece[3][0];
    dst[0][1] = piece[2][0];
    dst[0][2] = piece[1][0];
    dst[0][3] = piece[0][0];

    dst[1][0] = piece[3][1];
    dst[1][1] = piece[2][1];
    dst[1][2] = piece[1][1];
    dst[1][3] = piece[0][1];

    dst[2][0] = piece[3][2];
    dst[2][1] = piece[2][2];
    dst[2][2] = piece[1][2];
    dst[2][3] = piece[0][2];

    dst[3][0] = piece[3][3];
    dst[3][1] = piece[2][3];
    dst[3][2] = piece[1][3];
    dst[3][3] = piece[0][3];

    memcpy(piece, dst, 16);
}

//Check for completed lines
void check_completed_lines() {
    unsigned short int a, b;
    for(b=0; b<20; b++) {
        bool complete_line = true;
        for(a=0; a<10; a++) {
            if(grid[a][b] == 0)
                complete_line = false;
        }
        //If one is found, clear it and move everbthing above it down
        if(complete_line) {
            unsigned short int a;
            for(a=b+1; a<20; a++) {
                for(a=0; a<10; a++) {
                    grid[a][a - 1] = grid[a][a];
                }
            }
            for(a=0; a<10; a++) {
                grid[a][19] = 0;
            }
        }
    }
}

//ISR to move the piece right TODO actual collision detection rather than the 4x4
void move_right() {
    detachInterrupt(0);
    if(piece_position[0] != 9 && check_collision() != 4)
        piece_position[0]++;
}

//ISR to move the piece left TODO actual collision detection rather than the 4x4
void move_left() {
    detachInterrupt(1);
    if(piece_position[0] != 0 && check_collision() != 3)
        piece_position[0]--;
}

//Transmit the correct bytes for any given colour-number
// Each pixel must be transmitted as two bytes
//  RRRRRGGG GGGBBBBBB
// The colours are encoded with bytes as follows
//  0    BLACK   0x00 0x00
//  1    RED     0xD8 0x00
//  2    GREEN   0x06 0xE0
//  3    BLUE    0x00 0xFB
//  4    YELLOW  0xDE 0xE0
//  5    PURPLE  0xD8 0x1B
//  6    CYAN    0x06 0xFB
//  7    ORANGE  0xD9 0xA0
void send_colour(unsigned short int colour) {
    switch(colour) {
        case 0:
            transmit(0x00);
            transmit(0x00);
            break;
        case 1:
            transmit(0xD8);
            transmit(0x00);
            break;
        case 2:
            transmit(0x06);
            transmit(0xE0);
            break;
        case 3:
            transmit(0x00);
            transmit(0xFB);
            break;
        case 4:
            transmit(0xDE);
            transmit(0xE0);
            break;
        case 5:
            transmit(0xD8);
            transmit(0x1B);
            break;
        case 6:
            transmit(0x06);
            transmit(0xFB);
            break;
        case 7:
            transmit(0xD9);
            transmit(0xA0);
            break;
        default:
            transmit(0x00);
            transmit(0x00);
            break;
    }
}

//Initialisation
void setup() {
    //Set the SCK, MOSI and CS pins to output
    pinMode(10, OUTPUT);
    pinMode(11, OUTPUT);
    pinMode(13, OUTPUT);

    //Set up the SPI registers
    SPCR = (1<<SPE)|(1<<MSTR)|(0<<SPR0)|(0<<SPR1);
    SPSR = (1<<SPI2X);

    //Set the four button pins to be input pulled high
    pinMode(2, INPUT);
    pinMode(3, INPUT);
    pinMode(4, INPUT);
    pinMode(5, INPUT);
    digitalWrite(2, HIGH);
    digitalWrite(3, HIGH);
    digitalWrite(4, HIGH);
    digitalWrite(5, HIGH);

    //Seed the random number generator
    int random_seed;
    random_seed = analogRead(5);
    randomSeed(random_seed);
    random_seed = analogRead(5);
    srand(random_seed);

    //Store current millis
    milliseconds = millis();

    //Initialise the LCD
    for(x = 0;; x += 2) {
        //If the current instruction is 0xFFFF we either break or delay
        if( initcode[x] == 0xFFFF ) {
            if( initcode[x+1] == 0xFFFF ) break;
            else delay(initcode[x+1]);
        } else {
            write_reg(initcode[x], initcode[x+1]);
        }
    }

    //Clear the grid and piece
    memset(grid, 0x00, 200);
    memset(piece, 0x00, 16);

    //Tell the LCD we're about to start sending data
    PORTB = PORTB & ~(1<<PB2);
    transmit( IDBYTE|INDEX );
    transmit( 0x00 );
    transmit( 0x22 );
    PORTB = PORTB | (1<<PB2);
    PORTB = PORTB & ~(1<<PB2);
    transmit( IDBYTE|DATA );

}

void loop() {

    //Re-enable all interrupts
    attachInterrupt(0, move_right, LOW);
    attachInterrupt(1, move_left, LOW);

    //Rotate the piece
    if(digitalRead(4) == LOW) {
        rotate();
    }

    //Drop the piece
    if(digitalRead(5) == LOW) {
        if(piece_position[1] > 2)
            piece_position[1] -= 2;
    }

    //Gravity
    if( millis() - milliseconds > 300 ) {
        piece_position[1]--;
        collision = check_collision();
        if(collision == 1) {
            //Move it back up
            piece_position[1]++;
            //Copy piece to grid
            blit_piece_to_grid();
            //Check for completed lines
            check_completed_lines();
            //We now need a new piece
            piece_in_play = 0;
        } else if(collision == 2) {
            //Copy piece to grid
            blit_piece_to_grid();
            //Check for completed lines
            check_completed_lines();
            //We now need a new piece
            piece_in_play = 0;
        } else if(collision == 5) {
            //oh shit we just lost the game
            memset(grid, 1, 200);
        }
        milliseconds = millis();
    }

    //Make a new piece if needed
    if(!piece_in_play) {
        //Copy a random piece into the piece matrix
        char random_piece = rand() % 7;
        switch(random_piece) {
            case 0:
                memcpy(piece, pieceI, 16);
                break;
            case 1:
                memcpy(piece, pieceJ, 16);
                break;
            case 2:
                memcpy(piece, pieceL, 16);
                break;
            case 3:
                memcpy(piece, pieceO, 16);
                break;
            case 4:
                memcpy(piece, pieceS, 16);
                break;
            case 5:
                memcpy(piece, pieceT, 16);
                break;
            case 6:
                memcpy(piece, pieceZ, 16);
                break;
        }

        //Set its start position
        piece_position[0] = 5;
        piece_position[1] = 19;

        piece_in_play = true;
    }

    //Iterate over every pixel
    for(x = 0; x < 96; x++) {
        for(y = 0; y < 128; y++) {
            //Draw the game area
            if( x < 61 && y < 121) {
                //Draw the grid lines
                if( x % 6 == 0 || y % 6 == 0 ) {
                    transmit(0x39);
                    transmit(0xE7);
                //Draw the blocks
                } else {
                    unsigned short int a = x/6;
                    unsigned short int b = y/6;

                    if( (a - piece_position[0] < 4) && (b - piece_position[1] < 4) ) {
                        char piece_colour = piece[a - piece_position[0]][b - piece_position[1]];
                        if( piece_colour )
                            send_colour(piece_colour);
                        else
                            send_colour(grid[a][b]);
                    } else {
                        send_colour(grid[a][b]);
                    }

                }
            //Draw black everywhere else
            } else {
                transmit(0x00);
                transmit(0x00);
            }
        }
    }

    if(collision == 5) {
        for(;;);
    }
}

//This function binds it all together and makes the rest
// just like an Arduino. Call Arduino's init() then my
// setup then keep calling loop().
int main() {
    init();
    setup();
    for(;;)
        loop();
    return 0;
}
