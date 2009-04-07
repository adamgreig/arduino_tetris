// Tetris for Arduino
// Adam Greig, April 2009

//============================================================================
// Includes
//----------------------------------------------
#include <WProgram.h>
#include <stdlib.h>
#include <avr/interrupt.h>
#include <avr/io.h>

//A listing of registers that need to be set on the LCD
#include "initcode.h"

//============================================================================
// Constants
//----------------------------------------------

//Constants that are set at the start of SPI messages to the LCD
#define INDEX   0x00
#define DATA    0x02
#define IDBYTE  0x74

//Colours that grid squares might be
#define BLACK   0
#define RED     1
#define GREEN   2
#define BLUE    3
#define YELLOW  4
#define PURPLE  5
#define CYAN    6
#define ORANGE  7

//Collision results
#define COLLIDE_NONE    0
#define COLLIDE_BLOCK   1
#define COLLIDE_FLOOR   2
#define COLLIDE_LEFT    3
#define COLLIDE_RIGHT   4
#define COLLIDE_TOP     5

//============================================================================
// Structs
//----------------------------------------------

//Store point and piece positions
typedef struct Position_ {
    char x;
    char y;
} Position;

//Store a piece
typedef struct Piece_ {
    Position pos;
    Position points[4];
    char colour;
    char in_play;
} Piece;

//============================================================================
// SPI Function Prototypes
//----------------------------------------------

//Transmit one byte of data
void transmit(char data);

//Write a register on the LCD
void write_reg(unsigned short int index, unsigned short int data);

//============================================================================
// Grid Control Prototypes
//----------------------------------------------

//Send the two bytes that represent a numerical colour
void send_colour(char colour);

//Check for completed lines
void check_completed_lines(void);

//Apply gravity
void apply_gravity(void);

//Make a new piece
void new_piece(void);

//============================================================================
// Piece Control Prototypes
//----------------------------------------------

//Check if the current piece has any collisions
char check_collisions(void);

//Blit piece to grid
void blit(void);

//Rotate a piece
void rotate(void);

//Drop a piece
void drop(void);

//Move a piece left
void move_left(void);

//Move a piece right
void move_right(void);

//============================================================================
// Global Variables
//----------------------------------------------

//Store the current piece
Piece piece;

//Store the grid itself
char grid[10][20];

//Store number of milliseconds since execution started
unsigned long milliseconds;

//============================================================================
// The seven Tetrominoes
//----------------------------------------------

Position PieceI[4] = {{0,0}, {1,0}, {2,0}, {3,0}};
Position PieceJ[4] = {{0,0}, {0,1}, {1,1}, {2,1}};
Position PieceL[4] = {{0,1}, {1,1}, {2,1}, {2,0}};
Position PieceO[4] = {{0,0}, {0,1}, {1,0}, {1,1}};
Position PieceS[4] = {{0,1}, {1,1}, {1,0}, {2,0}};
Position PieceT[4] = {{0,1}, {1,1}, {2,1}, {1,0}};
Position PieceZ[4] = {{0,0}, {1,0}, {1,1}, {2,1}};

//============================================================================
//============================================================================
// Initialisation
//----------------------------------------------

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
    srand(2 * analogRead(5));
    rand(); rand();

    //Store current millis
    milliseconds = millis();

    //Initialise the LCD
    unsigned short int x;
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
    memset(grid, BLACK, 200);
    memset(piece.points, BLACK, 4);

    //Tell the LCD we're about to start sending data
    PORTB = PORTB & ~(1<<PB2);
    transmit( IDBYTE|INDEX );
    transmit( 0x00 );
    transmit( 0x22 );
    PORTB = PORTB | (1<<PB2);
    PORTB = PORTB & ~(1<<PB2);
    transmit( IDBYTE|DATA );

}

//============================================================================
//============================================================================
// Main Loop
//----------------------------------------------

void loop() {
    //Re-enable all interrupts
    attachInterrupt(0, move_right, LOW);
    attachInterrupt(1, move_left, LOW);
    PCICR = (1<<PCIE2);
    PCMSK2 = (1<<PCINT20) | (1<<PCINT21);

    apply_gravity();

    if( !piece.in_play )
        new_piece();

    //Iterate over every pixel
    unsigned short int x, y;
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
                    char a = x/6;
                    char b = y/6;

                    char i, sent = 0;
                    for(i=0; i<4; i++) {
                        if( piece.pos.x + piece.points[i].x == a &&
                            piece.pos.y + piece.points[i].y == b ) {
                            send_colour(piece.colour);
                            sent = 1;
                            break;
                        }
                    }

                    if( !sent )
                        send_colour(grid[a][b]);
                }
                //Draw black everywhere else
            } else {
                send_colour(BLACK);
            }
        }
    }

}

//============================================================================
//============================================================================
// SPI Functions
//----------------------------------------------

//Send one byte over SPI
void transmit( char data ) {
    SPDR = data;
    while(!(SPSR & (1<<SPIF)));
}

//Write a register on the LCD display
void write_reg( unsigned short int index, unsigned short int data ) {
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

//============================================================================
// Grid Control Functions
//----------------------------------------------

//Check for completed lines
void check_completed_lines() {
    unsigned short int a, b;

    for(b=0; b<20; b++) {
        char complete_line = 1;

        for(a=0; a<10; a++) {
            if(grid[a][b] == 0)
                complete_line = 0;
        }

        //If one is found, clear it and move everything above it down
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

            //Having moved everything down we might have another line at
            // this position, so decrement b
            b--;
        }
    }
}

