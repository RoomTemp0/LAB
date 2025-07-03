#include <math.h>     // For fabs(), sqrt(), and fmod()

// --- USER'S ORIGINAL PIN DEFINITIONS ---
const int digitalPin1 = 8;  // User's input pin for initial move to scan rect corner
const int digitalPin2 = 12; // User's input pin for starting the scan within selected circle
const int digitalPin9 = 9;  // Selects Circle 2 (index 2: Bottom-Left)
const int digitalPin10 = 10; // Selects Circle 1 (index 1: Top-Right)
const int digitalPin11 = 11; // Selects Circle 0 (index 0: Top-Left)
const int analogpin1 = A0;
const int analogpin2 = A1;


// Motor 1 (X-axis)
const int X_MOTOR_PIN_A = 2;
const int X_MOTOR_PIN_B = 3;

// Motor 2 (Y-axis)
const int Y_MOTOR_PIN_A = 4;
const int Y_MOTOR_PIN_B = 5;

// --- USER'S ORIGINAL SPEED VARIABLE ---
float speed = 0.53; // mm/s

// --- 1. Main Stage Parameters (Overall Physical Dimensions) ---
#define STAGE_WIDTH_MM  75.0  // Total usable width of the main stage in mm (X-dimension)
#define STAGE_HEIGHT_MM 81.0  // Total usable height of the main stage in mm (Y-dimension)

// Define the absolute starting point of your stage in mm.
// NOW: (0,0) is defined as the CENTER of the SCAN_RECT.
#define HOME_POS_MM_X 0.0
#define HOME_POS_MM_Y 0.0

// --- 2. Rectangular Scanning Zone Parameters (The larger rectangular area encompassing the circles) ---
#define SCAN_RECT_SIZE_X 13.05 // mm
#define SCAN_RECT_SIZE_Y 12.15 // mm

// --- MODIFIED: SCAN_RECT is now centered at (0,0) ---
#define SCAN_RECT_MIN_X (-SCAN_RECT_SIZE_X / 2.0) // Extends from -size/2 to +size/2
#define SCAN_RECT_MAX_X (SCAN_RECT_SIZE_X / 2.0)
#define SCAN_RECT_MIN_Y (-SCAN_RECT_SIZE_Y / 2.0)
#define SCAN_RECT_MAX_Y (SCAN_RECT_SIZE_Y / 2.0)

// With (0,0) at the center of SCAN_RECT, its center is simply (0,0)
#define SCAN_RECT_CENTER_X 0.0
#define SCAN_RECT_CENTER_Y 0.0

// Define the step size for scanning within this inner rectangular zone
const float SCAN_STEP_MM_X = 0.2; // Recommended for good resolution within small circles
const float SCAN_STEP_MM_Y = 0.2; // Recommended for good resolution within small circles

// --- 3. CIRCULAR SCANNING AREA PARAMETERS (Four Circles) ---
#define CIRCLE_DIAMETER_MM 3.4 // mm
#define CIRCLE_RADIUS_MM   (CIRCLE_DIAMETER_MM / 2.0) // 1.7 mm

// Offset of each circle's center from the center of the SCAN_RECT (which is now 0,0)
#define CIRCLE_OFFSET_MM_X 3.2 // Offset in X direction
#define CIRCLE_OFFSET_MM_Y 3.2 // Offset in Y direction

// Define the centers of the four circles using an array of structs for cleaner management
struct CircleCenter {
  float x;
  float y;
};

// Define the 4 circle centers relative to the (0,0) origin (which is SCAN_RECT's center)
// Order: Top-Left (0), Top-Right (1), Bottom-Left (2), Bottom-Right (3)
CircleCenter circleCenters[] = {
  {SCAN_RECT_CENTER_X - CIRCLE_OFFSET_MM_X, SCAN_RECT_CENTER_Y + CIRCLE_OFFSET_MM_Y}, // Index 0: Top-Left
  {SCAN_RECT_CENTER_X + CIRCLE_OFFSET_MM_X, SCAN_RECT_CENTER_Y + CIRCLE_OFFSET_MM_Y}, // Index 1: Top-Right
  {SCAN_RECT_CENTER_X - CIRCLE_OFFSET_MM_X, SCAN_RECT_CENTER_Y - CIRCLE_OFFSET_MM_Y}, // Index 2: Bottom-Left
  {SCAN_RECT_CENTER_X + CIRCLE_OFFSET_MM_X, SCAN_RECT_CENTER_Y - CIRCLE_OFFSET_MM_Y}  // Index 3: Bottom-Right
};
const int NUM_CIRCLES = sizeof(circleCenters) / sizeof(circleCenters[0]);


// --- Current Position Tracking ---
float currentPosX_mm = HOME_POS_MM_X;
float currentPosY_mm = HOME_POS_MM_Y;

