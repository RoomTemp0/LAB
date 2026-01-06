#include <math.h>

// ---------------- PIN DEFINITIONS ----------------
const int SPECTRA_TRIGGER_PIN = 7;

// Motor pins
const int X_MOTOR_PIN_A = 6;
const int X_MOTOR_PIN_B = 3;
const int Y_MOTOR_PIN_A = 4;
const int Y_MOTOR_PIN_B = 5;

// ---------------- MOTION PARAMETERS ----------------
float speedX = 0.61215;   // mm/sec
float speedY = 0.53;

float currentX = 0.0;
float currentY = 0.0;

bool End == false;
// ---------------- SCAN GEOMETRY ----------------
#define RECT_X 13.05
#define RECT_Y 12.15

#define CIRCLE_DIAMETER 3.4
#define CIRCLE_RADIUS (CIRCLE_DIAMETER / 2.0)

#define CIRCLE_OFFSET_X 3.2
#define CIRCLE_OFFSET_Y 3.2

struct CircleCenter {
  float x;
  float y;
};

CircleCenter circles[4] = {
  {-CIRCLE_OFFSET_X, +CIRCLE_OFFSET_Y},
  {+CIRCLE_OFFSET_X, +CIRCLE_OFFSET_Y},
  {-CIRCLE_OFFSET_X, -CIRCLE_OFFSET_Y},
  {+CIRCLE_OFFSET_X, -CIRCLE_OFFSET_Y}
};

// ---------------- STATE ----------------
bool busy = false;

// ---------------- LOW LEVEL MOTION ----------------
void stopMotors() {
  digitalWrite(X_MOTOR_PIN_A, LOW);
  digitalWrite(X_MOTOR_PIN_B, LOW);
  digitalWrite(Y_MOTOR_PIN_A, LOW);
  digitalWrite(Y_MOTOR_PIN_B, LOW);
}

void moveX(float d) {
  if (d == 0) return;
  digitalWrite(X_MOTOR_PIN_A, d > 0);
  digitalWrite(X_MOTOR_PIN_B, d < 0);
  delay(abs(d / speedX) * 1000);
  stopMotors();
  currentX += d;
}

void moveY(float d) {
  if (d == 0) return;
  digitalWrite(Y_MOTOR_PIN_A, d > 0);
  digitalWrite(Y_MOTOR_PIN_B, d < 0);
  delay(abs(d / speedY) * 1000);
  stopMotors();
  currentY += d;
}

void moveTo(float x, float y) {
  moveX(x - currentX);
  moveY(y - currentY);
}

// ---------------- SCAN ACTION ----------------
void triggerScan() {
  digitalWrite(SPECTRA_TRIGGER_PIN, HIGH);
  delay(100); // spectra integration time
  digitalWrite(SPECTRA_TRIGGER_PIN, LOW);
  delay(12000); // for the spectra machine to analyze the scan
  Serial.print("SCAN at X="); Serial.print(currentX,3);
  Serial.print(" Y="); Serial.println(currentY,3);
}

// ---------------- RECTANGLE NXN ----------------
void rectangleScan(int nx, int ny, float step) {
  Serial.println("BUSY");
  busy = true;

  // Clamp scan to physical rectangle
  float maxStepX = floor(RECT_X / step);
  float maxStepY = floor(RECT_Y / step);
  if (nx > maxStepX) nx = maxStepX;
  if (ny > maxStepY) ny = maxStepY;
  Serial.print("Adjusted to fit within the square");
  Serial.println(maxStepX);
  Serial.println(maxStepY);

  moveTo(0, 0);
  float startX = -((nx - 1) * step) / 2.0;
  float startY =  ((ny - 1) * step) / 2.0;
  moveTo(startX, startY);

  for (int y = 0; y < ny; y++) {
    for (int x = 0; x < nx; x++) {
      triggerScan();
      if (x < nx - 1)
        moveX((y % 2 == 0) ? step : -step);
    }
    if (y < ny - 1) moveY(-step);
  }

  moveTo(0, 0);
  Serial.println("DONE");
  busy = false;
}

