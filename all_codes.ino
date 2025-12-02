#include <math.h>
#include <limits.h>

// ---------------- PIN DEFINITIONS ----------------
const int digitalPin12 = 12;  // Master "scan enable" switch (unused currently)
const int SPECTRA_TRIGGER_PIN = 7;

// Motor pins
const int X_MOTOR_PIN_A = 6;
const int X_MOTOR_PIN_B = 3;
const int Y_MOTOR_PIN_A = 4;
const int Y_MOTOR_PIN_B = 5;

// ---------------- SCAN PARAMETERS & STATE ----------------
float speedy = 0.53;
float speedx = 0.61215;
float currentPosX_mm = 0.0;
float currentPosY_mm = 0.0;

// Center as globals
float centerX = 0.0;
float centerY = 0.0;

#define SCAN_RECT_SIZE_X 13.05
#define SCAN_RECT_SIZE_Y 12.15
#define SCAN_RECT_MIN_X (-SCAN_RECT_SIZE_X / 2.0)
#define SCAN_RECT_MAX_X (SCAN_RECT_SIZE_X / 2.0)
#define SCAN_RECT_MIN_Y (-SCAN_RECT_SIZE_Y / 2.0)
#define SCAN_RECT_MAX_Y (SCAN_RECT_SIZE_Y / 2.0)

#define CIRCLE_DIAMETER_MM 3.4
#define CIRCLE_RADIUS_MM (CIRCLE_DIAMETER_MM / 2.0)
#define CIRCLE_OFFSET_MM_X 3.2
#define CIRCLE_OFFSET_MM_Y 3.2

struct CircleCenter { float x; float y; };
CircleCenter circleCenters[] = {
  { -CIRCLE_OFFSET_MM_X, +CIRCLE_OFFSET_MM_Y }, // 0: Top-Left
  { +CIRCLE_OFFSET_MM_X, +CIRCLE_OFFSET_MM_Y }, // 1: Top-Right
  { -CIRCLE_OFFSET_MM_X, -CIRCLE_OFFSET_MM_Y }, // 2: Bottom-Left
  { +CIRCLE_OFFSET_MM_X, -CIRCLE_OFFSET_MM_Y }  // 3: Bottom-Right
};
const int NUM_CIRCLES = sizeof(circleCenters) / sizeof(circleCenters[0]);