// --- USER'S ORIGINAL STATE VARIABLE FOR BUTTON ---
int previousStateDigitalPin2 = LOW;


// --- USER'S ORIGINAL MOTOR CONTROL FUNCTIONS ---
void forward1(float d) { // X-axis positive direction
  float time_ms = (d / speed) * 1000.0;
  digitalWrite(X_MOTOR_PIN_A, HIGH);
  digitalWrite(X_MOTOR_PIN_B, LOW);
  delay(time_ms);
  digitalWrite(X_MOTOR_PIN_A, LOW);
  digitalWrite(X_MOTOR_PIN_B, LOW);
  currentPosX_mm += d; // Update current position
}

void reverse1(float d) { // X-axis negative direction
  float time_ms = (d / speed) * 1000.0;
  digitalWrite(X_MOTOR_PIN_A, LOW);
  digitalWrite(X_MOTOR_PIN_B, HIGH);
  delay(time_ms);
  digitalWrite(X_MOTOR_PIN_A, LOW);
  digitalWrite(X_MOTOR_PIN_B, LOW);
  currentPosX_mm -= d; // Update current position
}

void forward2(float d) { // Y-axis positive direction
  float time_ms = (d / speed) * 1000.0;
  digitalWrite(Y_MOTOR_PIN_A, HIGH);
  digitalWrite(Y_MOTOR_PIN_B, LOW);
  delay(time_ms);
  digitalWrite(Y_MOTOR_PIN_A, LOW);
  digitalWrite(Y_MOTOR_PIN_B, LOW);
  currentPosY_mm += d; // Update current position
}

void reverse2(float d) { // Y-axis negative direction
  float time_ms = (d / speed) * 1000.0;
  digitalWrite(Y_MOTOR_PIN_A, LOW);
  digitalWrite(Y_MOTOR_PIN_B, HIGH);
  delay(time_ms);
  digitalWrite(Y_MOTOR_PIN_A, LOW);
  digitalWrite(Y_MOTOR_PIN_B, LOW);
  currentPosY_mm -= d; // Update current position
}

/**
 * @brief Moves the stage to a specified target (X,Y) coordinate in millimeters.
 * Movement is sequential: X-axis moves, then Y-axis moves.
 * The duration of movement is calculated based on the fixed actuator speed.
 * @param targetX_mm The target X-coordinate in millimeters relative to (0,0).
 * @param targetY_mm The target Y-coordinate in millimeters relative to (0,0).
 */
void moveTo(float targetX_mm, float targetY_mm) {
    float deltaX_mm = targetX_mm - currentPosX_mm;
    float deltaY_mm = targetY_mm - currentPosY_mm;

    Serial.print("Moving from ("); Serial.print(currentPosX_mm); Serial.print(", ");
    Serial.print(currentPosY_mm); Serial.print(") to ("); Serial.print(targetX_mm);
    Serial.print(", "); Serial.print(targetY_mm); Serial.println(") mm");

    // --- Move X-axis ---
    if (fabs(deltaX_mm) > 0.00000001) { // Only move if distance is significant (0.001mm tolerance)
        Serial.print("   X-axis movement: "); Serial.print(deltaX_mm); Serial.println(" mm");
        if (deltaX_mm > 0) {
            forward1(deltaX_mm);
        } else {
            reverse1(fabs(deltaX_mm));
        }
    } else {
        Serial.println("   X-axis: No significant movement needed.");
    }

    // --- Move Y-axis ---
    if (fabs(deltaY_mm) > 0.00000001) { // Only move if distance is significant
        Serial.print("   Y-axis movement: "); Serial.print(deltaY_mm); Serial.println(" mm");
        if (deltaY_mm > 0) {
            forward2(deltaY_mm);
        } else {
            reverse2(fabs(deltaY_mm));
        }
    } else {
        Serial.println("   Y-axis: No significant movement needed.");
    }

    Serial.print("Arrived at ("); Serial.print(currentPosX_mm); Serial.print(", ");
    Serial.print(currentPosY_mm); Serial.println(") mm\n");
}

/**
 * @brief Performs an action at the current stage position.
 * Replace this with your actual spectrometer trigger or data acquisition code.
 */
void performActionAtLocation() {
    Serial.print("Performing action at (");
    Serial.print(currentPosX_mm); Serial.print(", ");
    Serial.print(currentPosY_mm); Serial.println(") mm -- (Spectrometer Trigger/Readout goes here)\n");
    
    int analogValue1 = analogRead(analogpin1);
    int analogValue2 = analogRead(analogpin2);
    int analogValue = analogValue1 - analogValue2;
    float voltage = (analogValue * 5.0) / 1023.0; // Use 1023.0 for exact max voltage mapping
    Serial.print("  Read Voltage Difference:"); Serial.print(analogValue);
    Serial.print(" (Voltage Difference: "); Serial.print(voltage, 3); Serial.println(" V)");

    // Example:
    // digitalWrite(SPECTRA_TRIGGER_PIN, HIGH); // Send trigger pulse
    // delay(10); // Pulse duration
    // digitalWrite(SPECTRA_TRIGGER_PIN, LOW);
    // delay(500); // Time for spectrometer to acquire data
    delay(1000); // Simulate action time
}

