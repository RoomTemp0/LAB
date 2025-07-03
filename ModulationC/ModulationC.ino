#include <math.h>

float speed = .53; // mm/s


void setup() {
  Serial.begin(9600);
  pinMode(2, OUTPUT); // Pin 2 for motor 1 control
  pinMode(3, OUTPUT); // Pin 3 for motor 1 control
  pinMode(4, OUTPUT); // Pin 4 for motor 2 control
  pinMode(5, OUTPUT); // Pin 5 for motor 2 control

  // Initialize motor control pins to LOW to ensure motors are off at start
  digitalWrite(2, LOW); // IN1
  digitalWrite(3, LOW); // IN2
  digitalWrite(4, LOW); // IN3
  digitalWrite(5, LOW); // IN4
  // set up motor pins if needed
  Serial.println("Move motors to specificed starting position, depending on the graph");
}

void forwardx(float d)  { // d is for distance that is in mm 
  float time = (d/speed) * 1000;
  digitalWrite(2, HIGH);
  digitalWrite(3, LOW);
  delay(time);//time is also supposed to be in ms requiring the * 1000 in the equation above
  digitalWrite(2, LOW);
  digitalWrite(3, LOW);
}

void backwardsx(float d) {
  float time = (d/speed) * 1000;
  digitalWrite(2, LOW);
  digitalWrite(3, HIGH);
  delay(time);
  digitalWrite(2, LOW);
  digitalWrite(3, LOW);
}

void forwardy(float d) {
  float time = (d/speed) * 1000;
  digitalWrite(4, HIGH);
  digitalWrite(5, LOW);
  delay(time);
  digitalWrite(4, LOW);
  digitalWrite(5, LOW);
}

void backwardsy(float d) {
  float time = (d/speed) * 1000;
  digitalWrite(4, LOW);
  digitalWrite(5, HIGH);
  delay(time);
  digitalWrite(4, LOW);
  digitalWrite(5, LOW);
}


void loop() {
  // put your main code here, to run repeatedly:
  if (Serial.available()) {
    char key = Serial.read();

    switch (key) {
      case 'w':
      case 'W':
        forwardy(.5);
        break;
      case 's':
      case 'S':
        backwardsy(.5);
        break;
      case 'a':
      case 'A':
        backwardsx(.5);
        break;
      case 'd':
      case 'D':
        forwardx(.5);
        break;
      default:
        Serial.println("Use: 'w', 'a', 's', or 'd'");
        break;
    }
  }
}
