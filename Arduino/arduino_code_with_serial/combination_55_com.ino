//***********************************************
//
//            AI VS HUMAN CHESSBOARD
//
//***********************************************

//******************************  INCLUDING FILES
#include "global.h"
#include <Wire.h>
#include <ArduinoJson.h>


//****************************************  JSON
void sendTFTMessage(String message) {
    StaticJsonDocument<200> doc;
    doc["msg"] = message;  // Store the formatted message

    String jsonString;
    serializeJson(doc, jsonString);  // Convert JSON to a string
    Serial1.println(jsonString);      // Send JSON to Uno
}



//****************************************  LCD DISPLAY FUNCTION
void startDisplay() {
    if (!game_active) {  // Show default screen if no game is active
        sendTFTMessage("\n"
                       "        CHESS \n"
                       "\n"
                       "     AI VS HUMAN \n"
                       "\n\n"
                       "     Press START!\n");
    }
}

void promptHumanMove() {
    sendTFTMessage("     Please make\n\n"
                   "      your move"
                   "\n\n\n\n  Press Move Button");
}

void promptHumanMoveCheck() {
    sendTFTMessage("       Check!\n"
                   "     Please make\n\n"
                   "      your move"
                   "\n\n\n  Press Move Button");
}


//****************************************  SEND TO PYTHON
void sendToPython(String data) {
    Serial.println(data);
}



//****************************************  RECEIVE DATA FROM PYTHON
String receiveFromPython(unsigned long timeout = 3000) {
    unsigned long start_time = millis();
    
    while (millis() - start_time < timeout) {
        if (Serial.available()) {
            String received_data = Serial.readStringUntil('\n');
            received_data.trim(); 
            return received_data; 
        }
    }
    
    return "TIMEOUT";  // Return "TIMEOUT" if no data received
}


//****************************************  SETUP
void setup() {
  Serial.begin(9600);
  Serial1.begin(9600);


  // Initialize Motor Pins
  pinMode(STEP_A, OUTPUT);
  pinMode(DIR_A, OUTPUT);
  pinMode(EN_A, OUTPUT);
  pinMode(STEP_B, OUTPUT);
  pinMode(DIR_B, OUTPUT);
  pinMode(EN_B, OUTPUT);

  // Initialize Electromagnet and Button
  pinMode(MAGNET, OUTPUT);
  pinMode(START_BUTTON, INPUT_PULLUP);
  pinMode(END_BUTTON, INPUT_PULLUP);
  pinMode(MOVE_CONFIRM_BUTTON, INPUT);
  pinMode(EASY_BUTTON, INPUT);
  pinMode(HARD_BUTTON, INPUT);

  // Initialize Limit Switches
  pinMode(LIMIT_X, INPUT_PULLUP);
  pinMode(LIMIT_Y, INPUT_PULLUP);

  // Disable Motors initially
  digitalWrite(EN_A, HIGH);
  digitalWrite(EN_B, HIGH);

  Serial.println("DEBUG: System initialized...");
    
  startDisplay();
}

