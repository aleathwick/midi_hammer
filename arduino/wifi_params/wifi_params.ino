/*
 * based on ESP32 AJAX Demo
 * Updates and Gets data from webpage without page refresh
 * https://circuits4you.com/2018/11/20/web-server-on-esp32-how-to-update-and-display-sensor-values/
 */
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <elapsedMillis.h>
#include "page.h"  //Web page header file

WebServer server(80);

elapsedMillis timer;

//Enter your SSID and PASSWORD
const char* ssid = "_";
const char* password = "_";

//===============================================================
// This routine to execute for various HTTP requests
//===============================================================
void handleRoot() {
 String s = MAIN_page; //Read HTML contents
 server.send(200, "text/html", s); //Send web page
}
 
void handleADC() {
 double a = ((double)timer / 1000);
 String adcValue = String(a);
 
 server.send(200, "text/plane", adcValue); //Send ADC value only to client ajax request
}

//===============================================================
// Setup
//===============================================================

void setup(void){
  Serial.begin(115200);
  Serial.println();
  Serial.println("Booting Sketch...");

/*
//ESP32 As access point
  WiFi.mode(WIFI_AP); //Access Point mode
  WiFi.softAP(ssid, password);
*/
//ESP32 connects to your wifi -----------------------------------
  WiFi.mode(WIFI_STA); //Connectto your wifi
  WiFi.begin(ssid, password);

  Serial.println("Connecting to ");
  Serial.print(ssid);

  //Wait for WiFi to connect
  while(WiFi.waitForConnectResult() != WL_CONNECTED){      
      Serial.print(".");
    }
    
  //If connection successful show IP address in serial monitor
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());  //IP address assigned to your ESP
//----------------------------------------------------------------
 
  server.on("/", handleRoot);      //This is display page
  server.on("/readADC", handleADC);//To get update of ADC Value only
  
  // we also want to be able to receive data from the client, e.g. from forms
  // https://forum.arduino.cc/t/parse-http-get-request-into-something-useful/456626/4
  server.on("/control", [](){
    //server.send(200, "text/plain", "this works as well");
    Serial.println ( server.arg("input1" )  ) ;
    // Serial.println ( server.arg("time" )  ) ;
  });


  server.begin();                  //Start server
  Serial.println("HTTP server started");
}

//===============================================================
// This routine is executed when you open its IP in browser
//===============================================================
void loop(void){
  server.handleClient();
  delay(1);
}