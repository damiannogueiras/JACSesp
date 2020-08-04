/*---------------------------------------------------------------------------------------------
  Open Sound Control (OSC) server for the ESP8266/ESP32
  Recibe mensages OSC y los manda al MEGA2560 para que actue
  sobre los motores, leds, etc. del set
  OSC(esp8266) -> SLIP -> Mega
  https://www.esp8266.com/viewtopic.php?f=29&t=4533

  For upload file: 
  https://tttapa.github.io/ESP8266/Chap12%20-%20Uploading%20to%20Server.html#:~:text=To%20upload%20a%20new%20file,and%20open%20the%20new%20file.
  Upload data dir to esp8266
  $ pio run -t uploadfs
--------------------------------------------------------------------------------------------- */

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
#define DEBUG true

// leds informacion
#define ROJO D0
#define AZUL D1
#define VERDE D2

char ssid[] = "jacs";            // your network SSID (name)
char pass[] = "perlitalagatita"; // your network password
const char * hostName = "JACSesp";

// A UDP instance to let us send and receive packets over UDP
WiFiUDP Udp;
const IPAddress outIp(192, 168, 1, 33); // remote IP (not needed for receive)
const unsigned int outPort = 8888;      // remote port (not needed for receive)
const unsigned int localPort = 8888;    // local port to listen for UDP packets (here's where we send the packets)

// comunicacion serie con el arduino mega
SLIPEncodedSerial SLIPSerial(Serial);
OSCBundle bundleOUT;
OSCErrorCode error;

// web server
ESP8266WebServer server(80);
// a File object
File fsUploadFile;
String getContentType(String filename); // convert the file extension to the MIME type
bool handleFileRead(String path);       // send the right file to the client (if it exists)
void handleFileUpload();                // upload a new file to the SPIFFS

String getContentType(String filename) { // convert the file extension to the MIME type
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}
bool handleFileRead(String path) { // send the right file to the client (if it exists)
  Serial.println("handleFileRead: " + path);
  if (path.endsWith("/")) path += "upload.html";          // If a folder is requested, send the upload form
  String contentType = getContentType(path);             // Get the MIME type
  String pathWithGz = path + ".gz";
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) { // If the file exists, either as a compressed archive, or normal
    if (SPIFFS.exists(pathWithGz))                         // If there's a compressed version available
      path += ".gz";                                         // Use the compressed verion
    File file = SPIFFS.open(path, "r");                    // Open the file
    size_t sent = server.streamFile(file, contentType);    // Send it to the client
    file.close();                                          // Close the file again
    Serial.println(String("\tSent file: ") + path);
    return true;
  }
  Serial.println(String("\tFile Not Found: ") + path);   // If the file doesn't exist, return false
  return false;
}
void handleFileUpload(){ // upload a new file to the SPIFFS
  HTTPUpload& upload = server.upload();
  if(upload.status == UPLOAD_FILE_START){
    String filename = upload.filename;
    if(!filename.startsWith("/")) filename = "/"+filename;
    Serial.print("handleFileUpload Name: "); Serial.println(filename);
    fsUploadFile = SPIFFS.open(filename, "w");            // Open the file for writing in SPIFFS (create if it doesn't exist)
    filename = String();
  } else if(upload.status == UPLOAD_FILE_WRITE){
    if(fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
  } else if(upload.status == UPLOAD_FILE_END){
    if(fsUploadFile) {                                    // If the file was successfully created
      fsUploadFile.close();                               // Close the file again
      Serial.print("handleFileUpload Size: "); Serial.println(upload.totalSize);
      server.sendHeader("Location","/success.html");      // Redirect the client to the success page
      server.send(303);
    } else {
      server.send(500, "text/plain", "500: couldn't create file");
    }
  }
}


/**
 * enciende o apaga los leds de informacion
 */
void infoLeds(int rojo, int verde, int azul)
{
  digitalWrite(ROJO, rojo);
  digitalWrite(VERDE, verde);
  digitalWrite(AZUL, azul);
}

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(ROJO, OUTPUT);
  pinMode(AZUL, OUTPUT);
  pinMode(VERDE, OUTPUT);

  // inicio
  infoLeds(1, 0, 0);

  SLIPSerial.begin(9600);
  Serial.begin(9600);
  Serial.setDebugOutput(true);

  // AP mode
  WiFi.mode(WIFI_AP);
  while (!WiFi.softAP(ssid, pass))
  {
    Serial.println(".");
    delay(100);
  }

  if (DEBUG)
  {
    Serial.print("Iniciado AP ");
    Serial.println(ssid);
    Serial.print("IP address:\t");
    Serial.println(WiFi.softAPIP());
  }

  /* No necesitamos conexion solo AP
  // Connect to WiFi network
  if (DEBUG)
  {
    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);
  }
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED)
  {
    // esperamos a que se conecte
    delay(500);
    if (DEBUG)
      Serial.print(".");
  }

  if (DEBUG)
  {
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.println("Starting UDP");
    Serial.print("Local port: ");
    Serial.println(Udp.localPort());
  } */

  // OSC server
  Udp.begin(localPort);
  if (DEBUG)
  {
    Serial.print("Local port: ");
    Serial.println(Udp.localPort());
  }

  // ready
  for (int i = 1; i < 5; i++)
  {
    infoLeds(1, 0, 0);
    delay(200);
    infoLeds(0, 0, 0);
    delay(200);
  }
  infoLeds(0, 1, 0);

  // file system
  SPIFFS.begin();
  server.on("/upload", HTTP_GET, []() {                 // if the client requests the upload page
    if (!handleFileRead("/upload.html"))                // send it if it exists
      server.send(404, "text/plain", "404: Not Found"); // otherwise, respond with a 404 (Not Found) error
  });

  server.on("/upload", HTTP_POST,                       // if the client posts to the upload page
    [](){ server.send(200); },                          // Send status 200 (OK) to tell the client we are ready to receive
    handleFileUpload                                    // Receive and save the file
  );

  server.onNotFound([]() {                              // If the client requests any URI
    if (!handleFileRead(server.uri()))                  // send it if it exists
      server.send(404, "text/plain", "404: Not Found"); // otherwise, respond with a 404 (Not Found) error
  });

  server.begin();                           // Actually start the server
  Serial.println("HTTP server started");
  



}

void loop()
{
  OSCMessage msg;

  int size = Udp.parsePacket();
  // recibimos un msg por UDP
  if (size > 0)
  {
    while (size--)
    {
      infoLeds(0, 0, 1);
      msg.fill(Udp.read());
      infoLeds(0, 0, 0);
    }
    if (!msg.hasError())
    {
      // lo enviamos al arduino
      SLIPSerial.beginPacket();
      msg.send(SLIPSerial);   // send the bytes to the SLIP stream
      SLIPSerial.endPacket(); // mark the end of the OSC Packet
      msg.empty();            // free space occupied by message
      infoLeds(0, 0, 1);
      delay(20);

      // para procesar en el propio esp
      // msg.dispatch("/address", callback);
    }
  }

  // ready
  infoLeds(0, 1, 0);

  server.handleClient();

}