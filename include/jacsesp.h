/*
* 
* definiciones utilizadas en el codigo principal
*
*/

#include <FS.h>

#include <ESP8266WiFi.h>

// upload csv file
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
// osc server
#include <WiFiUdp.h>
#include <OSCMessage.h>
#include <OSCBundle.h>
#include <SLIPEncodedSerial.h>

// salida por terminal
// Importante a false para que no envie al mega los mensajes de debug
#define DEBUG false

// leds informacion
#define ROJO D0
#define AZUL D1
#define VERDE D2

char ssid[] = "jacs";            // your network SSID (name)
char pass[] = "perlitalagatita"; // your network password
const char *hostName = "JACSesp";

// A UDP instance to let us send and receive packets over UDP
WiFiUDP Udp;
const IPAddress outIp(192, 168, 1, 33); // remote IP (not needed for receive)
const unsigned int outPort = 8888;      // remote port (not needed for receive)
const unsigned int localPort = 8888;    // local port to listen for UDP packets (here's where we send the packets)

// comunicacion serie con el arduino mega
SLIPEncodedSerial SLIPSerial(Serial);
OSCBundle bundleOUT;
OSCErrorCode error;
// esperamos confirmacion de que el mega puede recibir
bool ack = true;

// web server
ESP8266WebServer server(80);
// a File object
File fsUploadFile;
// file name to store frames, siempre el mismo
String filename = "frames.csv";
String getContentType(String filename); // convert the file extension to the MIME type
bool handleFileRead(String path);       // send the right file to the client (if it exists)
void handleFileUpload();                // upload a new file to the SPIFFS

// number of fields in the csv file
#define FIELDSIZE 11
/* Fields
 * n frame number
 * m motor 
 * s servo
 * l led
 * np neopixel 
 */
// char fields[FIELDSIZE] = {'n', "m1", "m2", "s1", "s2", "l1", "l2", "np1r", "np1g", "np3b"};