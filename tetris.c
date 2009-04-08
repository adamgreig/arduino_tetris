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
    char rotation;
} Piece;

//============================================================================
// The seven tetrominoes, and their rotations.
//  Included here so as to be after the struct definitions.
//----------------------------------------------

#include "tetrominoes.h"

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

//Check the top row for any blocks causing game over
char check_top_row(void);

//Enable interrupts
void enable_interrupts(void);

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

//Is it game over?
char game_over = 0;

//The player's score
unsigned short int score = 0;

//The game speed, starts at 300ms and drops
unsigned short int speed = 300;

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
char flip = 0;
void loop() {
    
    if( !piece.in_play )
        new_piece();
    
    apply_gravity();

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
                    
                    if( a - piece.pos.x < 4 && b - piece.pos.y < 4 ) {
                        if( (piece.pos.x + piece.points[0].x == a &&
                             piece.pos.y + piece.points[0].y == b) ||
                            (piece.pos.x + piece.points[1].x == a &&
                             piece.pos.y + piece.points[1].y == b) ||
                            (piece.pos.x + piece.points[2].x == a &&
                             piece.pos.y + piece.points[2].y == b) ||
                            (piece.pos.x + piece.points[3].x == a &&
                             piece.pos.y + piece.points[3].y == b) )
                                send_colour(piece.colour);
                        else
                            send_colour(grid[a][b]);
                    } else {
                        send_colour(grid[a][b]);
                    }
                }
                //Draw black everywhere else
            } else {
                send_colour(BLACK);
            }
        }
    }
    
    if( game_over )
        for(;;);
    
    //Enable interrupts
    PCICR = (1<<PCIE2);
    PCMSK2 = (1<<PCINT18) | (1<<PCINT19) | (1<<PCINT20) | (1<<PCINT21);
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
            unsigned short int c;
            for(a=b+1; a<20; a++) {
                for(c=0; c<10; c++) {
                    grid[c][a - 1] = grid[c][a];
                }
            }
            for(c=0; c<10; c++) {
                grid[c][19] = BLACK;
            }
            
            //Having moved everything down we might have another line at
            // this position, so decrement b
            b--;
            
            //Give them a point
            score++;
            
            //Make it all go a tad faster
            speed -= 5;
            if( speed < 50 ) speed = 50;
        }
    }
}

//Check for anything on the top of the grid
char check_top_row() {
    char i;
    for(i=0; i<10; i++) {
        if(grid[i][19])
            return 1;
    }
    return 0;
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
    if( millis() - milliseconds > speed ) {
        
        piece.pos.y--;
        
        if( check_collisions() ) {
            piece.pos.y++;
            blit();
            check_completed_lines();
            piece.in_play = 0;
            
            if( check_top_row() ) {
                memset(grid, RED, 200);
                memset(grid, GREEN, score);
                memset(piece.points, RED, 8);
                game_over = 1;
            }
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
            memcpy(piece.points, PieceI[0], 8);
            piece.colour = CYAN;
            break;
        case 1:
            memcpy(piece.points, PieceJ[0], 8);
            piece.colour = BLUE;
            break;
        case 2:
            memcpy(piece.points, PieceL[0], 8);
            piece.colour = ORANGE;
            break;
        case 3:
            memcpy(piece.points, PieceO[0], 8);
            piece.colour = YELLOW;
            break;
        case 4:
            memcpy(piece.points, PieceS[0], 8);
            piece.colour = GREEN;
            break;
        case 5:
            memcpy(piece.points, PieceT[0], 8);
            piece.colour = PURPLE;
            break;
        case 6:
            memcpy(piece.points, PieceZ[0], 8);
            piece.colour = RED;
            break;
    }

    //Inititalise its starting values
    piece.pos.x = 5;
    piece.pos.y = 19;
    piece.rotation = 0;
    piece.in_play = 1;
}

//============================================================================
// Piece Control Functions
//----------------------------------------------

//Check if the current piece has any collisions
char check_collisions() {
    char i, x, y;
    for(i=0; i<4; i++) {
        x = piece.pos.x + piece.points[i].x;
        y = piece.pos.y + piece.points[i].y;
        if( grid[x][y] || x < 0 || x > 9 || y < 0 )
            return 1;
    }
    return 0;
}

//Blit piece to grid
void blit() {
    char i, x, y;
    for(i=0; i<4; i++) {
        x = piece.pos.x + piece.points[i].x;
        y = piece.pos.y + piece.points[i].y;
        if( x >= 0 && x < 10 && y >= 0 && y < 20 )
            grid[x][y] = piece.colour;
    }
}

//Actually copy the relevent new bytes in
void apply_rotation() {
    switch(piece.colour) {
        case CYAN:
            memcpy(piece.points, PieceI[piece.rotation], 8);
            break;
        case BLUE:
            memcpy(piece.points, PieceJ[piece.rotation], 8);
            break;
        case ORANGE:
            memcpy(piece.points, PieceL[piece.rotation], 8);
            break;
        case YELLOW:
            memcpy(piece.points, PieceO[piece.rotation], 8);
            break;
        case GREEN:
            memcpy(piece.points, PieceS[piece.rotation], 8);
            break;
        case PURPLE:
            memcpy(piece.points, PieceT[piece.rotation], 8);
            break;
        case RED:
            memcpy(piece.points, PieceZ[piece.rotation], 8);
            break;
    }
}

//Rotate a piece
void rotate() {
    piece.rotation++;
    if( piece.rotation > 3 ) piece.rotation = 0;
    apply_rotation();
    if( check_collisions() ) {
        piece.rotation--;
        if( piece.rotation < 0 ) piece.rotation = 3;
        apply_rotation();
    }
}

void drop() {
    piece.pos.y -= 2;
    if( check_collisions() ) {
        piece.pos.y += 2;
    }
}

//Move a piece left
void move_left() {
    piece.pos.x--;
    if( check_collisions() ) {
        piece.pos.x++;
    }
}

//Move a piece right
void move_right() {
    piece.pos.x++;
    if( check_collisions() ) {
        piece.pos.x--;
    }
}

//Handle PCINT interrupt
ISR( PCINT2_vect ) {
    PCICR = 0;
    if( digitalRead(2) == LOW )
        move_right();
    else if( digitalRead(3) == LOW )
        move_left();
    else if( digitalRead(4) == LOW )
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