//*****************************************  LOOP
void loop() {
    stop_execution = false;
    unsigned long current_time = millis(); // Get the current time

    // Read button states
    if (digitalRead(START_BUTTON) == LOW) sequence = start;
    else if (digitalRead(END_BUTTON) == LOW) sequence = end;
    else if (digitalRead(MOVE_CONFIRM_BUTTON) == HIGH) sequence = move_confirm;
    else if (digitalRead(EASY_BUTTON) == HIGH) sequence = easy;
    else if (digitalRead(HARD_BUTTON) == HIGH) sequence = hard;

    // Switch-Case to handle button actions
    switch (sequence) {
        case start:
            if (!game_active && (current_time - last_start_press_time > debounce_delay)) {
                last_start_press_time = current_time;

                sendToPython("START");
                String python_response = receiveFromPython(3000);

                if (python_response == "START_OK") {
                    startGame();  // Start the game only if Python confirmed
                } else if (python_response == "START_ERROR") {
                    sendTFTMessage("\n Board Setup Error!\n"
                                  "\n    Press Start");
                    delay(1500);
                    startDisplay();
                } else if (python_response == "TIMEOUT") {
                    Serial.println("DEBUG: No response from Python. Try again.");
                    sendTFTMessage("\n No Response\n"
                                  "\n    Press Start");
                    delay(1500);
                    startDisplay();
                }
             } else if (game_active && (current_time - last_start_press_time > debounce_delay)) {
                last_start_press_time = current_time;
                Serial.println("DEBUG: Game in Progress");
            }
            break;

        case end:
            if (game_active && (current_time - last_end_press_time > debounce_delay)) {
                last_end_press_time = current_time;
                sendToPython("END");
                stopGame();
            } else if (!game_active && (current_time - last_end_press_time > debounce_delay)) {
                last_end_press_time = current_time;
                sendTFTMessage("\n\n       No game\n\n     in progress");
                delay(1500);
                startDisplay();
            }
            break;

        case move_confirm:
            if (game_active && !difficulty_selected) {
                sendTFTMessage("\n    Please select\n"
                              "\n     difficulty\n\n"
                              "\n    EASY or HARD");
                delay(1500);
            } else if (game_active && difficulty_selected) {
                sendToPython("MOVE_CONFIRM");
                String python_response = receiveFromPython(5000);

                if (python_response == "MOVE_OK") {
                    Serial.println("DEBUG: Move passed from Python.");
                    
                    // Wait for Human Move Status
                    sendToPython("ASK_HUMAN_STATUS");
                    String human_status = receiveFromPython(5000);
                    Serial.print("DEBUG: Human Status: "); Serial.println(human_status);

                    if (human_status.endsWith("CHECK") || human_status == "GAME_CONTINUES") {
                        if (human_status.endsWith("CHECK")) {
                           sendTFTMessage("\n\n " + human_status);
                           delay(6000);
                        }

                        // Human Move is Valid, Now Wait for AI Move
                        sendToPython("ASK_AI_MOVE");
                        String aiMove = receiveFromPython(5000);
                        Serial.print("DEBUG: AI Move Received: "); Serial.println(aiMove);

                        // Validate AI move format (e.g., "e2e4:0" or "e7e8:1")
                        if (aiMove.length() == 6 && aiMove[4] == ':') {
                            char move[5];
                            aiMove.substring(0, 4).toCharArray(move, 5);
                            capture_flag = aiMove[5] - '0'; // '0' or '1'

                            String displayMessage = "\n\n AI MOVE: " + String(move);

                            if (capture_flag == 0) {
                                displayMessage += "\n\n Capture";
                            }

                            sendTFTMessage(displayMessage);
                            piece_movement(move, capture_flag);

                            // Wait for AI Move Status
                            sendToPython("ASK_AI_STATUS");
                            String ai_status = receiveFromPython(5000);
                            Serial.print("DEBUG: AI Status: "); Serial.println(ai_status);
                            if (ai_status.endsWith("CHECK") || ai_status == "GAME_CONTINUES") {
                                Serial.println("DEBUG: Perform Human Move and Press Move Confirm");

                                if (ai_status.endsWith("CHECK")) {
                                    sendTFTMessage("\n\n " + ai_status);
                                    delay(6000);
                                    promptHumanMoveCheck();  // ✅ Human is in check, use special prompt
                                } else {
                                    promptHumanMove();  // ✅ Normal move prompt
                                }
                            } else {
                                // Game Over from AI Move (Checkmate, Stalemate, Draw)
                                Serial.println("DEBUG: Game Over. Status: " + ai_status);
                                sendTFTMessage("\n\n " + ai_status);
                                delay(10000);
                                stopGame();
                            }
                        } else {
                            Serial.println("DEBUG: Invalid AI Move format.");
                            sendTFTMessage("\n Invalid Move Format!");
                            delay(2000);
                            promptHumanMove();
                        }
                    } else {
                        // Game Over from Human Move (Checkmate, Stalemate, Draw)
                        Serial.println("DEBUG: Game Over. Status: " + human_status);
                        sendTFTMessage("\n\n " + human_status);
                        delay(10000);
                        stopGame();
                    }
                } else if (python_response.startsWith("MOVE_ERROR")) {
                    if (python_response.startsWith("MOVE_ERROR_CHECK")) {
                        Serial.println("DEBUG: Invalid move and you are in check!");

                        // Extract the moves from the response
                        int colonIndex = python_response.indexOf(':');
                        String moves = python_response.substring(colonIndex + 1);

                        // Display the check warning and available moves
                        sendTFTMessage("\n      CHECK \n"
                                      "\n   Invalid Move\n"
                                      "\n  Available Moves:\n\n" 
                                      + moves);

                        delay(3000);
                    } else {
                        Serial.println("DEBUG: Invalid move or invalid board!");

                        sendTFTMessage("\n    Invalid Move\n"
                                      "\n   \n"
                                      "\n    Try Again");

                        delay(2000);
                        promptHumanMove();
                    }
                } else if (python_response == "MOVE_NO_CHANGE") {
                    Serial.println("DEBUG: No board changes detected.");
                    sendTFTMessage("\n No Change Detected\n"
                                  "\n    Try Again");
                    delay(2000);
                    promptHumanMove();
                } else if (python_response == "TIMEOUT") {
                    Serial.println("DEBUG: Did not receive AI move.");
                    sendTFTMessage("  AI Move Timeout!");
                    delay(2000);
                    promptHumanMove();
                }
            } else {
                sendTFTMessage("\n\n       No game\n\n     in progress");
                delay(1500);
                startDisplay();
            }
            delay(250);
            break;


        case easy:
            if (!game_active) {
                sendTFTMessage("\n\n       No game\n\n     in progress");
                delay(1500);
                startDisplay();
            } else if (!difficulty_selected) {
                difficulty_selected = true;
                sendToPython("EASY");
                sendTFTMessage("\n\n\n      EASY MODE!");
                delay(1500);
                Serial.println("DEBUG: Perform Human Move and Press Move Confirm.");
                promptHumanMove();
            }
            break;

        case hard:
            if (!game_active) {
                sendTFTMessage("\n\n       No game\n\n     in progress");
                delay(1500);
                startDisplay();
            } else if (!difficulty_selected) {
                difficulty_selected = true;
                sendToPython("HARD");
                sendTFTMessage("\n\n\n      HARD MODE!");
                delay(1500);
                Serial.println("DEBUG: Perform Human Move and Press Move Confirm.");
                promptHumanMove();
            }
            break;

        case no_pressed:
        default:
            break;
    }

    sequence = no_pressed;

}