/**
 * @brief Checks if a given point (x, y) is inside a specific defined scanning circle.
 * @param checkX The X-coordinate to check.
 * @param checkY The Y-coordinate to check.
 * @param circleIndex The index of the circle in the circleCenters array (0-3).
 * @return true if the point is inside or on the boundary of the specified circle, false otherwise.
 */
bool isPointInSpecificCircle(float checkX, float checkY, int circleIndex) {
    if (circleIndex < 0 || circleIndex >= NUM_CIRCLES) {
        return false; // Invalid circle index
    }

    float radiusSquared = CIRCLE_RADIUS_MM * CIRCLE_RADIUS_MM;
    float dx = checkX - circleCenters[circleIndex].x;
    float dy = checkY - circleCenters[circleIndex].y;
    float distanceSquared = (dx * dx) + (dy * dy);

    return distanceSquared <= radiusSquared;
}


void setup() {
    Serial.begin(9600); // Initialize serial communication
    delay(1000); // Give Serial Monitor time to connect

    // Configure user's input pins
    pinMode(digitalPin1, INPUT);
    pinMode(digitalPin2, INPUT);
    pinMode(digitalPin9, INPUT);   // New: Circle selection input
    pinMode(digitalPin10, INPUT); // New: Circle selection input
    pinMode(digitalPin11, INPUT); // New: Circle selection input

    // Configure motor control pins as outputs
    pinMode(X_MOTOR_PIN_A, OUTPUT);
    pinMode(X_MOTOR_PIN_B, OUTPUT);
    pinMode(Y_MOTOR_PIN_A, OUTPUT);
    pinMode(Y_MOTOR_PIN_B, OUTPUT);

    // Ensure motors are initially off (all pins LOW)
    digitalWrite(X_MOTOR_PIN_A, LOW);
    digitalWrite(X_MOTOR_PIN_B, LOW);
    digitalWrite(Y_MOTOR_PIN_A, LOW);
    digitalWrite(Y_MOTOR_PIN_B, LOW);

    Serial.println("--- Stage Control System Initialized ---");
    Serial.print("Main Stage Size: "); Serial.print(STAGE_WIDTH_MM); Serial.print("x"); Serial.print(STAGE_HEIGHT_MM); Serial.println(" mm");
    Serial.print("Rectangular Scan Area: Min X="); Serial.print(SCAN_RECT_MIN_X); Serial.print(", Max X="); Serial.print(SCAN_RECT_MAX_X);
    Serial.print(", Min Y="); Serial.print(SCAN_RECT_MIN_Y); Serial.print(", Max Y="); Serial.print(SCAN_RECT_MAX_Y); Serial.println(" mm");
    Serial.print("Circular Scan Diameter: "); Serial.print(CIRCLE_DIAMETER_MM); Serial.println(" mm");
    Serial.print("Circular Offset from Rectangular Center: "); Serial.print(CIRCLE_OFFSET_MM_X); Serial.print("mm X, "); Serial.print(CIRCLE_OFFSET_MM_Y); Serial.print("mm Y"); Serial.println(" mm");
    Serial.print("Scanning Step: "); Serial.print(SCAN_STEP_MM_X); Serial.print("x"); Serial.print(SCAN_STEP_MM_Y); Serial.println(" mm");
    Serial.print("Actuator Speed: "); Serial.print(speed); Serial.println(" mm/s");
    Serial.println("Assuming stage is at HOME_POS_MM_X, HOME_POS_MM_Y (0,0) mm, which is the CENTER of the SCAN_RECT.\n");

    Serial.println("Calculated Circle Centers (X, Y):");
    Serial.println("   Index 0 (Pin 11): Top-Left");
    Serial.println("   Index 1 (Pin 10): Top-Right");
    Serial.println("   Index 2 (Pin 9): Bottom-Left");
    Serial.println("   Index 3 (Default): Bottom-Right");

    for(int i = 0; i < NUM_CIRCLES; i++) {
      Serial.print("   Circle "); Serial.print(i); Serial.print(": (");
      Serial.print(circleCenters[i].x); Serial.print(", ");
      Serial.print(circleCenters[i].y); Serial.println(") mm");
    }
    Serial.println();

    // Initialize current position to the assumed home/start
    currentPosX_mm = HOME_POS_MM_X;
    currentPosY_mm = HOME_POS_MM_Y;

    Serial.println("Press digitalPin1 to move to the starting point of the rectangular scan zone.");
    Serial.println("Use digitalPin9, 10, 11, or 12 to select a circle, then press digitalPin2 to start scanning.\n");
}