//Transmit the correct bytes for any given colour-number
// Each pixel must be transmitted as two bytes
//  RRRRRGGG GGGBBBBBB
void send_colour(char colour) {
    switch(colour) {
        case BLACK:
            transmit(0x00);
            transmit(0x00);
            break;
        case RED:
            transmit(0xD8);
            transmit(0x00);
            break;
        case GREEN:
            transmit(0x06);
            transmit(0xE0);
            break;
        case BLUE:
            transmit(0x00);
            transmit(0xFB);
            break;
        case YELLOW:
            transmit(0xDE);
            transmit(0xE0);
            break;
        case PURPLE:
            transmit(0xD8);
            transmit(0x1B);
            break;
        case CYAN:
            transmit(0x06);
            transmit(0xFB);
            break;
        case ORANGE:
            transmit(0xD9);
            transmit(0xA0);
            break;
        default:
            transmit(0x00);
            transmit(0x00);
            break;
    }
}

//Apply gravity
void apply_gravity() {
    if( millis() - milliseconds > 300 ) {
        piece.pos.y--;

        switch(check_collisions()) {
            case COLLIDE_BLOCK:
                //Dropping made us hit a block

                //Go back to where we're not colliding
                piece.pos.y++;

                //Lock the piece onto the grid
                blit();

                //Check for any now complete lines
                check_completed_lines();

                //Mark the piece as not in play, so a new one is made
                piece.in_play = 0;

                break;

            case COLLIDE_FLOOR:
                //Dropping made us hit the floor

                //Lock the piece onto the grid
                blit();

                //Check for any now complete lines
                check_completed_lines();

                //Mark the piece as not in play, so a new one is made
                piece.in_play = 0;

                break;
            case COLLIDE_TOP:
                //We hit the top, game over
                memset(grid, RED, 200);
        }

        milliseconds = millis();
    }
}

//Make a new piece
void new_piece() {
    //Copy a random piece into the piece matrix
    char random_piece = (rand() % 14) / 2;
    switch(random_piece) {
        case 0:
            memcpy(piece.points, PieceI, 4);
            piece.colour = CYAN;
            break;
        case 1:
            memcpy(piece.points, PieceJ, 4);
            piece.colour = BLUE;
            break;
        case 2:
            memcpy(piece.points, PieceL, 4);
            piece.colour = ORANGE;
            break;
        case 3:
            memcpy(piece.points, PieceO, 4);
            piece.colour = YELLOW;
            break;
        case 4:
            memcpy(piece.points, PieceS, 4);
            piece.colour = GREEN;
            break;
        case 5:
            memcpy(piece.points, PieceT, 4);
            piece.colour = PURPLE;
            break;
        case 6:
            memcpy(piece.points, PieceZ, 4);
            piece.colour = RED;
            break;
    }

    //Set its start position
    piece.pos.x = 5;
    piece.pos.y = 19;

    piece.in_play = 1;
}

//============================================================================
// Piece Control Functions
//----------------------------------------------

//Check if the current piece has any collisions
char check_collisions() {
    char i, x, y;
    for(i=0; i<10; i++) {
        if(grid[i][19])
            return COLLIDE_TOP;
    }
    for(i=0; i<4; i++) {
        x = piece.pos.x + piece.points[i].x;
        y = piece.pos.y + piece.points[i].y;
        if( grid[x][y] )
            return COLLIDE_BLOCK;
        else if( x == 0 )
            return COLLIDE_LEFT;
        else if( x == 9 )
            return COLLIDE_RIGHT;
        else if( y == 0 )
            return COLLIDE_FLOOR;
    }
    return COLLIDE_NONE;
}

//Blit piece to grid
void blit() {
    char i, x, y;
    for(i=0; i<4; i++) {
        x = piece.pos.x + piece.points[i].x;
        y = piece.pos.y + piece.points[i].y;
        grid[x][y] = piece.colour;
    }
}

//Rotate a piece
void rotate() {
}

void drop() {
    if( piece.pos.y > 2 ) {
        piece.pos.y -= 2;
    }
    if( check_collisions() == COLLIDE_BLOCK ) {
        piece.pos.y += 2;
    }
}

//Move a piece left
void move_left() {
    detachInterrupt(1);
    if( check_collisions() != COLLIDE_LEFT ) {
        piece.pos.x--;
    }
    if( check_collisions() == COLLIDE_BLOCK ) {
        piece.pos.x++;
    }
}

//Move a piece right
void move_right() {
    detachInterrupt(0);
    if( check_collisions() != COLLIDE_RIGHT ) {
        piece.pos.x++;
    }
    if( check_collisions() == COLLIDE_BLOCK ) {
        piece.pos.x--;
    }
}

//Handle PCINT interrupt
ISR( PCINT2_vect ) {
    PCICR = 0;
    if( digitalRead(4) == LOW )
        rotate();
    else if( digitalRead(5) == LOW )
        drop();
}

//============================================================================
//============================================================================
// Arduino Wrapper
//----------------------------------------------

int main(void) {
    init();
    setup();
    for(;;)
        loop();
    return 0;
}