//************************************  START GAME
void startGame() {
  if (!game_active) {
    game_active = true;
    difficulty_selected = false;

    sendTFTMessage("\n   Game Starting...\n\n\n\n"
                   "    Please Wait\n");

    // Enable Motors
    digitalWrite(EN_A, LOW);
    digitalWrite(EN_B, LOW);

    // Calibrate Trolley
    calibratePosition();
    if (stop_execution) return;

    Serial.println("DEBUG: Select difficulty (Easy or Hard) to continue.");
    sendTFTMessage("\n    Please select\n"
                   "\n     difficulty\n\n"
                   "\n    EASY or HARD");
  }
}



//************************************  STOP GAME
void stopGame() {
    if (game_active) {    
      game_active = false;
      stop_execution = true;
      difficulty_selected = false;

      // Disable Motors and Magnet
      digitalWrite(EN_A, HIGH);
      digitalWrite(EN_B, HIGH);
      digitalWrite(MAGNET, LOW);

      // Reset global variables related to game state
      capture_flag = 1;
      captured_piece_count = 0;
      trolley_coordinate_X = 5;
      trolley_coordinate_Y = 7;

      sendTFTMessage("\n  Stopping game...");
      delay(1500);
      startDisplay();
    }
}



//************************************  CHECK STOP CONDITION
bool checkStopCondition() {
    unsigned long current_time = millis();   // Get current time

    if (digitalRead(END_BUTTON) == LOW) {  
        // debounce timing
        if (current_time - last_end_press_time < debounce_delay) return false;

        last_end_press_time = current_time;  // Update last press time
        Serial.println("DEBUG: Game interrupted. Stopping...");
        sendToPython("END");
        stopGame();
        return true;  // Stop execution
    }
    return false;  // Continue execution
}



