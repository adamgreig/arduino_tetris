//Constants that are set at the start of SPI messages to the LCD
#define INDEX 0x00
#define DATA 0x02
#define IDBYTE 0x74

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
#define COLLIDE_SIDE    3
#define COLLIDE_TOP     4