// ---------------- RECTANGLE MANUAL ----------------
void rectangleScanManualEven(int totalScans, float step) {
  Serial.println("BUSY");
  busy = true;

  //defining center 
  currentX = 0.0;
  currentY = 0.0;
  moveTo(0, 0);

  // Compute max possible points along X and Y
  int maxXPoints = floor(RECT_X / step) + 1;
  int maxYPoints = floor(RECT_Y / step) + 1;

  // Clamp totalScans to physically possible points
  if (totalScans > maxXPoints * maxYPoints) {
    totalScans = maxXPoints * maxYPoints;
    Serial.print("Adjusted total scans to fit rectangle: ");
    Serial.println(totalScans);
  }

  // Determine how many rows and columns to evenly distribute scans
  int cols = ceil(sqrt((float)totalScans * RECT_X / RECT_Y));
  int rows = ceil((float)totalScans / cols);

  if (cols > maxXPoints) cols = maxXPoints;
  if (rows > maxYPoints) rows = maxYPoints;

  // Compute spacing along X and Y based on number of scans
  float spacingX = (cols > 1) ? RECT_X / (cols - 1) : 0;
  float spacingY = (rows > 1) ? RECT_Y / (rows - 1) : 0;

  float startX = -RECT_X / 2.0;
  float startY = RECT_Y / 2.0;

  int scansDone = 0;
  for (int r = 0; r < rows && scansDone < totalScans; r++) {
    for (int c = 0; c < cols && scansDone < totalScans; c++) {
      float x = startX + c * spacingX;
      float y = startY - r * spacingY;
      moveTo(x, y);
      triggerScan();
      scansDone++;
    }
  }


  moveTo(0, 0);
  Serial.println("DONE");
  busy = false;
}


// ---------------- CIRCLE SCAN ----------------
void circleScan(int idx, float step, int points) {
  Serial.println("BUSY");
  busy = true;

  if (idx < 0 || idx > 3) {
    Serial.println("Invalid circle index!");
    busy = false;
    return;
  }

  moveTo(0, 0);
  moveTo(circles[idx].x, circles[idx].y);

  int rows = max(1, points / 5); // simple row estimate
  float dy = (2 * CIRCLE_RADIUS) / rows;

  for (int r = 0; r <= rows; r++) {
    float y = -CIRCLE_RADIUS + r * dy;
    float span = sqrt(max(0.0, CIRCLE_RADIUS*CIRCLE_RADIUS - y*y));
    int cols = floor(span / step);

    for (int c = 0; c <= cols; c++) {
      float x = (r % 2 == 0) ? (-span + c * step) : (span - c * step);
      moveTo(circles[idx].x + x, circles[idx].y + y);
      triggerScan();
    }
  }

  moveTo(0, 0);
  Serial.println("DONE");
  busy = false;
}

// ---------------- MANUAL WASD ----------------
void manualMove(char dir, float step) {
  if (dir == 'W') moveY(step);
  if (dir == 'S') moveY(-step);
  if (dir == 'A') moveX(-step);
  if (dir == 'D') moveX(step);
}

// ---------------- SERIAL PARSER ----------------
void handleCommand(String cmd) {
  if (busy && !cmd.startsWith("MANUAL")) return;

  if (cmd.startsWith("MANUAL")) {
    if (cmd.indexOf("CENTER") > 0) {
        // Set current position as new center
        currentX = 0.0;
        currentY = 0.0;
        Serial.println("Center updated to current position");
        return;
    }

    char dir = cmd.charAt(7);
    float step = cmd.substring(cmd.lastIndexOf(',') + 1).toFloat();

    if (cmd.indexOf("STOP") > 0) {
        stopMotors();
        return;
    }
    if (cmd.indexOf("SCAN") > 0) {
        triggerScan();
        return;
    }

    if (dir == 'W') moveY(step);
    if (dir == 'S') moveY(-step);
    if (dir == 'A') moveX(-step);
    if (dir == 'D') moveX(step);
}


  else if (cmd.startsWith("RECT_NXN")) {
    int p1 = cmd.indexOf(',') + 1;
    int p2 = cmd.indexOf(',', p1);
    int p3 = cmd.indexOf(',', p2 + 1);
    int x = cmd.substring(p1, p2).toInt();
    int y = cmd.substring(p2 + 1, p3).toInt();
    float step = cmd.substring(p3 + 1).toFloat();
    rectangleScan(x, y, step);
  }

  else if (cmd.startsWith("RECT_MANUAL")) {
    int firstComma = cmd.indexOf(',');
    int secondComma = cmd.indexOf(',', firstComma + 1);

    int total = cmd.substring(firstComma + 1, secondComma).toInt();
    float step = cmd.substring(secondComma + 1).toFloat();

    Serial.print("RECT_MANUAL received: totalScans=");
    Serial.print(total);
    Serial.print(" step=");
    Serial.println(step);

    rectangleScanManualEven(total, step);
}


  else if (cmd.startsWith("CIRCLE")) {
    int p1 = cmd.indexOf(',') + 1;
    int p2 = cmd.indexOf(',', p1);
    int p3 = cmd.indexOf(',', p2 + 1);
    int idx = cmd.substring(p1, p2).toInt();
    float step = cmd.substring(p2 + 1, p3).toFloat();
    int pts = cmd.substring(p3 + 1).toInt();
    circleScan(idx, step, pts);
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

  stopMotors();
  Serial.println("READY");
}

// ---------------- LOOP ----------------
void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length()) handleCommand(cmd);
  }
}
