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

#include "jacsesp.h"

String getContentType(String filename)
{ // convert the file extension to the MIME type
    if (filename.endsWith(".html"))
        return "text/html";
    else if (filename.endsWith(".css"))
        return "text/css";
    else if (filename.endsWith(".js"))
        return "application/javascript";
    else if (filename.endsWith(".ico"))
        return "image/x-icon";
    else if (filename.endsWith(".gz"))
        return "application/x-gzip";
    else if (filename.endsWith(".csv"))
        return "text/csv";
    return "text/plain";
}

/*
 * Send file to the web client
 */
bool handleFileRead(String path)
{ // send the right file to the client (if it exists)
    if (DEBUG) Serial.println("handleFileRead: " + path);
    if (path.endsWith("/"))
        path += "upload.html";                   // If a folder is requested, send the upload form
    String contentType = getContentType(path); // Get the MIME type
    String pathWithGz = path + ".gz";
    if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path))
    {                                     // If the file exists, either as a compressed archive, or normal
        if (SPIFFS.exists(pathWithGz))      // If there's a compressed version available
            path += ".gz";                    // Use the compressed verion
        File file = SPIFFS.open(path, "r"); // Open the file
        // Send it to the client
        size_t sent = server.streamFile(file, contentType);

        if (DEBUG) Serial.println(String("\tSent file: ") + path);
        file.close(); // Close the file again
        return true;
    }
    // If the file doesn't exist, return false
    return false;
    if (DEBUG) Serial.println(String("\tFile Not Found: ") + path);
}

/*
 * upload a new file to the SPIFFS
 */
void handleFileUpload()
{
    HTTPUpload &upload = server.upload();
    // use the same file ever
    if (DEBUG) Serial.print("Upload start");
    if (upload.status == UPLOAD_FILE_START)
    {

        if (!filename.startsWith("/"))
            filename = "/" + filename;
        if (DEBUG) {
            Serial.print("Upload start");
            Serial.print(" handleFileUpload Name: ");
            Serial.println(filename);
        }
        fsUploadFile = SPIFFS.open(filename, "w"); // Open the file for writing in SPIFFS (create if it doesn't exist)
        filename = String();
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        if (fsUploadFile)
            fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
        if (DEBUG) Serial.println(" upload_file_write");
    }
    else if (upload.status == UPLOAD_FILE_END)
    {

        if (fsUploadFile)
        {
            //fsUploadFile.close(); // Close the file again
            if (DEBUG) {
                Serial.print(" handleFileUpload Size: ");
                Serial.println(upload.totalSize);
            }

            fsUploadFile.close();
            // send OK to client
            server.sendHeader("Location", "/success.html"); // Redirect the client to the success page
            server.send(303);
        }
    }
    else
    {
        server.send(500, "text/plain", "500: couldn't create file");
    }
}

/*
 * Parse csv
 * https://forum.arduino.cc/index.php?topic=340849.0
 *
 * file - csv file
 * str - Character array for the field.
 * size - Size of str array.
 * delim - String containing field delimiters.
 * return - length of field including terminating delimiter.
 *
 * Note, the last character of str will not be a delimiter if
 * a read error occurs, the field is too long, or the file
 * does not end with a delimiter.  Consider this an error
 * if not at end-of-file.
 *
 */
size_t readField(File* file, char* str, size_t size, char* delim) {
    uint8_t ch;
    size_t n = 0;
    while ((n + 1) < size && file->read(&ch, 1) == 1) {
        // Delete CR.
        if (ch == '\r') {
            continue;
        }
        str[n++] = ch;
        if (strchr(delim, ch)) {
            break;
        }
    }
    str[n] = '\0';
    return n;
}

/*
 * Read frames file and send osc to mega
 * One msg per line (frame)
 */