//************************************  CALIBRATE
void calibratePosition() {
    if (stop_execution) return;  // NEW: Exit immediately if stopping

    Serial.println("DEBUG: Starting calibration...");

    // Move to X limit (left side)
    Serial.println("DEBUG: Moving towards X-axis limit switch...");
    while (digitalRead(LIMIT_X) == HIGH) {
        if (stop_execution) return;
        motor(B_T, SPEED_SLOW, calibrate_speed);
    }
    if (stop_execution) return;
    Serial.println("DEBUG: X-axis limit switch reached!");

    // Move to Y limit (bottom side)
    Serial.println("DEBUG: Moving towards Y-axis limit switch...");
    while (digitalRead(LIMIT_Y) == HIGH) {
        if (stop_execution) return;
        motor(L_R, SPEED_SLOW, calibrate_speed);
    }
    if (stop_execution) return;
    Serial.println("DEBUG: Y-axis limit switch reached!");

    delay(500);
    if (stop_execution) return;

    // Move to starting position (e7)
    Serial.print("DEBUG: Moving to starting position: X=");
    Serial.print(TROLLEY_START_POSITION_X);
    Serial.print(", Y=");
    Serial.println(TROLLEY_START_POSITION_Y);

    motor(R_L, SPEED_FAST, TROLLEY_START_POSITION_X);
    motor(T_B, SPEED_FAST, TROLLEY_START_POSITION_Y);
    if (stop_execution) return;

    delay(500);
    if (stop_execution) return;

    Serial.println("DEBUG: Calibration complete!");
}




//****************************************  MOTOR
void motor(byte direction, int speed, float distance) {
  if (stop_execution) return;

  float step_number = 0;

  //  Calcul the distance
  if (distance == calibrate_speed) step_number = 4;
  else if (direction == DIAG_NE || direction == DIAG_SW || direction == DIAG_SE || direction == DIAG_NW) step_number = distance * SQUARE_SIZE * 2.00; //  Add an extra length for the diagonal
  else step_number = distance * SQUARE_SIZE;

  //  Direction of the motor rotation
  if (direction == R_L || direction == T_B || direction == DIAG_SW) digitalWrite(DIR_A, HIGH);
  else digitalWrite(DIR_A, LOW);
  if (direction == B_T || direction == R_L || direction == DIAG_NW) digitalWrite(DIR_B, HIGH);
  else digitalWrite(DIR_B, LOW);

  //  Active the motors
  for (int x = 0; x < step_number; x++) {
    if (checkStopCondition() || stop_execution) return;

    if (direction == DIAG_SE || direction == DIAG_NW) digitalWrite(STEP_A, LOW);
    else digitalWrite(STEP_A, HIGH);
    if (direction == DIAG_NE || direction == DIAG_SW) digitalWrite(STEP_B, LOW);
    else digitalWrite(STEP_B, HIGH);
    delayMicroseconds(speed);
    digitalWrite(STEP_A, LOW);
    digitalWrite(STEP_B, LOW);
    delayMicroseconds(speed);
  }
}



// *******************************  ELECTROMAGNET
void electromagnet(boolean state) {

  if (state == true)  {
    digitalWrite(MAGNET, HIGH);
    delay(600);
  }
  else  {
    delay(600);
    digitalWrite(MAGNET, LOW);
  }
}



