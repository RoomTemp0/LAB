#include <math.h>
#include <limits.h>

/* ---------------- PIN DEFINITIONS ---------------- */
const int SPECTRA_TRIGGER_PIN = 7;
const int X_MOTOR_PIN_A = 6;
const int X_MOTOR_PIN_B = 3;
const int Y_MOTOR_PIN_A = 4;
const int Y_MOTOR_PIN_B = 5;

/* ---------------- PARAMETERS ---------------- */
float speedy = 0.53;
float speedx = 0.61215;
float currentPosX_mm = 0.0;
float currentPosY_mm = 0.0;
float centerX = 0.0;
float centerY = 0.0;

#define SCAN_RECT_SIZE_X 13.05
#define SCAN_RECT_SIZE_Y 12.15
#define SCAN_RECT_MIN_X (-SCAN_RECT_SIZE_X / 2.0)
#define SCAN_RECT_MAX_X ( SCAN_RECT_SIZE_X / 2.0)
#define SCAN_RECT_MIN_Y (-SCAN_RECT_SIZE_Y / 2.0)
#define SCAN_RECT_MAX_Y ( SCAN_RECT_SIZE_Y / 2.0)

#define CIRCLE_DIAMETER_MM 3.4
#define CIRCLE_RADIUS_MM (CIRCLE_DIAMETER_MM / 2.0)
#define CIRCLE_OFFSET_MM_X 3.2
#define CIRCLE_OFFSET_MM_Y 3.2

struct CircleCenter { float x; float y; };
CircleCenter circleCenters[] = {
  {-CIRCLE_OFFSET_MM_X, +CIRCLE_OFFSET_MM_Y},
  {+CIRCLE_OFFSET_MM_X, +CIRCLE_OFFSET_MM_Y},
  {-CIRCLE_OFFSET_MM_X, -CIRCLE_OFFSET_MM_Y},
  {+CIRCLE_OFFSET_MM_X, -CIRCLE_OFFSET_MM_Y}
};
const int NUM_CIRCLES = 4;

/* ---------------- STATE ---------------- */
String serialLine = "";
bool busy = false;

/* ---------------- UTIL ---------------- */
float clampf(float v,float lo,float hi){
  if(v<lo) return lo;
  if(v>hi) return hi;
  return v;
}

/* ---------------- MOTOR CONTROL ---------------- */
void forwardx(float d){
  digitalWrite(X_MOTOR_PIN_A, HIGH);
  digitalWrite(X_MOTOR_PIN_B, LOW);
  delay((unsigned long)((d / speedx) * 1000));
  digitalWrite(X_MOTOR_PIN_A, LOW);
  digitalWrite(X_MOTOR_PIN_B, LOW);
  currentPosX_mm += d;
}

void backwardsx(float d){
  digitalWrite(X_MOTOR_PIN_A, LOW);
  digitalWrite(X_MOTOR_PIN_B, HIGH);
  delay((unsigned long)((d / speedx) * 1000));
  digitalWrite(X_MOTOR_PIN_A, LOW);
  digitalWrite(X_MOTOR_PIN_B, LOW);
  currentPosX_mm -= d;
}

void forwardy(float d){
  digitalWrite(Y_MOTOR_PIN_A, HIGH);
  digitalWrite(Y_MOTOR_PIN_B, LOW);
  delay((unsigned long)((d / speedy) * 1000));
  digitalWrite(Y_MOTOR_PIN_A, LOW);
  digitalWrite(Y_MOTOR_PIN_B, LOW);
  currentPosY_mm += d;
}

void backwardsy(float d){
  digitalWrite(Y_MOTOR_PIN_A, LOW);
  digitalWrite(Y_MOTOR_PIN_B, HIGH);
  delay((unsigned long)((d / speedy) * 1000));
  digitalWrite(Y_MOTOR_PIN_A, LOW);
  digitalWrite(Y_MOTOR_PIN_B, LOW);
  currentPosY_mm -= d;
}

void stopMotors(){
  digitalWrite(X_MOTOR_PIN_A, LOW);
  digitalWrite(X_MOTOR_PIN_B, LOW);
  digitalWrite(Y_MOTOR_PIN_A, LOW);
  digitalWrite(Y_MOTOR_PIN_B, LOW);
}

/* ---------------- MOVE + SCAN ---------------- */
void moveTo(float x,float y){
  x = clampf(x,SCAN_RECT_MIN_X,SCAN_RECT_MAX_X);
  y = clampf(y,SCAN_RECT_MIN_Y,SCAN_RECT_MAX_Y);
  float dx = x - currentPosX_mm;
  float dy = y - currentPosY_mm;
  if(dx>0) forwardx(dx); else backwardsx(-dx);
  if(dy>0) forwardy(dy); else backwardsy(-dy);
}