void loop() {
    int currentStateDigitalPin1 = digitalRead(digitalPin1);
    int currentStateDigitalPin2 = digitalRead(digitalPin2);

    // Store current states of circle selection pins
    int stateDigitalPin9 = digitalRead(digitalPin9);
    int stateDigitalPin10 = digitalRead(digitalPin10);
    int stateDigitalPin11 = digitalRead(digitalPin11);


    // --- Logic for Initializing at starting point (User's inputPin1 logic) ---
    if (currentStateDigitalPin1 == HIGH) {
        Serial.println("digitalPin1 is HIGH. Initializing at starting point of rectangular scanning zone...");
        // This will now move to the bottom-left corner relative to the new (0,0) center.
        moveTo(SCAN_RECT_MIN_X, SCAN_RECT_MIN_Y);
        Serial.println("Stage is at the starting point of the rectangular scanning zone (bottom-left corner).");
        delay(5000); // Debounce delay
        while(digitalRead(digitalPin1) == HIGH); // Wait for button release
    }

    // --- Logic for Starting Scan (User's inputPin2 logic with edge detection) ---
    if (previousStateDigitalPin2 == LOW && currentStateDigitalPin2 == HIGH) {
        int selectedCircleIndex = -1; // -1 means no circle selected yet

        // Determine which circle is selected based on pin priority (11 > 10 > 9)
        if (stateDigitalPin11 == HIGH) {
            selectedCircleIndex = 0; // Top-Left
        } else if (stateDigitalPin10 == HIGH) {
            selectedCircleIndex = 1; // Top-Right
        } else if (stateDigitalPin9 == HIGH) {
            selectedCircleIndex = 2; // Bottom-Left
        } else {
            // Default to the fourth circle if no selection pins are high
            selectedCircleIndex = 3; // Bottom-Right
            Serial.println("No specific circle selection pin (9, 10, 11) is HIGH. Defaulting to Circle 3 (Bottom-Right).");
        }

        Serial.print("digitalPin2 pressed. Starting scan of selected Circle ");
        Serial.print(selectedCircleIndex); Serial.println("...");
        Serial.println("--- Starting scan within the selected CIRCULAR zone ---");

        // Loop through Y coordinates (rows) of the rectangular scanning zone
        for (float y = SCAN_RECT_MIN_Y; y <= SCAN_RECT_MAX_Y + SCAN_STEP_MM_Y/2; y += SCAN_STEP_MM_Y) {

            // Determine scan direction for the current row (zig-zag pattern)
            if (fmod((y - SCAN_RECT_MIN_Y) / SCAN_STEP_MM_Y, 2.0) < 1.0) { // Even row: scan from MIN_X to MAX_X
                for (float x = SCAN_RECT_MIN_X; x <= SCAN_RECT_MAX_X + SCAN_STEP_MM_X/2; x += SCAN_STEP_MM_X) {
                    // Check if the current point (x,y) is inside the selected circle
                    if (isPointInSpecificCircle(x, y, selectedCircleIndex)) {
                        moveTo(x, y);
                        performActionAtLocation();
                    }
                }
            } else { // Odd row: scan from MAX_X to MIN_X (reverse direction)
                for (float x = SCAN_RECT_MAX_X; x >= SCAN_RECT_MIN_X - SCAN_STEP_MM_X/2; x -= SCAN_STEP_MM_X) {
                    // Check if the current point (x,y) is inside the selected circle
                    if (isPointInSpecificCircle(x, y, selectedCircleIndex)) {
                        moveTo(x, y);
                        performActionAtLocation();
                    }
                }
            }
        }
        Serial.print("--- Scan of selected Circle "); Serial.print(selectedCircleIndex); Serial.println(" complete ---\n");

        // --- Return to Home Position (0,0) after each circle scan ---
        // This will now move to the center of the SCAN_RECT.
        Serial.println("Returning to Home Position (0,0), which is the center of the scan rectangle...");
        moveTo(HOME_POS_MM_X, HOME_POS_MM_Y); // Explicitly move back to (0,0)
        Serial.println("Returned to Home Position.\n");

        // After completing scan, wait for button release to prevent re-triggering immediately
        while(digitalRead(digitalPin2) == HIGH);
        delay(5000); // Debounce delay after button release
    }

    // Update previous state for next loop iteration
    previousStateDigitalPin2 = currentStateDigitalPin2;

    delay(10); // Small delay to prevent the loop from running too fast when nothing is happening


}