// ---------------- UTIL ----------------
float clampf(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

// Wait for a valid line of input from Serial and return it as a string
String waitForSerialLine() {
  String input = "";
  while (input.length() == 0) {
    if (Serial.available() > 0) {
      input = Serial.readStringUntil('\n');
      input.trim(); // remove whitespace and newlines
    }
    delay(10); // avoid CPU hog
  }
  return input;
}

// ---------------- MOTOR CONTROL ----------------
void forwardx(float d)  {
  if (d <= 0.0) return;
  digitalWrite(X_MOTOR_PIN_A, HIGH);
  digitalWrite(X_MOTOR_PIN_B, LOW);
  delay((unsigned long)((d / speedx) * 1000.0));
  digitalWrite(X_MOTOR_PIN_A, LOW);
  digitalWrite(X_MOTOR_PIN_B, LOW);
  currentPosX_mm += d;
}

void backwardsx(float d){
  if (d <= 0.0) return;
  digitalWrite(X_MOTOR_PIN_A, LOW);
  digitalWrite(X_MOTOR_PIN_B, HIGH);
  delay((unsigned long)((d / speedx) * 1000.0));
  digitalWrite(X_MOTOR_PIN_A, LOW);
  digitalWrite(X_MOTOR_PIN_B, LOW);
  currentPosX_mm -= d;
}

void forwardy(float d){
  if (d <= 0.0) return;
  digitalWrite(Y_MOTOR_PIN_A, HIGH);
  digitalWrite(Y_MOTOR_PIN_B, LOW);
  delay((unsigned long)((d / speedy) * 1000.0));
  digitalWrite(Y_MOTOR_PIN_A, LOW);
  digitalWrite(Y_MOTOR_PIN_B, LOW);
  currentPosY_mm += d;
}

void backwardsy(float d){
  if (d <= 0.0) return;
  digitalWrite(Y_MOTOR_PIN_A, LOW);
  digitalWrite(Y_MOTOR_PIN_B, HIGH);
  delay((unsigned long)((d / speedy) * 1000.0));
  digitalWrite(Y_MOTOR_PIN_A, LOW);
  digitalWrite(Y_MOTOR_PIN_B, LOW);
  currentPosY_mm -= d;
}

// Move to a target coordinate (clamped)
void moveTo(float targetX_mm, float targetY_mm){
  float clampedX = clampf(targetX_mm, SCAN_RECT_MIN_X, SCAN_RECT_MAX_X);
  float clampedY = clampf(targetY_mm, SCAN_RECT_MIN_Y, SCAN_RECT_MAX_Y);
  if (clampedX != targetX_mm || clampedY != targetY_mm) {
    Serial.println("Warning: target clamped to bounds.");
  }
  float dx = clampedX - currentPosX_mm;
  float dy = clampedY - currentPosY_mm;
  if (fabs(dx) > 0.00001) dx > 0 ? forwardx(dx) : backwardsx(fabs(dx));
  if (fabs(dy) > 0.00001) dy > 0 ? forwardy(dy) : backwardsy(fabs(dy));
}

void performActionAtLocation() {
  digitalWrite(SPECTRA_TRIGGER_PIN, HIGH);
  delay(100);
  digitalWrite(SPECTRA_TRIGGER_PIN, LOW);
  Serial.print("Scan at X=");
  Serial.print(currentPosX_mm, 3);
  Serial.print(" Y=");
  Serial.println(currentPosY_mm, 3);
}

// ---------------- CIRCLE SCAN HELPERS ----------------
bool isPointInCircle(float x,float y,int circleIndex){
  float dx = x - circleCenters[circleIndex].x;
  float dy = y - circleCenters[circleIndex].y;
  return (dx*dx + dy*dy) <= (CIRCLE_RADIUS_MM * CIRCLE_RADIUS_MM);
}

int countPointsForRows(float stepX,int numberRows,float r){
  int total = 0;
  if (numberRows <= 0) return 0;
  float stepY = (2.0 * r) / numberRows;
  for(int row = 0; row <= numberRows; row++){
    float y = -r + row * stepY;
    float span = 2.0 * sqrt(max(0.0, r*r - y*y));
    if(stepX <= 0.0) continue;
    int cols = (int)floor(span / stepX) + 1;
    if(cols < 0) cols = 0;
    total += cols;
  }
  return total;
}

int estimateNumberRows(float stepX,int desiredPoints,float r){
  if(stepX <= 0.0) return 1;
  if(desiredPoints <= 1) return 1;
  float approx = (2.0f * (float)desiredPoints * stepX) / (M_PI * r);
  int guess = max(1, (int)round(approx));
  int bestRows = guess;
  int bestDiff = INT_MAX;
  int searchRange = max(5, guess / 10);
  int start = max(1, guess - searchRange);
  int end = guess + searchRange;
  for(int cand = start; cand <= end; cand++){
    int pts = countPointsForRows(stepX, cand, r);
    int diff = abs(pts - desiredPoints);
    if(diff < bestDiff){
      bestDiff = diff;
      bestRows = cand;
      if(bestDiff == 0) break;
    }
  }
  return bestRows;
}

void scanCircleZigzag(float stepX,int numberRows,int circleIdx){
  if (circleIdx < 0 || circleIdx >= NUM_CIRCLES) {
    Serial.println("Invalid circle index!");
    return;
  }
  float r = CIRCLE_RADIUS_MM;
  float stepY = (2.0 * r) / numberRows;
  CircleCenter c = circleCenters[circleIdx];
  for(int row = 0; row <= numberRows; row++){
    float y = -r + row * stepY;
    float halfSpan = sqrt(max(0.0, r*r - y*y));
    float xMin = -halfSpan;
    float xMax = +halfSpan;
    int numCols = (int)floor((xMax - xMin) / stepX) + 1;
    if(numCols <= 0) continue;
    for(int col = 0; col < numCols; col++){
      float x = (row % 2 == 0) ? (xMin + col * stepX) : (xMax - col * stepX);
      moveTo(c.x + x, c.y + y);
      performActionAtLocation();
    }
  }
}

// ---------------- RECTANGLE SCAN ----------------
void scanRectangle(float stepSize,int totalScans){
  if (stepSize <= 0.0 || totalScans <= 0) {
    Serial.println("Invalid step size or total scans.");
    return;
  }
  int xSteps = (int)ceil(sqrt((float)totalScans));
  int ySteps = (int)ceil((float)totalScans / (float)xSteps);
  float totalX = (xSteps - 1) * stepSize;
  float totalY = (ySteps - 1) * stepSize;
  if (totalX > SCAN_RECT_SIZE_X || totalY > SCAN_RECT_SIZE_Y) {
    Serial.println("Error: Scan exceeds rectangle limits!");
    return;
  }
  float startX = SCAN_RECT_MIN_X;
  float startY = SCAN_RECT_MAX_Y;
  moveTo(startX, startY);
  Serial.print("Starting rectangle scan: ");
  Serial.print(xSteps); Serial.print(" x "); Serial.println(ySteps);
  for (int row = 0; row < ySteps; row++) {
    if (row % 2 == 0) {
      for (int col = 0; col < xSteps; col++) {
        performActionAtLocation();
        delay(8000);
        if (col < xSteps - 1) forwardx(stepSize);
      }
    } else {
      for (int col = 0; col < xSteps; col++) {
        performActionAtLocation();
        delay(8000);
        if (col < xSteps - 1) backwardsx(stepSize);
      }
    }
    if (row < ySteps - 1) backwardsy(stepSize);
  }
  Serial.println("Rectangle scan complete. Returning to center...");
  moveTo(centerX, centerY);
  Serial.println("Returned to center.");
}

// ---------------- NXN SCAN ----------------
void rectangleNXN(int xScans, int yScans) {
  if (xScans <= 0 || yScans <= 0) {
    Serial.println("Invalid x/y scans.");
    return;
  }
  float step = 0.2f;
  float totalX = (xScans - 1) * step;
  float totalY = (yScans - 1) * step;
  if (totalX > SCAN_RECT_SIZE_X || totalY > SCAN_RECT_SIZE_Y) {
    Serial.println("Error: Requested scan exceeds stage rectangle size!");
    return;
  }
  float startX = SCAN_RECT_MIN_X;
  float startY = SCAN_RECT_MAX_Y;
  moveTo(startX, startY);
  Serial.println("Moved to top-left. Starting NXN scan...");
  for (int row = 0; row < yScans; row++) {
    if (row % 2 == 0) {
      for (int col = 0; col < xScans; col++) {
        performActionAtLocation();
        delay(10000);
        if (col < xScans - 1) forwardx(step);
      }
    } else {
      for (int col = 0; col < xScans; col++) {
        performActionAtLocation();
        delay(10000);
        if (col < xScans - 1) backwardsx(step);
      }
    }
    if (row < yScans - 1) backwardsy(step);
  }
  Serial.println("NXN scan complete. Returning to center...");
  moveTo(centerX, centerY);
  Serial.println("Returned to center. NXN scan finished.");
}

// ---------------- MANUAL CONTROL ----------------
void manualWASD(){
  Serial.println("Manual WASD mode: Use w/a/s/d to move stage and scan.");
  while(true){
    if(Serial.available() > 0){
      char key = Serial.read();
      switch(key){
        case 'w': case 'W': forwardy(0.5); break;
        case 's': case 'S': backwardsy(0.5); break;
        case 'a': case 'A': backwardsx(0.5); break;
        case 'd': case 'D': forwardx(0.5); break;
        case 't': case 'T': performActionAtLocation(); break;
        case 'q': case 'Q': return;
        default: Serial.println("Use w/a/s/d to move, t to scan, q to exit."); break;
      }
    }
    delay(10);
  }
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(9600);
  pinMode(SPECTRA_TRIGGER_PIN, OUTPUT);
  pinMode(X_MOTOR_PIN_A, OUTPUT);
  pinMode(X_MOTOR_PIN_B, OUTPUT);
  pinMode(Y_MOTOR_PIN_A, OUTPUT);
  pinMode(Y_MOTOR_PIN_B, OUTPUT);
  digitalWrite(X_MOTOR_PIN_A, LOW);
  digitalWrite(X_MOTOR_PIN_B, LOW);
  digitalWrite(Y_MOTOR_PIN_A, LOW);
  digitalWrite(Y_MOTOR_PIN_B, LOW);

  Serial.println("Stage initialized.");
  Serial.println("Choose mode:");
  Serial.println("1 = Manual WASD");
  Serial.println("2 = Rectangle scan (Automatic NXN grid)");
  Serial.println("3 = Rectangle scan (Manual step & total scans)");
  Serial.println("4 = Circle scan (user step/points)");
  Serial.println("5 = Circle scan automated");
}

// ---------------- LOOP ----------------
void loop(){
  if(Serial.available() > 0){
    int mode = Serial.parseInt();
    switch(mode){
      case 1: manualWASD(); break;
      case 2:{
        Serial.println("Enter X scans: ");
        int x = waitForSerialLine().toInt();
        Serial.println("Enter Y scans: ");
        int y = waitForSerialLine().toInt();
        rectangleNXN(x, y);
        break;
      }
      case 3:{
        Serial.println("Enter step size (mm): ");
        float step = waitForSerialLine().toFloat();
        Serial.println("Enter total scans: ");
        int total = waitForSerialLine().toInt();
        scanRectangle(step, total);
        break;
      }
      case 4:{
        Serial.println("Enter step size (mm): ");
        float step = waitForSerialLine().toFloat();
        Serial.println("Enter total points: ");
        int points = waitForSerialLine().toInt();
        Serial.println("Enter circle index (0-3): ");
        int circleIdx = waitForSerialLine().toInt();
        if(circleIdx < 0 || circleIdx >= NUM_CIRCLES){
          Serial.println("Invalid circle index.");
          break;
        }
        int rows = estimateNumberRows(step, points, CIRCLE_RADIUS_MM);
        scanCircleZigzag(step, rows, circleIdx);
        Serial.println("Circle scan complete! Returning to center...");
        moveTo(centerX, centerY);
        Serial.println("Returned to center.");
        break;
      }
      case 5:{
        Serial.println("Enter circle index (0-3): ");
        int circleIdx = waitForSerialLine().toInt();
        if(circleIdx < 0 || circleIdx >= NUM_CIRCLES){
          Serial.println("Invalid circle index.");
          break;
        }
        float step = 0.2;
        int points = 30;
        int rows = estimateNumberRows(step, points, CIRCLE_RADIUS_MM);
        scanCircleZigzag(step, rows, circleIdx);
        Serial.println("Automated circle scan complete! Returning to center...");
        moveTo(centerX, centerY);
        Serial.println("Returned to center.");
        break;
      }
      default:
        Serial.println("Invalid mode. Enter 1-5.");
        break;
    }
    Serial.println("Choose mode (1-5):");
  }
  delay(10);
}
