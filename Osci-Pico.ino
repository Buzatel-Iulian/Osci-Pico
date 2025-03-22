#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <LEAmDNS.h>

#include <LittleFS.h>
#include "pico/multicore.h"

#include "index.h"

#ifndef STASSID
#define STASSID "admin"
#define STAPSK "admin_password"
#endif

// REMEMBER TO USE A BASIC BROWSER (Chromium) OR APP TO ACCESS VIA MDNS ADDRESS !!!

const char* ssid = STASSID;
const char* password = STAPSK;

WebServer server(80);

const int led = LED_BUILTIN;

void handleRoot() {
  digitalWrite(led, 1);
  server.send(200, "text/plain", "hello from pico w!\r\n");
  digitalWrite(led, 0);
}

void handleNotFound() {
  digitalWrite(led, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  digitalWrite(led, 0);
}

void split(String * output, String originalString, char delimiter) {
  int startIndex = 0;
  int outIndex = 0;

  for (int i = 0; i < originalString.length(); i++) {
    if (originalString[i] == delimiter) {
      // Extract substring
      output[outIndex] = originalString.substring(startIndex, i);
      outIndex = outIndex + 1;

      // Update the start index
      startIndex = i + 1;
    }
  }

  output[outIndex] = originalString.substring(startIndex);
}

void setup(void) {
  pinMode(led, OUTPUT);
  digitalWrite(led, 0);
  Serial.begin(115200);
  delay(1000);
  LittleFS.begin();


  Serial.println("---------------");
  File i = LittleFS.open("net_conn.txt", "r");
  if (i) {
    while (i.available()) {
      Serial.write(i.read());
    }
    Serial.println("---------------");
    i.close();
  }

  // Start second operation core
  multicore_launch_core1(loop2);

  // Init and start wifi
  init_wifi();

  // Init and start web server and MDNS
  init_server();

}

void loop(void) {
  if (Serial.available()) {
    String comm;
    String sp[10];
    comm = Serial.readString();

    split(sp, comm, ':');

    if (sp[0] == "" || sp[0] == NULL || sp[1] == "" || sp[1] == NULL)
    {
      Serial.write("Invalid input");
    }

    if (sp[0] == "NET")
    {
      Serial.println("Network connection string received");

      LittleFS.begin();
      File f = LittleFS.open("net_conn.txt", "w");
      if (f) {
        f.write(comm.c_str(), strlen(comm.c_str()));
        f.close();
      }
      else
      {
        Serial.println("Network file could not be accessed");
      }
      LittleFS.end();
      WiFi.end();

      init_wifi();

    }

    //Serial.write(sp[0].c_str());
    //Serial.write(sp[1].c_str());


  }


  server.handleClient();
  MDNS.update();
}

void loop2() {
  // Use this to genereate reference square wave
  /////////////////////////////////////////////
  while (true) {
    digitalWrite(LED_BUILTIN, HIGH);  // turn the LED on (HIGH is the voltage level)
    delay(1000);                      // wait for a second
    digitalWrite(LED_BUILTIN, LOW);   // turn the LED off by making the voltage LOW
    delay(1000);                      // wait for a second
  }
  //////////////////////////////////////////////
}

void init_server(){
  ////////// INIT SERVER FUNCTION >>>>

  server.on("/", handleRoot);

  server.on("/inline", []() {
    String message = "this works as well\n\n\n";
    for (uint8_t i = 0; i < server.args(); i++) {
      message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }
    //server.send(200, "text/plain", message);

    // For http://picow.local/inline?query=foobar&query1=foobar1 prints:
    //this works as well


    //query: foobar
    //query1: foobar1

    String json_message = "{\n    \"data\":[\n        ";
    for (uint8_t i = 0; i < 100; i++) {
      json_message += String(i%10) + ",\n        ";
    }
    json_message += "0\n    ]\n}";
    server.send(200, "text/plain", json_message);
  
  });

  server.on("/osc",[]() {
    server.send(200, "text/html", INDEX_HTML);
  });

  server.on("/gif", []() {
    static const uint8_t gif[] = {
      0x47, 0x49, 0x46, 0x38, 0x37, 0x61, 0x10, 0x00, 0x10, 0x00, 0x80, 0x01,
      0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x2c, 0x00, 0x00, 0x00, 0x00,
      0x10, 0x00, 0x10, 0x00, 0x00, 0x02, 0x19, 0x8c, 0x8f, 0xa9, 0xcb, 0x9d,
      0x00, 0x5f, 0x74, 0xb4, 0x56, 0xb0, 0xb0, 0xd2, 0xf2, 0x35, 0x1e, 0x4c,
      0x0c, 0x24, 0x5a, 0xe6, 0x89, 0xa6, 0x4d, 0x01, 0x00, 0x3b
    };
    char gif_colored[sizeof(gif)];
    memcpy_P(gif_colored, gif, sizeof(gif));
    // Set the background to a random set of colors
    gif_colored[16] = millis() % 256;
    gif_colored[17] = millis() % 256;
    gif_colored[18] = millis() % 256;
    server.send(200, "image/gif", gif_colored, sizeof(gif_colored));
  });

  server.onNotFound(handleNotFound);

  /////////////////////////////////////////////////////////
  // Hook examples

  server.addHook([](const String & method, const String & url, WiFiClient * client, WebServer::ContentTypeFunction contentType) {
    (void)method;       // GET, PUT, ...
    (void)url;          // example: /root/myfile.html
    (void)client;       // the webserver tcp client connection
    (void)contentType;  // contentType(".html") => "text/html"
    Serial.printf("A useless web hook has passed\n");
    return WebServer::CLIENT_REQUEST_CAN_CONTINUE;
  });

  server.addHook([](const String&, const String & url, WiFiClient*, WebServer::ContentTypeFunction) {
    if (url.startsWith("/fail")) {
      Serial.printf("An always failing web hook has been triggered\n");
      return WebServer::CLIENT_MUST_STOP;
    }
    return WebServer::CLIENT_REQUEST_CAN_CONTINUE;
  });

  server.addHook([](const String&, const String & url, WiFiClient * client, WebServer::ContentTypeFunction) {
    if (url.startsWith("/dump")) {
      Serial.printf("The dumper web hook is on the run\n");

      // Here the request is not interpreted, so we cannot for sure
      // swallow the exact amount matching the full request+content,
      // hence the tcp connection cannot be handled anymore by the
      auto last = millis();
      while ((millis() - last) < 500) {
        char buf[32];
        size_t len = client->read((uint8_t*)buf, sizeof(buf));
        if (len > 0) {
          Serial.printf("(<%d> chars)", (int)len);
          Serial.write(buf, len);
          last = millis();
        }
      }
      // Two choices: return MUST STOP and webserver will close it
      //                       (we already have the example with '/fail' hook)
      // or                  IS GIVEN and webserver will forget it
      // trying with IS GIVEN and storing it on a dumb WiFiClient.
      // check the client connection: it should not immediately be closed
      // (make another '/dump' one to close the first)
      Serial.printf("\nTelling server to forget this connection\n");
      static WiFiClient forgetme = *client;  // stop previous one if present and transfer client refcounter
      return WebServer::CLIENT_IS_GIVEN;
    }
    return WebServer::CLIENT_REQUEST_CAN_CONTINUE;
  });

  // Hook examples
  /////////////////////////////////////////////////////////

  server.begin();
  Serial.println("HTTP server started");
  
  ///////////  <<<<
}

void init_wifi(){
  //WiFi.end();

  String net_con;
  String sp[10];

  Serial.println("Reading saved WIFI credentials");
  LittleFS.begin();
  File f = LittleFS.open("net_conn.txt", "r");
  if (f) {
    while (f.available()) {
      net_con = f.readString();
      Serial.println(net_con);
    }
    f.close();
  }
  else
  {
    Serial.println("Network file could not be accessed");
  }
  LittleFS.end();

  split(sp, net_con, ':');
  if (!(sp[0] == "" || sp[0] == NULL || sp[1] == "" || sp[1] == NULL))
  {
    ssid = sp[1].c_str();
    password = sp[2].c_str();
  }

  ////////// WIFI INIT >>>
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");
  Serial.println("Connecting to ");
  Serial.println(ssid);
  Serial.println(password);

  // Try for connection
  int i = 0;
  while (WiFi.status() != WL_CONNECTED && i <= 20) {
    delay(1000);
    Serial.print(".");
    i++;
  }

  if (i > 20) {
    Serial.println("");
    Serial.println("Failed to connect to network !");
    return;
  }

  Serial.println("");
  Serial.println("Connection succesfull !");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("picow")) {
    Serial.println("MDNS responder started");
  }

  //////////  <<<
}
