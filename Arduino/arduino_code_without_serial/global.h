//  Chessboard
const float TROLLEY_START_POSITION_X = 2;
const float TROLLEY_START_POSITION_Y = 5.7;

byte trolley_coordinate_X = 5;
byte trolley_coordinate_Y = 7;

byte capture_flag = 1;
byte captured_piece_count = 0;
boolean game_active = false;
boolean stop_execution = false;
boolean difficulty_selected = false;
String game_difficulty = "";


// Debounce timing
const unsigned long debounce_delay = 200;  // 200 milliseconds
unsigned long last_start_press_time = 0;
unsigned long last_end_press_time = 0;

// Game Button Actions
enum {
    no_pressed,     // initial
    start,    // Start game
    end,      // End game
    move_confirm,
    easy,
    hard // Confirm player move
};
byte sequence = no_pressed;

//  Movement
enum {T_B, B_T, L_R, R_L, DIAG_NE, DIAG_SW, DIAG_SE, DIAG_NW, eight, nine, calibrate_speed};
// T=Top  -  B=Bottom  -  L=Left  -  R=Right 

//  Electromagnet
#define MAGNET 6

//  Motor
#define STEP_A 2
#define DIR_A 3
#define EN_A 22

#define STEP_B 4
#define DIR_B 5
#define EN_B 24

const int SQUARE_SIZE = 275;
const int SPEED_SLOW (3000);
const int SPEED_FAST (1000);

//  Button and switches
const byte LIMIT_X = 7;
const byte LIMIT_Y = 8;
const byte EASY_BUTTON = 9;
const byte HARD_BUTTON = 10;
const byte MOVE_CONFIRM_BUTTON = 11;
const byte START_BUTTON = 12;
const byte END_BUTTON = 13;

