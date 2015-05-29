#include <I2C.h>
#include <RunningMedian.h>
#include <Servo.h>
#include <LSM303.h>
#include <Wire.h>

#define    LIDARLite_ADDRESS   0x62          // Default I2C Address of LIDAR-Lite.
#define    RegisterMeasure     0x00          // Register to write to initiate ranging.
#define    MeasureValue        0x04          // Value to initiate ranging.
#define    RegisterHighLowB    0x8f          // Register to get both High and Low bytes in 1 call.

#define encoderPin  2                        // Wheel encoder pin

LSM303 compass;

Servo myServo;

int servoL = 135;
int servoC = 90;
int servoR = 45;
int servoDelay = 150;
int servoPin = 10;

int fw = 3;
int rv = 5;
int lf = 6;
int rt = 9;

int driveSpeed = 110;
int turnSpeed = 255;
float wiggle = 2.5;

int turnDelay = 350;

float stopDist = 85.0;

float targetHeading;
long distTraveled;
int legHeading [4] = {0.0, 90.0, 180.0, 270.0};
int legDist [4] = {10, 20, 30, 40};

bool moving, frontClear, rightClear, leftClear;

RunningMedian samples = RunningMedian(30);

void setup() {
  Serial.begin(9600);
  distTraveled = 0;

  //compass setup
  Wire.begin();
  compass.init();
  compass.enableDefault();
  compass.m_min = (LSM303::vector<int16_t>) {
    -42, -700, -432
  };
  compass.m_max = (LSM303::vector<int16_t>) {
    +971, +238, +464
  };
  //Lidar Lite setup
  I2c.begin(); // Opens & joins the irc bus as master
  delay(100); // Waits to make sure everything is powered up before sending or receiving data
  I2c.timeOut(50); // Sets a timeout to ensure no locking up of sketch if I2C communication fails

  myServo.attach(servoPin);
  myServo.write(servoC);

  //Setup wheel encoder
  pinMode(encoderPin, INPUT);
  attachInterrupt(0, encoderTick, HIGH);
  moving = false;
}

void loop() {
  drive();
}

float measure() {
  samples.clear();
  // Write 0x04 to register 0x00
  uint8_t nackack = 100; // Setup variable to hold ACK/NACK resopnses
  while (nackack != 0)  // While NACK keep going (i.e. continue polling until sucess message (ACK) is received )
  {
    nackack = I2c.write(LIDARLite_ADDRESS, RegisterMeasure, MeasureValue); // Write 0x04 to 0x00
    delay(1); // Wait 1 ms to prevent overpolling
  }
  for (int i = 0; i < 30; i++)
  {

    byte distanceArray[2]; // array to store distance bytes from read function

    // Read 2byte distance from register 0x8f
    nackack = 100; // Setup variable to hold ACK/NACK resopnses

    // Read 2byte distance from register 0x8f
    nackack = 100; // Setup variable to hold ACK/NACK resopnses
    while (nackack != 0)  // While NACK keep going (i.e. continue polling until sucess message (ACK) is received )
    {
      nackack = I2c.read(LIDARLite_ADDRESS, RegisterHighLowB, 2, distanceArray); // Read 2 Bytes from LIDAR-Lite Address and store in array
      delay(1); // Wait 1 ms to prevent overpolling
    }
    int distance = (distanceArray[0] << 8) + distanceArray[1];  // Shift high byte [0] 8 to the left and add low byte [1] to create 16-bit int
    samples.add(distance);
  }
  return samples.getMedian();
}

int checkCompass() {
  compass.read();
  return compass.heading((LSM303::vector<int>) {
    1, 0, 0
  });
}

void navigate() {
  if (distTraveled < legDist[0]) {
    targetHeading = legHeading[0];
  }
  if (distTraveled > legDist[0] && distTraveled < legDist[1]) {
    targetHeading = legHeading[1];
  }
  if (distTraveled > legDist[1] && distTraveled < legDist[2]) {
    targetHeading = legHeading[2];
  }
  if (distTraveled > legDist[2]){
    targetHeading = legHeading[3];
  }

  float currentHeading = checkCompass();

  if (currentHeading < targetHeading - wiggle) {
    while (checkCompass() < targetHeading - wiggle) {
      analogWrite(rt, turnSpeed);
    }
    analogWrite(rt, 0);
  }
  if (currentHeading > targetHeading + wiggle) {
    while (checkCompass() > targetHeading + wiggle) {
      analogWrite(lf, turnSpeed);
    }
    analogWrite(lf, 0);
  }
}

void sweep() {
  myServo.write(servoC);
  delay(servoDelay);
  frontClear = measure() > stopDist;

  myServo.write(servoR);
  delay(servoDelay);
  rightClear = measure() > stopDist;

  myServo.write(servoL);
  delay(servoDelay);
  leftClear = measure() > stopDist;

  myServo.write(servoC);
  delay(servoDelay);
}

void drive() {
  myServo.write(servoC);
  delay(servoDelay);
  frontClear = measure() > stopDist;

  if (frontClear) {
    forward();
    navigate();
  } else {
    stopAll();
    sweep();
    if (rightClear) {
      forward();
      rightTurn();
    } else {
      if (leftClear) {
        forward();
        leftTurn();
      } else {
        reverse();
        stopAll();
      }
    }
  }
}

void encoderTick() {
  distTraveled ++;
}
void forward()
{
  if (!moving) {
    analogWrite(rv, 0);
    analogWrite(fw, driveSpeed);
    moving = true;
  }
}

void reverse()
{
  analogWrite(fw, 0);
  analogWrite(rv, 200);
  delay(500);
  analogWrite(rv, 0);

}
void stopAll()
{
  analogWrite(fw, 0);
  analogWrite(rv, 0);
  analogWrite(lf, 0);
  analogWrite(rt, 0);
  moving = false;
}

void leftTurn()
{
  analogWrite(lf, turnSpeed);
  delay(turnDelay);
  analogWrite(lf, 0);
}
void rightTurn()
{
  analogWrite(rt, turnSpeed);
  delay(turnDelay);
  analogWrite(rt, 0);
}
