#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266SSDP.h>


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
const String hubIP = "192.168.1.127";
unsigned long openerEvent = 0;
unsigned long doorCheckEvent = 0;
char* doorState = "unknown";
int door = -1;
int doorDelay = 0;


// Smartthings hub information
IPAddress hubIp(192,168,1,127); // smartthings hub ip
const unsigned int hubPort = 39500; // smartthings hub port

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
  String addy = server.client().remoteIP().toString();
  if (addy == hubIP) {
    server.send(200, "text/plain", "this is a garage door!");
  } else { freakOut();}
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
    if (door == LOW){ doorState = "closed";sendNotify();}
    else { doorState = "open"; sendNotify();} 
    Serial.println("Door is now " + String(doorState));
  }
  
}

void freakOut() {
  // handleNotFound();
  Serial.println("Access from device other than Hub Detected.  Shutting down");
  ESP.deepSleep(0);
}

String statusJson(String doorStatus){
  String statusJson = statusJson1 + doorStatus + statusJson2 + upTimeString() + statusJson3;
  return statusJson;
}

void sendJSONData(WiFiClient client) {
  String jsonbody = statusJson(doorState);
  Serial.println("sendJSONData");
  client.println(F("CONTENT-TYPE: application/json"));
  client.print(F("CONTENT-LENGTH: "));
  client.print(jsonbody.length());
  client.println();
  client.print(jsonbody);
  client.println();
  
}

// send data
int sendNotify() //client function to send/receieve POST data.
{
  WiFiClient client;
  int returnStatus = 1;
  if (client.connect(hubIp, hubPort)) {
    client.println(F("POST / HTTP/1.1"));
    client.print(F("HOST: "));
    client.print(hubIp);
    client.print(F(":"));
    client.println(hubPort);
    sendJSONData(client);
  }
  else {
    //connection failed
    Serial.println("failed to connect when sending JSON notify");
    returnStatus = 0;
}
}
void handleStatus() {
  String addy = server.client().remoteIP().toString();
  if (addy == hubIP) {
    server.send(200, "application/json", statusJson(doorState));
    Serial.println("Returning Status page");
  } else { freakOut();}
}


void handleOpen() {
  String addy = server.client().remoteIP().toString();
  if (addy == hubIP) {
    if ( digitalRead(PIN_SENSOR) != HIGH ) {
      pressGarageButton();
      server.send(200, "application/json", statusJson("opening"));
      door = -1;
      doorState = "opening";
      doorDelay = millis();
    }else{
      server.send(200, "application/json", statusJson("Nothing to do, door already open"));
        Serial.println("Nothing to do, door already open");
    }
  } else { freakOut();}
}

void handleClose() {
  String addy = server.client().remoteIP().toString();
  if (addy == hubIP) {
    if ( digitalRead(PIN_SENSOR) != LOW ) {
    pressGarageButton();
    server.send(200, "application/json", statusJson("closing"));
    door = -1;
    doorState = "closing";
    doorDelay = millis();
    }else{
      server.send(200, "application/json", statusJson("Nothing to do, door already closed"));
      Serial.println("Nothing to do, door already closed");
    }
  } else { freakOut();}
}

void handleReboot() {
 // ESP.restart();
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
  server.on("/on", handleOpen);
  server.on("/close", handleClose);
  server.on("/off", handleClose);
  server.on("/reboot", handleReboot);
  server.on("/description.xml", HTTP_GET, [](){
      SSDP.schema(server.client());
    });
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");

  Serial.printf("Starting SSDP...\n");
    SSDP.setSchemaURL("description.xml");
    SSDP.setHTTPPort(80);
    SSDP.setName("ESP8266 Garage Door Opener");
    SSDP.setSerialNumber("000008675309");
    SSDP.setURL("/");
    SSDP.setModelName("Sonoff Wifi Switch");
    SSDP.setModelNumber("000000000001");
    SSDP.setModelURL("http://www.meethue.com");
    SSDP.setManufacturer("Me!");
    SSDP.setManufacturerURL("http://www.reddit.com/r/the_donald");
SSDP.begin();
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