bool sendFramesMega()
{
    // declare the msg
    OSCMessage msgLine("/frame");
    // open the file
    if (!filename.startsWith("/"))
        filename = "/" + filename;
    File framesFile = SPIFFS.open(filename, "r");
    // Rewind the file for read.
    framesFile.seek(0);
    if (DEBUG) {
        Serial.print("Tamanho: ");
        Serial.println(framesFile.size());
    }

    size_t n;      // Length of returned line with delimiter.
    char str[64];  // Must hold longest line with delimiter and zero byte.

    // Read the file and print lines
    // only with ack
    while (true && ack) {
        // delimitador end of line
        n = readField(&framesFile, str, sizeof(str), "\n");

        // done if Error or at EOF.
        if (n == 0) break;

        // delimiter EOL then send line
        if (str[n-1] == '\n' || !framesFile.available()) {
            // Remove the delimiter.
            str[n-1] = 0;

            // add the line to the osc msg
            msgLine.add(str);
            // Print the line.
            if (DEBUG) Serial.println(str);
            // send to mega
            SLIPSerial.beginPacket();
            msgLine.send(SLIPSerial); // send the bytes to the SLIP stream
            SLIPSerial.endPacket(); // mark the end of the OSC Packet
            msgLine.empty();
            /*
             * delay to the mega process
             * TODO: cuando el mega acabe de procesar que mande un ACK
             */
             //ack = false;
            delay(1000);
            if (DEBUG) Serial.println();
        }
        else {
            // At eof, too long, or read error.  Too long is error.
            if (DEBUG) Serial.print(framesFile.available() ? F("error: ") : F("eof: "));
        }

    }
    framesFile.close();
    return true;
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

    SLIPSerial.begin(57600);
    //Serial.begin(9600);
    if (DEBUG) Serial.setDebugOutput(true);

    // AP mode
    WiFi.mode(WIFI_AP);
    while (!WiFi.softAP(ssid, pass))
    {
        if (DEBUG) Serial.println(".");
        delay(100);
    }

    if (DEBUG)
    {
        Serial.print("Iniciado AP ");
        Serial.println(ssid);
        Serial.print("IP address:\t");
        Serial.println(WiFi.softAPIP());
    }

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

    server.on(
        "/", HTTP_POST,             // if the client posts to the upload page
        []() { server.send(200); }, // Send status 200 (OK) to tell the client we are ready to receive
        handleFileUpload);

    server.on(
        "/upload", HTTP_POST,       // if the client posts to the upload page
        []() { server.send(200); }, // Send status 200 (OK) to tell the client we are ready to receive
        handleFileUpload);

    server.on(
        "/upload.html", HTTP_POST,  // if the client posts to the upload page
        []() { server.send(200); }, // Send status 200 (OK) to tell the client we are ready to receive
        handleFileUpload);

    // cliente solicita enviar frames al mega
    server.on("/mega", HTTP_GET, []() {
        if (!sendFramesMega()) {
            // error
            server.send(404, "text/plain", "Error enviando a Mega");
        }
        else {

            // send OK to client
            server.send(200, "text/plain", "Datos enviados al Mega");
            //server.sendHeader("Location", "/success.html"); // Redirect the client to the success page
            //server.send(303);
        }
        });

    server.onNotFound([]() {                              // If the client requests any URI
        if (!handleFileRead(server.uri()))                  // send it if it exists
            server.send(404, "text/plain", "404: Not Found"); // otherwise, respond with a 404 (Not Found) error
        });

    server.begin(); // Actually start the server
    if (DEBUG) Serial.println("HTTP server started");
}

void loop()
{
    // OSCBundle bdl;
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
            // lo enviamos al arduino mega
            SLIPSerial.beginPacket();
            msg.send(SLIPSerial);   // send the bytes to the SLIP stream
            SLIPSerial.endPacket(); // mark the end of the OSC Packet
            msg.empty();            // free space occupied by message
            infoLeds(0, 0, 1);
            delay(20);
        }
    }

    // ready
    infoLeds(0, 1, 0);
    // necesario para el web server
    server.handleClient();
}