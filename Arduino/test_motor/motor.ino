// ========== Motor Pins ==========
#define STEP_A 2
#define DIR_A 3
#define EN_A 22

#define STEP_B 4
#define DIR_B 5
#define EN_B 24

// ========== Constants ==========
const int SPEED_FAST = 1000;
const int SPEED_SLOW = 3000;
const int SQUARE_DISTANCE = 1;  // Move 1 square
const int SQUARE_SIZE = 275;    // Assuming this is your chessboard square size

// ========== Directions ==========
enum {T_B, B_T, L_R, R_L, DIAG_NE, DIAG_SW, DIAG_SE, DIAG_NW};

// ========== Setup ==========
void setup() {
  Serial.begin(9600);

  // Motor Pins
  pinMode(STEP_A, OUTPUT);
  pinMode(DIR_A, OUTPUT);
  pinMode(EN_A, OUTPUT);
  pinMode(STEP_B, OUTPUT);
  pinMode(DIR_B, OUTPUT);
  pinMode(EN_B, OUTPUT);

  // Enable Motors
  digitalWrite(EN_A, LOW);
  digitalWrite(EN_B, LOW);

  Serial.println("Motor Test: Initialized");
}

// ========== Loop ==========
void loop() {
  // Fast Speed Test
  Serial.println("FAST speed testing...");

  moveMotor(T_B, SPEED_FAST, SQUARE_DISTANCE);
  delay(500);
  moveMotor(B_T, SPEED_FAST, SQUARE_DISTANCE);
  delay(500);
  moveMotor(L_R, SPEED_FAST, SQUARE_DISTANCE);
  delay(500);
  moveMotor(R_L, SPEED_FAST, SQUARE_DISTANCE);
  delay(500);
  moveMotor(DIAG_NE, SPEED_FAST, SQUARE_DISTANCE);
  delay(500);
  moveMotor(DIAG_SW, SPEED_FAST, SQUARE_DISTANCE);
  delay(500);
  moveMotor(DIAG_SE, SPEED_FAST, SQUARE_DISTANCE);
  delay(500);
  moveMotor(DIAG_NW, SPEED_FAST, SQUARE_DISTANCE);
  delay(2000); // Wait 2 seconds

  // Slow Speed Test
  Serial.println("SLOW speed testing...");

  moveMotor(T_B, SPEED_SLOW, SQUARE_DISTANCE);
  delay(500);
  moveMotor(B_T, SPEED_SLOW, SQUARE_DISTANCE);
  delay(500);
  moveMotor(L_R, SPEED_SLOW, SQUARE_DISTANCE);
  delay(500);
  moveMotor(R_L, SPEED_SLOW, SQUARE_DISTANCE);
  delay(500);
  moveMotor(DIAG_NE, SPEED_SLOW, SQUARE_DISTANCE);
  delay(500);
  moveMotor(DIAG_SW, SPEED_SLOW, SQUARE_DISTANCE);
  delay(500);
  moveMotor(DIAG_SE, SPEED_SLOW, SQUARE_DISTANCE);
  delay(500);
  moveMotor(DIAG_NW, SPEED_SLOW, SQUARE_DISTANCE);
  delay(3000); // Wait 3 seconds before repeating
}

// ========== Motor Move Function ==========
void moveMotor(byte direction, int speed, float distance) {
  float step_number = 0;

  if (direction == DIAG_NE || direction == DIAG_SW || direction == DIAG_SE || direction == DIAG_NW)
    step_number = distance * SQUARE_SIZE * 2.0; // Diagonal needs extra length
  else
    step_number = distance * SQUARE_SIZE;

  // Set motor directions
  if (direction == R_L || direction == T_B || direction == DIAG_SW)
    digitalWrite(DIR_A, HIGH);
  else
    digitalWrite(DIR_A, LOW);

  if (direction == B_T || direction == R_L || direction == DIAG_NW)
    digitalWrite(DIR_B, HIGH);
  else
    digitalWrite(DIR_B, LOW);

  // Move motors
  for (int x = 0; x < step_number; x++) {
    if (direction == DIAG_SE || direction == DIAG_NW)
      digitalWrite(STEP_A, LOW);
    else
      digitalWrite(STEP_A, HIGH);

    if (direction == DIAG_NE || direction == DIAG_SW)
      digitalWrite(STEP_B, LOW);
    else
      digitalWrite(STEP_B, HIGH);

    delayMicroseconds(speed);
    digitalWrite(STEP_A, LOW);
    digitalWrite(STEP_B, LOW);
    delayMicroseconds(speed);
  }

  Serial.print("Moved: ");
  Serial.print(direction);
  Serial.print(" | Steps: ");
  Serial.println(step_number);
}