void performActionAtLocation(){
  digitalWrite(SPECTRA_TRIGGER_PIN, HIGH);
  delay(100);
  digitalWrite(SPECTRA_TRIGGER_PIN, LOW);
  Serial.print("SCAN ");
  Serial.print(currentPosX_mm,3);
  Serial.print(",");
  Serial.println(currentPosY_mm,3);
}

/* ---------------- RECT SCANS ---------------- */
void rectangleNXN(int nx,int ny,float step){
  Serial.println("BUSY");
  for(int y=0;y<ny;y++){
    for(int x=0;x<nx;x++){
      performActionAtLocation();
      if(x<nx-1) (y%2==0)?forwardx(step):backwardsx(step);
    }
    if(y<ny-1) backwardsy(step);
  }
  moveTo(centerX,centerY);
  Serial.println("DONE");
}

void rectangleManual(float step,int total){
  int nx = ceil(sqrt(total));
  int ny = ceil((float)total / nx);
  rectangleNXN(nx,ny,step);
}

/* ---------------- CIRCLE ---------------- */
int estimateRows(float step,int pts){
  return max(1,(int)round((2.0*pts*step)/(M_PI*CIRCLE_RADIUS_MM)));
}

void scanCircle(int idx,float step,int pts){
  Serial.println("BUSY");
  int rows = estimateRows(step,pts);
  float dy = (2*CIRCLE_RADIUS_MM)/rows;
  CircleCenter c = circleCenters[idx];
  for(int r=0;r<=rows;r++){
    float y = -CIRCLE_RADIUS_MM + r*dy;
    float span = sqrt(max(0.0,CIRCLE_RADIUS_MM*CIRCLE_RADIUS_MM-y*y));
    int cols = floor((2*span)/step);
    for(int i=0;i<=cols;i++){
      float x = (r%2==0)?(-span+i*step):(span-i*step);
      moveTo(c.x+x,c.y+y);
      performActionAtLocation();
    }
  }
  moveTo(centerX,centerY);
  Serial.println("DONE");
}

/* ---------------- COMMAND PARSER ---------------- */
void processCommand(String cmd){
  cmd.trim();
  if(cmd.length()==0) return;

  int p1 = cmd.indexOf(',');
  String head = (p1==-1)?cmd:cmd.substring(0,p1);

  if(head=="MANUAL"){
    if(cmd.endsWith("STOP")) stopMotors();
    else if(cmd.endsWith("SCAN")){
      Serial.println("BUSY");
      performActionAtLocation();
      Serial.println("DONE");
    } else {
      char dir = cmd.charAt(p1+1);
      float step = cmd.substring(cmd.lastIndexOf(',')+1).toFloat();
      if(dir=='W') forwardy(step);
      if(dir=='S') backwardsy(step);
      if(dir=='A') backwardsx(step);
      if(dir=='D') forwardx(step);
    }
  }

  else if(head=="RECT" && !busy){
    busy=true;
    int p2=cmd.indexOf(',',p1+1);
    int p3=cmd.indexOf(',',p2+1);
    float a=cmd.substring(p1+1,p2).toFloat();
    float b=cmd.substring(p2+1,p3).toFloat();
    float c=cmd.substring(p3+1).toFloat();
    if(c>0) rectangleNXN((int)a,(int)b,c);
    else rectangleManual(a,(int)b);
    busy=false;
  }

  else if(head=="CIRCLE" && !busy){
    busy=true;
    int p2=cmd.indexOf(',',p1+1);
    int p3=cmd.indexOf(',',p2+1);
    int idx=cmd.substring(p1+1,p2).toInt();
    float step=cmd.substring(p2+1,p3).toFloat();
    int pts=cmd.substring(p3+1).toInt();
    if(step<=0){step=0.2;pts=30;}
    scanCircle(idx,step,pts);
    busy=false;
  }
}

/* ---------------- SETUP / LOOP ---------------- */
void setup(){
  Serial.begin(9600);
  pinMode(SPECTRA_TRIGGER_PIN,OUTPUT);
  pinMode(X_MOTOR_PIN_A,OUTPUT);
  pinMode(X_MOTOR_PIN_B,OUTPUT);
  pinMode(Y_MOTOR_PIN_A,OUTPUT);
  pinMode(Y_MOTOR_PIN_B,OUTPUT);
  Serial.println("READY");
}

void loop(){
  while(Serial.available()){
    char c=Serial.read();
    if(c=='\n'){ processCommand(serialLine); serialLine=""; }
    else serialLine+=c;
  }
}
