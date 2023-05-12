//Libraries
#include <algorithm>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WebSocketsServer.h>
#include <Servo.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include "Cars.h"
#include "UltrasonicSensors.h"
#include "index_html.h"

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
const char* ssid = "Tiara1";
const char* pass = "0320281926";
// const char* ssid = "Teter";
// const char* pass = "qwe123456";
AsyncWebServer server(80); 
WebSocketsServer webSocket = WebSocketsServer(81);

//Object declarations
Servo myServo;
PCF8574 pinExtender(0x20);  //set the PCF8574 address to 0x20
LiquidCrystal_I2C myLCD(0x27, 16, 2);  //set the LCD address to 0x27 for a 16 chars and 2 line display
Cars myCar(pinExtender, P3, P4, P5, P6, D4, D5); 
UltrasonicSensors sensorRight(pinExtender, D6, P0);
UltrasonicSensors sensorMid(pinExtender, D7, P1);
UltrasonicSensors sensorLeft(pinExtender, D8, P2);

//Global variables
uint8_t currentServoAngle = 90;
String currentLCDtext[2] ={
  "",
  ""
};
bool disableForward = false, disableRotateRight = false, disableRotateLeft = false, disableTurnRight = false, disableTurnLeft = false;
unsigned long lastTime = 0;


//Initialitation
void _init() {
  //initialize Serial
  Serial.begin(115200);

  //initialize PCF8574
  pinExtender.begin();

  //initialize Servo
  myServo.attach(D3);
  myServo.write(currentServoAngle);

  //initialize LCD
  myLCD.init();
  myLCD.clear();
  myLCD.backlight();

  //Connectiong to WiFi
  connectToWiFi();

  initServer();
}

void setup() {
  _init();
  myCar.setSpeed(255);
  pinExtender.pinMode(P7,OUTPUT);
}

void onWebSocketEvent(uint8_t clientNum, WStype_t type, uint8_t *payload, size_t length){
  switch (type){
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", clientNum);
    break;
    case WStype_CONNECTED:
      Serial.printf("[%u] Connected from ip %u.%u.%u.%u\n", clientNum, 
                     webSocket.remoteIP(clientNum)[0], webSocket.remoteIP(clientNum)[1],
                     webSocket.remoteIP(clientNum)[2], webSocket.remoteIP(clientNum)[3]);
    break;
    case WStype_TEXT: {
      Serial.printf("[%u] Received text message: %s\n", clientNum, payload);
      // parse incoming JSON data
      StaticJsonDocument<64> doc;
      DeserializationError error = deserializeJson(doc, payload);
      if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.print(error.c_str());
        Serial.println(" (input is not of Json type)");
        if(strstr((char *)payload, ",") != NULL){
        char *ptr = strtok((char*)payload, ",");
        String message = String(ptr);
        ptr = strtok(NULL, ",");
        uint8_t value = atoi(ptr);
          if ( strcmp(message.c_str(), "setSpeed") == 0 )
            myCar.setSpeed(value);
        }
        if ( strcmp((char *)payload, "rotateServoToRight") == 0 )
          turnServoMotor(-45);
        if ( strcmp((char *)payload, "rotateServoToLeft") == 0 )
          turnServoMotor(45);
        if ( strcmp((char *)payload, "moveForward") == 0 ){
          if(!disableForward)
            myCar.moveForward();
          }
        if ( strcmp((char *)payload, "moveBackward") == 0 ){
          myCar.moveBackward();
          }
        if ( strcmp((char *)payload, "moveForwardLeft") == 0 ){
          if(!disableTurnLeft)
            myCar.turnLeft();
          }
        if ( strcmp((char *)payload, "moveForwardRight") == 0 ){
          if(!disableTurnRight)
            myCar.turnRight();
          }
        if ( strcmp((char *)payload, "moveBackwardRight") == 0 ){
          if(!disableTurnRight)
          myCar.backLeft();
          }
        if ( strcmp((char *)payload, "moveBackwardLeft") == 0 ){
          if(!disableTurnLeft)
          myCar.backRight();
          }
        if ( strcmp((char *)payload, "rotateRight") == 0 ){
          if(!disableRotateRight)
          myCar.rotateRight();
          }
        if ( strcmp((char *)payload, "rotateLeft") == 0 ){
          if(!disableRotateLeft)
          myCar.rotateLeft();
          }
        if ( strcmp((char *)payload, "stopMove") == 0 )
          myCar.stopMovement();
        return;
      }
      currentLCDtext[0] = doc["row1"].as<String>();
      currentLCDtext[1] = doc["row2"].as<String>();

      myLCD.clear();
      myLCD.setCursor((16 - currentLCDtext[0].length())/2,0);
      myLCD.print(currentLCDtext[0]);
      myLCD.setCursor((16 - currentLCDtext[1].length())/2, 1);
      myLCD.print(currentLCDtext[1]);
      }break;
    default:
      break;
  }
}

