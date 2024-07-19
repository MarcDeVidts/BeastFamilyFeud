#include <Arduino.h>
#include <TickTwo.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h> 
#include <WebServer.h>
#include <LittleFS.h>
#include <WebSocketsServer.h>
#include <Bounce2.h>
#include <esp_wifi.h>

const char* ssid = "Main C";   
const char* password = "Ov3r100mill!";  

// IPAddress local_IP(10, 9, 5, 250);   // Set your static IP address
// IPAddress gateway(10, 9, 4, 1);      // Set your Gateway IP address
// IPAddress subnet(255, 255, 255, 0);     // Set your Subnet Mask
// IPAddress primaryDNS(8, 8, 8, 8);       // Set your primary DNS
// IPAddress secondaryDNS(8, 8, 4, 4);     // Set your secondary DNS

bool wifiConnected = false;

// 32, 33 OK for 24V output
// 

#define BUTTON1_PIN     12
#define BUTTON2_PIN     13
#define BUTTON3_PIN     14

// Bounce2::Button button1 = Bounce2::Button();
// Bounce2::Button button2 = Bounce2::Button();
Bounce2::Button button3 = Bounce2::Button();


#define LED1_DATA_PIN   32
#define LED2_DATA_PIN   33
#define LED3_DATA_PIN   26
#define LED4_DATA_PIN   27
#define NUMPIXELS       22

Adafruit_NeoPixel pixels1 = Adafruit_NeoPixel(NUMPIXELS, LED1_DATA_PIN, NEO_RGB + NEO_KHZ800);
Adafruit_NeoPixel pixels2 = Adafruit_NeoPixel(NUMPIXELS, LED2_DATA_PIN, NEO_RGB + NEO_KHZ800);

Adafruit_NeoPixel pixels3 = Adafruit_NeoPixel(NUMPIXELS, LED3_DATA_PIN, NEO_RGB + NEO_KHZ800);
Adafruit_NeoPixel pixels4 = Adafruit_NeoPixel(NUMPIXELS, LED4_DATA_PIN, NEO_RGB + NEO_KHZ800);

uint8_t result = 0;
bool timer1sDidFire = false;
bool ledTimerDidFire = false;

void pollFast();
void pollLEDs();

WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

void timer1sCallback() {
  timer1sDidFire = true;
}

void ledTimerCallback() {
  ledTimerDidFire = true;
}

TickTwo timer1s(timer1sCallback, 1000, 0, MILLIS);
TickTwo ledTimer(ledTimerCallback, 100, 0, MILLIS);

#define MACHINE_STATE_IDLE 0
#define MACHINE_STATE_VOTE 1
#define MACHINE_STATE_RESULT 2

uint8_t currentIntensity = 0;
uint8_t currentIntensity2 = 0;
uint8_t targetIntensity = 0;
int8_t  fadeDirection = 1;

uint8_t machineState = MACHINE_STATE_VOTE;

uint16_t ledCounter = 0;

void readMacAddress(){
  uint8_t baseMac[6];
  esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac);
  if (ret == ESP_OK) {
    Serial.printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
                  baseMac[0], baseMac[1], baseMac[2],
                  baseMac[3], baseMac[4], baseMac[5]);
  } else {
    Serial.println("Failed to read MAC address");
  }
}

void checkWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    if (wifiConnected) {
      Serial.printf("Disconnected from Wi-Fi\n");
      wifiConnected = false;
    }
  } else {
    if (!wifiConnected) {
      Serial.printf("Connected to Wi-Fi\n");
      Serial.printf("SSID: %s\n", WiFi.SSID().c_str());
      Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
      wifiConnected = true;
    }
  }
}

void handleRoot() {
  if (LittleFS.exists("/index.html")) {
    File file = LittleFS.open("/index.html", "r");
    server.streamFile(file, "text/html");
    file.close();
  } else {
    server.send(404, "text/plain", "File Not Found");
  }
}

void sendStateToClient() {
  String jsonMessage = "{\"machineState\":\"" + String(machineState) + "\",\"winner\":\"" + String(result) + "\"}";
  webSocket.broadcastTXT(jsonMessage);
}

uint8_t previousMachineState = 0;

void setResult(uint8_t winner) {
  machineState = MACHINE_STATE_RESULT;
  result = winner;
  currentIntensity = 0;
  currentIntensity2 = 0;
  ledCounter = 0;
}

void pollFast() {

  uint8_t button1 = digitalRead(BUTTON1_PIN);
  uint8_t button2 = digitalRead(BUTTON2_PIN);
  // uint8_t button3 = digitalRead(BUTTON3_PIN);

  switch (machineState) {
    case MACHINE_STATE_VOTE:
      if (button1 == 0) {
        setResult(1);
        Serial.println("Calling setResult(1) from pollFast");
      }
      if (button2 == 0) {
        setResult(2);
        Serial.println("Calling setResult(2) from pollFast");
      }
    break;
    case MACHINE_STATE_RESULT:
      if (button3.pressed()) {
        machineState = MACHINE_STATE_VOTE;
        result = 0;
      }
    break;
  }

  if (machineState != previousMachineState) {
    sendStateToClient();
  }
  previousMachineState = machineState;

}


