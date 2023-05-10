//Libraries
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <WebSocketsServer.h>
//#include <ESP8266WebServer.h>
#include <Servo.h>
#include <LiquidCrystal_I2C.h>
#include "Cars.h"
#include "UltrasonicSensors.h"

//NodeMCU GPIO
#define D0 16
#define D1 5
#define D2 4
#define D3 0
#define D4 2
#define D5 14
#define D6 12
#define D7 13
#define D8 15

//Web stuffs
const char* ssid = "";
const char* pass = "";
AsyncWebServer server(80); //

//Object declarations
Servo myServo;
PCF8574 pinExtender(0x20);  //set the PCF8574 address to 0x20
LiquidCrystal_I2C myLCD(0x27, 16, 2);  //set the LCD address to 0x27 for a 16 chars and 2 line display
Cars myCar(pinExtender, P4, P3, P5, P6, D4, D5); 
UltrasonicSensors sensorRight(pinExtender, D6, P0);
UltrasonicSensors sensorMid(pinExtender, D7, P1);
UltrasonicSensors sensorLeft(pinExtender, D8, P2);

//Initialitation
void _init() {
  //initialize Serial
  Serial.begin(115200);

  //initialize PCF8574
  pinExtender.begin();

  //initialize Servo
  myServo.attach(D3);

  //initialize LCD
  myLCD.init();
  myLCD.clear();
  myLCD.backlight();
  testLCD();
}

void setup() {
  _init();
  myCar.setSpeed(100);
}

void loop() {
  test_run();
}

void test_run() {
  uint8_t distance = sensorRight.getDistanceFromMicro();
  delay(5);
  uint8_t distance2 = sensorMid.getDistanceFromMicro();
  delay(5);
  uint8_t distance3 = sensorLeft.getDistanceFromMicro();
  delay(5);

  char* status1 = sensorRight.getCurrentStatus();
  char* status2 = sensorMid.getCurrentStatus();
  char* status3 = sensorLeft.getCurrentStatus();

  if (status1 == "very close") {
    myCar.stopMovement();
    delay(200);
    return;
  }

  if (status2 == "very close") {
    myCar.stopMovement();
    delay(200);
    return;
  }

  if (status3 == "very close") {
    myCar.stopMovement();
    delay(200);
    return;
  }

  myCar.moveForward();

  Serial.print(status1);
  Serial.print(" |");
  Serial.print(distance);
  Serial.print(" cm  |");
  Serial.print(status2);
  Serial.print(" |");
  Serial.print(distance2);
  Serial.print(" cm  |");
  Serial.print(status3);
  Serial.print(" |");
  Serial.print(distance3);
  Serial.print(" cm  |");
  Serial.println();
}

void testServo() {
  int pos;

  for (pos = 0; pos <= 180; pos += 1) {  // goes from 0 degrees to 180 degrees
    // in steps of 1 degree
    myServo.write(pos);  // tell myServo to go to position in variable 'pos'
    delay(15);           // waits 15ms for the myServo to reach the position
  }
  pinExtender.digitalWrite(P7, HIGH);
  for (pos = 180; pos >= 0; pos -= 1) {  // goes from 180 degrees to 0 degrees
    myServo.write(pos);                  // tell myServo to go to position in variable 'pos'
    delay(15);                           // waits 15ms for the myServo to reach the position
  }
  pinExtender.digitalWrite(P7, LOW);
}

void testLCD() {
  // Print a message on both lines of the myLCD.
  myLCD.setCursor(2, 0);  //Set cursor to character 2 on line 0
  myLCD.print("Hello world!");

  myLCD.setCursor(2, 1);  //Move cursor to character 2 on line 1
  myLCD.print("myLCD Tutorial");
}