void loop() {
  webSocket.loop();

  //Get readings from ultrasonic sensors
  uint8_t rightSensorDistance = sensorRight.getDistanceFromMicro();
  delay(5);
  uint8_t midSensorDistance = sensorMid.getDistanceFromMicro();
  delay(5);
  uint8_t leftSensorDistance = sensorLeft.getDistanceFromMicro();
  delay(5);

  char* rightStatus = sensorRight.getCurrentStatus();
  char* midStatus = sensorMid.getCurrentStatus();
  char* leftStatus = sensorLeft.getCurrentStatus();

  updateAvailableMove(rightStatus, midStatus, leftStatus);

  //////////////////////////////////////////
   ///////////////////////////////////////
      /////////////////////////////////

  ///
  /// Make a Json documents that hold statuses
  ///     and send it through websocket
  StaticJsonDocument<400> doc;

  doc["right_sensor_status"] = rightStatus;
  doc["middle_sensor_status"] = midStatus;
  doc["left_sensor_status"] = leftStatus;
  doc["right_sensor_distance"] = rightSensorDistance;
  doc["middle_sensor_distance"] = midSensorDistance;
  doc["left_sensor_distance"] = leftSensorDistance;
  doc["current_LCD_text_row1"] = currentLCDtext[0];
  doc["current_LCD_text_row2"] = currentLCDtext[1];
  doc["photoResistor_read"] = photoresistorRead();
  doc["current_servo_angle"] = currentServoAngle - 90;

  String jsonString;
  serializeJson(doc, jsonString);
  webSocket.broadcastTXT(jsonString);

  //////////////////////////////////////////
   ///////////////////////////////////////
      /////////////////////////////////
}

void connectToWiFi(){
  WiFi.begin(ssid, pass);
  
  Serial.print("Connecting to ");
  Serial.println(ssid);
  while (WiFi.status() != WL_CONNECTED){
    myLCD.clear();
    myLCD.setCursor(0,0);
    myLCD.print("Connecting");
    myLCD.setCursor(9,0);
    myLCD.print(".");
    Serial.print(".");
    delay(500);
    myLCD.print(".");
    Serial.print(".");
    delay(500);
    myLCD.print(".");
    Serial.print(".");
    delay(500);
  }
  Serial.print("\r\nConnected. IP: ");
  Serial.println(WiFi.localIP());
  myLCD.setCursor(0,0);
  myLCD.print("Connected at ");
  myLCD.setCursor(0,1);
  myLCD.print(WiFi.localIP());
  delay(2000);
  myLCD.clear();
}

void initServer(){
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send_P(200, "text/html", index_html);
    });

    server.begin();

    webSocket.begin();
    webSocket.onEvent(onWebSocketEvent);
}

void turnServoMotor(int8_t rotateValue){
  if (rotateValue > 180 || rotateValue < -180)
  return;
  currentServoAngle = std::clamp(currentServoAngle + rotateValue, 0, 180);

  myServo.write(currentServoAngle);

  Serial.printf("Rotating servo motor by %d and now is at %u\n", rotateValue, currentServoAngle);
}

void updateAvailableMove(char* rightStatus, char* midStatus, char* leftStatus){
  // bool disableForward = false, disableRotateRight = false, disableRotateLeft = false, disableTurnRight = false, disableTurnLeft = false;
  if (rightStatus == "very close") {
    disableForward = true;
    disableTurnRight = true;
    disableRotateRight = true;
  }

  if (midStatus == "very close") {
    disableForward = true;
    disableTurnLeft = true;
    disableTurnRight = true;
  }

  if (leftStatus == "very close") {
    disableForward = true;
    disableTurnLeft = true;
    disableRotateLeft = true;
  }

  if(leftStatus != "very close" && midStatus != "very close"){
    disableTurnLeft = false;
  }

  if(rightStatus != "very close" && midStatus != "very close"){
    disableTurnLeft = false;
  }

  if (leftStatus != "very close"){
    disableRotateLeft = false;
  }

  if (rightStatus != "very close"){
    disableRotateRight = false;
  }

  if (midStatus != "very close"){
    disableForward = false;
  }
}

// void stopCarIfUltrasonicSensorCaughtSomething(char* sensorStatus){
//   if(myCar.currentState != "Parking")
// }

String photoresistorRead(){
  int sensorValue = analogRead(A0);   // read the input on analog pin 0

	float voltage = sensorValue * (5.0 / 1023.0);   // Convert the analog reading (which goes from 0 - 1023) to a voltage (0 - 5V)

  Serial.println(voltage);

  if (voltage < 2.0) {
    return "Bright";
  } else {
    return "Dark";
  }
}

/////                                 /////
/////   CODE FOR TESTING COMPONENTS   /////
/////                                 /////

void test_run() {
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

  // myCar.moveForward();
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