void pollLEDs() {

  //Serial.printf("%d, %d, %d\n", button1, button2, button3);

  switch (machineState) {
    case MACHINE_STATE_VOTE:
      if (currentIntensity > targetIntensity) {
        currentIntensity -= 10;
      } else if (currentIntensity < targetIntensity) {
        currentIntensity += 10;
      }
      if (currentIntensity == targetIntensity) {
        targetIntensity = targetIntensity == 100 ? 50 : 100;
      }
      pixels1.fill(pixels1.Color(0, 0, currentIntensity), 0, NUMPIXELS);
      pixels2.fill(pixels2.Color(currentIntensity, 0, 0), 0, NUMPIXELS);
      pixels3.fill(pixels3.Color(0, 0, currentIntensity+150), 0, NUMPIXELS);
      pixels4.fill(pixels4.Color(currentIntensity+150, 0, 0), 0, NUMPIXELS);
      //Serial.printf("currentIntensity = %d, fadeDirection = %d\n", currentIntensity, fadeDirection);
    break;
    case MACHINE_STATE_RESULT:
      ledCounter++;
      if (ledCounter < 20 && ledCounter % 2 == 0) {
        if (currentIntensity == 0) {
          currentIntensity = 100;
          currentIntensity2 = 255;
        } else {
          currentIntensity = 0;
          currentIntensity2 = 0;
        }
      } else if (ledCounter > 40) {
        currentIntensity = 100;
        currentIntensity2 = 255;
      }
      if (ledCounter > 100) {
        ledCounter = 100;
      }
      if (result == 1) {
        pixels1.fill(pixels1.Color(0, 0, 0), 0, NUMPIXELS);
        pixels2.fill(pixels2.Color(currentIntensity, currentIntensity, currentIntensity), 0, NUMPIXELS);
        pixels3.fill(pixels3.Color(0, 0, 0), 0, NUMPIXELS);
        pixels4.fill(pixels4.Color(currentIntensity2, currentIntensity2, currentIntensity2), 0, NUMPIXELS);
      } else if (result == 2) {
        pixels1.fill(pixels1.Color(currentIntensity, currentIntensity, currentIntensity), 0, NUMPIXELS);
        pixels2.fill(pixels2.Color(0, 0, 0), 0, NUMPIXELS);
        pixels3.fill(pixels3.Color(currentIntensity2, currentIntensity2, currentIntensity2), 0, NUMPIXELS);
        pixels4.fill(pixels4.Color(0, 0, 0), 0, NUMPIXELS);
      }

    break;
  }

  pixels1.show();
  pixels2.show();
  pixels3.show();
  pixels4.show();

}

void handleWebSocketMessage(uint8_t num, uint8_t *payload, size_t length) {
  payload[length] = 0; // Null-terminate the string
  String message = (char*)payload;

  Serial.printf("WebSocket message received: %s\n", message.c_str());

  if (message == "setWinner1") {
    setResult(1);
    Serial.println("setWinner1 command received");
  } else if (message == "setWinner2") {
    setResult(2);
    Serial.println("setWinner2 command received");
  } else if (message == "reset") {
    machineState = MACHINE_STATE_VOTE;
    result = 0;
    Serial.println("Reset command received");
  }
  sendStateToClient();
}

void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("WebSocket [%u] disconnected\n", num);
      break;
    case WStype_CONNECTED: {
      IPAddress ip = webSocket.remoteIP(num);
      Serial.printf("WebSocket [%u] connected from %s\n", num, ip.toString().c_str());
      sendStateToClient();
      break;
    }
    case WStype_TEXT:
      handleWebSocketMessage(num, payload, length);
      break;
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  delay(3000);
  Serial.println("Starting");

  pixels1.begin(); // This initializes the NeoPixel library.
  pixels2.begin(); // This initializes the NeoPixel library.
  pixels3.begin(); // This initializes the NeoPixel library.
  pixels4.begin(); // This initializes the NeoPixel library.
  pixels1.setBrightness(150);
  pixels2.setBrightness(150);
  pixels3.setBrightness(255);
  pixels4.setBrightness(255);

  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  pinMode(BUTTON3_PIN, INPUT_PULLUP);

  // button1.attach(BUTTON1_PIN);
  // button1.interval(25);
  // button2.attach(BUTTON2_PIN);
  // button2.interval(25);
  button3.attach(BUTTON3_PIN);
  button3.setPressedState(LOW);
  button3.interval(25);

  timer1s.start();
  ledTimer.start();

  targetIntensity = 100;

  // Set static IP address
  // if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
  //   Serial.println("STA Failed to configure");
  // }

  uint8_t button3 = digitalRead(BUTTON3_PIN);
  if (button3 != 0) {
    WiFi.begin(ssid, password);
    Serial.print("Connecting to Wi-Fi");
  }
  wifiConnected = WiFi.status() == WL_CONNECTED;

  server.on("/", handleRoot);
  server.serveStatic("/", LittleFS, "/");
  server.begin();

  if (!LittleFS.begin()) {
    Serial.println("LittleFS Mount Failed");
    return;
  }

  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);

}


void loop() {

  pollFast();
  // button1.update();
  // button2.update();
  button3.update();

  timer1s.update();
  ledTimer.update();

  if (timer1sDidFire) {
    //Serial.printf("machineState: %d\n", machineState);
      readMacAddress();
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
    timer1sDidFire = 0;
    //checkWiFi();
  }  

  if (ledTimerDidFire) {
    ledTimerDidFire = false;
    pollLEDs();
  }

  server.handleClient();  // Handle web server client
  webSocket.loop();       // Handle WebSocket communication

}