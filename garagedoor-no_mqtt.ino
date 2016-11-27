#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#define PIN_RELAY         D1
#define PIN_SENSOR        D2
#define OPENER_EVENT_MS   1000
#define DOOR_CHANGE_TIME  12000
#define CHECK_DOOR_EVERY  500

const char* ssid = ".....";
const char* password = ".....";
const String statusJson1 = "{\"door\":\"";
const String statusJson2 = "\", \"uptime\":\"";
const String statusJson3 = "\"}";
unsigned long openerEvent = 0;
unsigned long doorCheckEvent = 0;
char* doorState = "unknown";
int door = -1;
int doorDelay = 0;

//************************** Just Some basic Definitions used for the Up Time LOgger ************//
long Day = 0;
int  Hour = 0;
int  Minute = 0;
int  Second = 0;
int  SecondStamp = 0;
int  Once = 0;

ESP8266WebServer server(80);

//************************ Uptime Code - Makes a count of the total up time since last start ****************//
//It will work for any main loop's, that loop moret han twice a second: not good for long delays etc
void uptime(){
//** Checks For a Second Change *****//  
  if ( millis() % 1000 <= 500 && Once == 0 ){
    SecondStamp=1;
    Once=1;
  }
  //** Makes Sure Second Count doesnt happen more than once a Second **//
  if( millis() % 1000 > 500 ){
    Once=0;
  }
 
  if( SecondStamp == 1 ) { 
    Second++; 
    SecondStamp = 0; 
    // print_Uptime();
    if ( Second == 60 ){ 
      Minute++; 
      Second=0;                
      if ( Minute == 60 ){
        Minute=0;
        Hour++;
        if (Hour==24){
          Hour=0;
          Day++;
}  }  }  }  }

String padding( int number, byte width ) {
 int currentMax = 10;
 String numberString;
 for (byte i=1; i<width; i++){
   if (number < currentMax) {
     numberString += "0";
   }
   currentMax *= 10;
 }
 numberString += number;
 return numberString;
}

String upTimeString () {
  String formattedUptime = String(padding(Day,4)) + ":" + String(padding(Hour,2)) + ":" + String(padding(Minute,2)) + ":" + String(padding(Second,2));
  return formattedUptime;
}
void handleRoot() {
  server.send(200, "text/plain", "this is a garage door!");
}

void pressGarageButton() {
    digitalWrite(PIN_RELAY, HIGH);
    Serial.println("Pressing Garage Door Button");
    openerEvent = millis();
}

void checkDoor() {
  int doorTemp = digitalRead(PIN_SENSOR);
  if (doorTemp != door)
  {
    door = doorTemp;
    if (door == LOW){ doorState = "closed";}
    else { doorState = "open"; } 
    Serial.println("Door is now " + String(doorState));
  }
  
}

String statusJson(String doorStatus){
  String statusJson = statusJson1 + doorStatus + statusJson2 + upTimeString() + statusJson3;
  return statusJson;
}
void handleStatus() {
  // String statusJson = statusJson1 + doorState + statusJson2 + upTimeString() + statusJson3;
  server.send(200, "application/json", statusJson(doorState));
  Serial.println("Returning Status page");
}


void handleOpen() {
  if ( digitalRead(PIN_SENSOR) != HIGH ) {
    pressGarageButton();
    server.send(200, "application/json", statusJson("opening"));
    door = -1;
    doorState = "opening";
    doorDelay = millis();
  }else{
    server.send(200, "application/json", statusJson("Nothing to do, door already open"));
  }
}

void handleClose() {
  if ( digitalRead(PIN_SENSOR) != LOW ) {
  pressGarageButton();
  server.send(200, "application/json", statusJson("closing"));
  door = -1;
  doorState = "closing";
  doorDelay = millis();
  }else{
    server.send(200, "application/json", statusJson("Nothing to do, door already closed"));
  }
}

void handleNotFound(){
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void setup(void){
  pinMode(PIN_RELAY, OUTPUT);
  pinMode(PIN_SENSOR, INPUT_PULLUP);
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/open", handleOpen);
  server.on("/close", handleClose);
  
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
}

void loop(void){
  server.handleClient();
  uptime();
  if (openerEvent && (millis() - openerEvent >= OPENER_EVENT_MS)) {
    digitalWrite(PIN_RELAY, LOW);
    Serial.println("Releasing Garage Door Button");
    openerEvent = 0;
  }
  if (doorDelay && (millis() - doorDelay >= DOOR_CHANGE_TIME)) {
    checkDoor();
    doorDelay = 0;
  }
  else if ( !doorDelay && (millis() - doorCheckEvent >= CHECK_DOOR_EVERY ) ) {
    checkDoor();
    doorCheckEvent = millis();
  }
}