// ***********************  PIECE MOVEMENT FUNCTION
void piece_movement(char *move, byte capture_flag) {
  
  Serial.println("DEBUG: Processing Move...");
  if (stop_execution) return;

  // Convert input (e.g., "e2e4") into board coordinates
  int departure_coord_X = move[0] - 'a' + 1;
  int departure_coord_Y = move[1] - '0';
  int arrival_coord_X = move[2] - 'a' + 1;
  int arrival_coord_Y = move[3] - '0';
  byte displacement_X = 0;
  byte displacement_Y = 0;

  Serial.print("DEBUG: Moving from: ");
  Serial.print(move[0]); Serial.print(move[1]);
  Serial.print(" to ");
  Serial.print(move[2]); Serial.println(move[3]);

  Serial.print("Capture Flag: ");
  Serial.println(capture_flag);

  // ==================== CAPTURING LOGIC ====================
  for (byte i = capture_flag; i < 2; i++) {
    if (stop_execution) return;
    if (i == 0) { // 0 means capture, trolley will go to arrival
      displacement_X = abs(arrival_coord_X - trolley_coordinate_X);
      displacement_Y = abs(arrival_coord_Y - trolley_coordinate_Y);

      if (arrival_coord_X > trolley_coordinate_X) motor(T_B, SPEED_FAST, displacement_X);
      else if (arrival_coord_X < trolley_coordinate_X) motor(B_T, SPEED_FAST, displacement_X);
      if (arrival_coord_Y > trolley_coordinate_Y) motor(L_R, SPEED_FAST, displacement_Y);
      else if (arrival_coord_Y < trolley_coordinate_Y) motor(R_L, SPEED_FAST, displacement_Y);

      if (stop_execution) return;
      Serial.println("DEBUG: Trolley moved to the captured piece");   
      // update Trolley location
      trolley_coordinate_X = arrival_coord_X;
      trolley_coordinate_Y = arrival_coord_Y;

    }
    else if (i == 1) { // 1 means no capture, trolley will go to departure
      displacement_X = abs(departure_coord_X - trolley_coordinate_X);
      displacement_Y = abs(departure_coord_Y - trolley_coordinate_Y);

      if (departure_coord_X > trolley_coordinate_X) motor(T_B, SPEED_FAST, displacement_X);
      else if (departure_coord_X < trolley_coordinate_X) motor(B_T, SPEED_FAST, displacement_X);
      if (departure_coord_Y > trolley_coordinate_Y) motor(L_R, SPEED_FAST, displacement_Y);
      else if (departure_coord_Y < trolley_coordinate_Y) motor(R_L, SPEED_FAST, displacement_Y);

      if (stop_execution) return;
      Serial.println("DEBUG: Trolley moved to the departure piece");   
    }

    if (stop_execution) return;
    if (i == 0) { // removes the captured piece to the corner
      // Grab the captured piece
      electromagnet(true);

      Serial.print("DEBUG: Captured Count: ");
      Serial.println(captured_piece_count);

      // Move to grid line .5 except for coord Y (8,6,7) must be atleast 5
      float additional_displacement;
      if (arrival_coord_Y == 8) additional_displacement = 3.5;
      else if (arrival_coord_Y == 7) additional_displacement = 2.5;
      else if (arrival_coord_Y == 6) additional_displacement = 1.5;
      else additional_displacement = 0.5;
      motor(B_T, SPEED_SLOW, 0.5); // Move slightly top first
      delay(500);
      motor(R_L, SPEED_SLOW, additional_displacement); // Move slightly right first
      delay(500);
      if (stop_execution) return;

      // Even count → 1st stack, Odd count → 2nd stack
      bool is_second_stack = (captured_piece_count % 2 == 1); 
      motor(B_T, SPEED_SLOW, arrival_coord_X - (is_second_stack ? 0.0 : 0.5)); // Move towards the capture removal zone
      delay(500);

      //Stacking logic and positions, with special handling for 5,6,7,8
      float adjusted_Y;
      if (arrival_coord_Y >= 5) {
          adjusted_Y = 4.5; // Since Y=8,7,6 are assumed at 5, they must be at 4.5
      } else {
          adjusted_Y = (10 - arrival_coord_Y) - 0.5; // Normal reversal for 4,3,2,1
      }
      float stacking_offset = adjusted_Y - ((captured_piece_count / 2) * 0.5); // Calculate stacking position with -0.5 decrement per piece
      motor(L_R, SPEED_SLOW, stacking_offset); // Move to the stacking area
      delay(500);

      // Release the piece
      electromagnet(false); 
      delay(200);

      // Move back to align the trolley
      float return_movement = stacking_offset - additional_displacement;
      if (return_movement > 0) { 
          motor(R_L, SPEED_FAST, return_movement); // Move right as normal 
      } else {
          motor(L_R, SPEED_FAST, abs(return_movement)); // Move left instead
      }
      Serial.print("DEBUG: Stacking Offset: ");
      Serial.println(stacking_offset);
      Serial.print("DEBUG: Additional Displacement: ");
      Serial.println(additional_displacement);
      Serial.print("DEBUG: Return Movement: ");
      Serial.println(return_movement);
      delay(500);
      motor(T_B, SPEED_FAST, (arrival_coord_X) + (is_second_stack ? 0.5 : 0.0));
      delay(500);

      trolley_coordinate_X = arrival_coord_X;
      trolley_coordinate_Y = arrival_coord_Y;
      captured_piece_count++;
    }
  }

  if (stop_execution) return;
  trolley_coordinate_X = arrival_coord_X;
  trolley_coordinate_Y = arrival_coord_Y;

  //  Move the chess piece to the arrival position
  displacement_X = abs(arrival_coord_X - departure_coord_X);
  displacement_Y = abs(arrival_coord_Y - departure_coord_Y);

  // Pick up the piece
  electromagnet(true);
  delay(500);

  // Move piece to destination
  Serial.println("DEBUG: Moving piece to destination...");

  if (stop_execution) return;
  // ==================== KNIGHT DISPLACEMENT ====================
  if ((displacement_X == 1 && displacement_Y == 2) || (displacement_X == 2 && displacement_Y == 1)) {
    if (displacement_Y == 2) {
      if (departure_coord_X < arrival_coord_X) {
        motor(T_B, SPEED_SLOW, displacement_X * 0.5);
        if (departure_coord_Y < arrival_coord_Y) motor(L_R, SPEED_SLOW, displacement_Y);
        else motor(R_L, SPEED_SLOW, displacement_Y);
        motor(T_B, SPEED_SLOW, displacement_X * 0.5);
      }
      else if (departure_coord_X > arrival_coord_X) {
        motor(B_T, SPEED_SLOW, displacement_X * 0.5);
        if (departure_coord_Y < arrival_coord_Y) motor(L_R, SPEED_SLOW, displacement_Y);
        else motor(R_L, SPEED_SLOW, displacement_Y);
        motor(B_T, SPEED_SLOW, displacement_X * 0.5);
      }
    }
    else if (displacement_X == 2) {
      if (departure_coord_Y < arrival_coord_Y) {
        motor(L_R, SPEED_SLOW, displacement_Y * 0.5);
        if (departure_coord_X < arrival_coord_X) motor(T_B, SPEED_SLOW, displacement_X);
        else motor(B_T, SPEED_SLOW, displacement_X);
        motor(L_R, SPEED_SLOW, displacement_Y * 0.5);
      }
      else if (departure_coord_Y > arrival_coord_Y) {
        motor(R_L, SPEED_SLOW, displacement_Y * 0.5);
        if (departure_coord_X < arrival_coord_X) motor(T_B, SPEED_SLOW, displacement_X);
        else motor(B_T, SPEED_SLOW, displacement_X);
        motor(R_L, SPEED_SLOW, displacement_Y * 0.5);
      }
    }
  }

  if (stop_execution) return;
  // ==================== DIAGONAL DISPLACEMENT ====================
  else if (displacement_X == displacement_Y) {
    if (departure_coord_X > arrival_coord_X && departure_coord_Y > arrival_coord_Y) motor(DIAG_NW, SPEED_SLOW, displacement_X);
    else if (departure_coord_X > arrival_coord_X && departure_coord_Y < arrival_coord_Y) motor(DIAG_NE, SPEED_SLOW, displacement_X);
    else if (departure_coord_X < arrival_coord_X && departure_coord_Y > arrival_coord_Y) motor(DIAG_SW, SPEED_SLOW, displacement_X);
    else if (departure_coord_X < arrival_coord_X && departure_coord_Y < arrival_coord_Y) motor(DIAG_SE, SPEED_SLOW, displacement_X);
  }

  if (stop_execution) return;
  // ==================== HORIZONTAL DISPLACEMENT ====================
  else if (displacement_Y == 0) {
    if (departure_coord_X > arrival_coord_X) motor(B_T, SPEED_SLOW, displacement_X);
    else if (departure_coord_X < arrival_coord_X) motor(T_B, SPEED_SLOW, displacement_X);
  }

  if (stop_execution) return;
  // ==================== VERTICAL DISPLACEMENT ====================
  else if (displacement_X == 0) {
    if (departure_coord_Y > arrival_coord_Y) motor(R_L, SPEED_SLOW, displacement_Y);
    else if (departure_coord_Y < arrival_coord_Y) motor(L_R, SPEED_SLOW, displacement_Y);
  }

  // Drop the piece
  if (stop_execution) return;
  electromagnet(false);
  delay(500);

  Serial.println("DEBUG: Move Completed!");
}

