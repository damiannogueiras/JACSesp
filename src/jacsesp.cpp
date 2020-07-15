/*---------------------------------------------------------------------------------------------
  Open Sound Control (OSC) server for the ESP8266/ESP32
  Recibe mensages OSC y los manda al MEGA2560 para que actué
  sobre los motores, leds, etc. del set
  OSC(esp8266) -> SLIP -> Mega
  https://www.esp8266.com/viewtopic.php?f=29&t=4533
--------------------------------------------------------------------------------------------- */

#include <ESP8266WiFi.h>
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

// A UDP instance to let us send and receive packets over UDP
WiFiUDP Udp;
const IPAddress outIp(192, 168, 1, 33); // remote IP (not needed for receive)
const unsigned int outPort = 8888;      // remote port (not needed for receive)
const unsigned int localPort = 8888;    // local port to listen for UDP packets (here's where we send the packets)

// comunicacion serie con el arduino mega
SLIPEncodedSerial SLIPSerial(Serial);
OSCBundle bundleOUT;
OSCErrorCode error;

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
  //Serial.begin(9600);

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
}