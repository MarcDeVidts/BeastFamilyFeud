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

#define BUTTON1_PIN 5
#define BUTTON2_PIN 18
#define BUTTON3_PIN 19
#define BUTTON4_PIN 21
#define BUTTON5_PIN 22
#define BUTTON6_PIN 23
#define BUTTON7_PIN 15

#define BUTTON_COUNT 7

#define RESET_BUTTON_PIN 33

#define LED1_PIN  13
#define LED2_PIN  12
#define LED3_PIN  14
#define LED4_PIN  27
#define LED5_PIN  26
#define LED6_PIN  32
#define LED7_PIN  4

#define MIN_TIME_ALLOWED_SECONDS 5
#define MAX_TIME_ALLOWED_SECONDS 120

Bounce2::Button resetButton = Bounce2::Button();

#define NUMPIXELS 60

Adafruit_NeoPixel pixels1 = Adafruit_NeoPixel(NUMPIXELS, LED1_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel pixels2 = Adafruit_NeoPixel(NUMPIXELS, LED2_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel pixels3 = Adafruit_NeoPixel(NUMPIXELS, LED3_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel pixels4 = Adafruit_NeoPixel(NUMPIXELS, LED4_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel pixels5 = Adafruit_NeoPixel(NUMPIXELS, LED5_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel pixels6 = Adafruit_NeoPixel(NUMPIXELS, LED6_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel pixels7 = Adafruit_NeoPixel(NUMPIXELS, LED7_PIN, NEO_GRB + NEO_KHZ800);

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

uint16_t timerRefreshRate = 100;

TickTwo timer1s(timer1sCallback, 1000, 0, MILLIS);
TickTwo ledTimer(ledTimerCallback, timerRefreshRate, 0, MILLIS);

#define MACHINE_STATE_IDLE 0
#define MACHINE_STATE_RUNNING 1
#define MACHINE_STATE_STOP 2
#define MACHINE_STATE_RESULT 3

#define GAME_DIRECTION_BOUNCE 0
#define GAME_DIRECTION_RANDOM 1

uint8_t currentIntensity = 0;
uint8_t currentIntensity2 = 0;
uint8_t targetIntensity = 0;
int8_t  fadeDirection = 1;


uint8_t selectedLED = 0;
uint8_t ledDirection = 1;
uint32_t ledColor;
uint16_t ledBlinkCounter = 0;

uint8_t machineState = MACHINE_STATE_RUNNING;

uint16_t ledCounter = 0;

uint16_t stopNumber;

uint16_t minRoundLengthSeconds = 5;
uint16_t maxRoundLengthSeconds = 15;

uint8_t gameDirection = GAME_DIRECTION_RANDOM;

void setStopNumber() {
  uint16_t stopTimeSeconds = minRoundLengthSeconds + random(maxRoundLengthSeconds - minRoundLengthSeconds);
  uint16_t ticksPerSecond = 1000 / timerRefreshRate;
  stopNumber = stopTimeSeconds * ticksPerSecond;
  Serial.printf("stopNumber = %d\n", stopNumber);
}

// void readMacAddress(){
//   uint8_t baseMac[6];
//   esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac);
//   if (ret == ESP_OK) {
//     Serial.printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
//                   baseMac[0], baseMac[1], baseMac[2],
//                   baseMac[3], baseMac[4], baseMac[5]);
//   } else {
//     Serial.println("Failed to read MAC address");
//   }
// }

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
  String jsonMessage =    "{\"machineState\":\"" + String(machineState) + 
                          "\",\"result\":\"" + String(result) + 
                          "\",\"gameDirection\":\"" + String(gameDirection) + 
                          "\",\"minRoundLengthSeconds\":\"" + String(minRoundLengthSeconds) +
                          "\",\"maxRoundLengthSeconds\":\"" + String(maxRoundLengthSeconds) + 
                          "\"}";

  webSocket.broadcastTXT(jsonMessage);
}

uint8_t previousMachineState = 0;

bool correct = false;

void setResult(uint8_t winner) {
  result = winner;
  if (selectedLED == winner) {
    correct = true;
  } else {
    correct = false;
    ledColor = pixels1.Color(255,0,0);

  }
  selectedLED = result;
  machineState = MACHINE_STATE_STOP;
}

void resetGame() {
  machineState = MACHINE_STATE_RUNNING;
  ledCounter = 0;
  setStopNumber();
  result = 0;
  correct = false;
}

void pollFast() {

  uint8_t button1 = digitalRead(BUTTON1_PIN);
  uint8_t button2 = digitalRead(BUTTON2_PIN);
  uint8_t button3 = digitalRead(BUTTON3_PIN);
  uint8_t button4 = digitalRead(BUTTON4_PIN);
  uint8_t button5 = digitalRead(BUTTON5_PIN);
  uint8_t button6 = digitalRead(BUTTON6_PIN);
  uint8_t button7 = digitalRead(BUTTON7_PIN);

  if (resetButton.pressed()) {
    resetGame();
  }

  switch (machineState) {
    case MACHINE_STATE_RUNNING:
      if (button1 == 0) {
        setResult(0);
      }
      if (button2 == 0) {
        setResult(1);
      }
      if (button3 == 0) {
        setResult(2);
      }
      if (button4 == 0) {
        setResult(3);
      }
      if (button5 == 0) {
        setResult(4);
      }
      if (button6 == 0) {
        setResult(5);
      }
      if (button7 == 0) {
        setResult(6);
      }
    break;
  }

  if (machineState != previousMachineState) {
    sendStateToClient();
  }
  previousMachineState = machineState;

}

void pollLEDs() {


  switch (machineState) {
    case MACHINE_STATE_RUNNING:

      if (ledCounter >= stopNumber) {
        ledCounter = stopNumber;
        ledColor = pixels1.Color(0,255,0);
      } else {
        ledColor = pixels1.Color(255,0,0);

        if (gameDirection == GAME_DIRECTION_BOUNCE) {
          if (ledDirection == 1) {
            selectedLED = selectedLED + 1;
            if (selectedLED == BUTTON_COUNT - 1) {
              ledDirection = 2;
            }
          } else if (ledDirection == 2) {
            selectedLED = selectedLED - 1;
            if (selectedLED == 0) {
              ledDirection = 1;
            }
          }
        } else if (gameDirection == GAME_DIRECTION_RANDOM) {
          uint16_t newLED = random(BUTTON_COUNT);
          while(newLED == selectedLED) {
             newLED = random(BUTTON_COUNT);
          }
          selectedLED = newLED;
        }
      }

      ledCounter++;

    break;
    case MACHINE_STATE_STOP:

      ledBlinkCounter++;
      if (ledBlinkCounter == 5) {
        ledColor = 0;
      } else if (ledBlinkCounter >= 10) {
        ledBlinkCounter = 0;
        if (correct && ledCounter >= stopNumber) {
          ledColor = pixels1.Color(0,255,0);
        } else {
          ledColor = pixels1.Color(255,0,0);
        }
      }
    break;
  }

  pixels1.fill(pixels1.Color(0, 0, 0), 0, NUMPIXELS);
  pixels2.fill(pixels2.Color(0, 0, 0), 0, NUMPIXELS);
  pixels3.fill(pixels3.Color(0, 0, 0), 0, NUMPIXELS);
  pixels4.fill(pixels4.Color(0, 0, 0), 0, NUMPIXELS);
  pixels5.fill(pixels5.Color(0, 0, 0), 0, NUMPIXELS);
  pixels6.fill(pixels6.Color(0, 0, 0), 0, NUMPIXELS);
  pixels7.fill(pixels7.Color(0, 0, 0), 0, NUMPIXELS);

  switch (selectedLED) {
    case 0:
      pixels1.fill(ledColor, 0, NUMPIXELS);
      break;
    case 1:
      pixels2.fill(ledColor, 0, NUMPIXELS);
      break;
    case 2:
      pixels3.fill(ledColor, 0, NUMPIXELS);
      break;
    case 3:
      pixels4.fill(ledColor, 0, NUMPIXELS);
      break;
    case 4:
      pixels5.fill(ledColor, 0, NUMPIXELS);
      break;
    case 5:
      pixels6.fill(ledColor, 0, NUMPIXELS);
      break;
    case 6:
      pixels7.fill(ledColor, 0, NUMPIXELS);
      break;
  }

  pixels1.show();
  pixels2.show();
  pixels3.show();
  pixels4.show();
  pixels5.show();
  pixels6.show();
  pixels7.show();

  Serial.printf("%d, %d, %d\n", machineState, result, selectedLED);


}

void handleWebSocketMessage(uint8_t num, uint8_t *payload, size_t length) {
  payload[length] = 0; // Null-terminate the string
  String message = (char*)payload;

  Serial.printf("WebSocket message received: %s\n", message.c_str());

  if (message == "setGameDirectionRandom") {
    gameDirection = GAME_DIRECTION_RANDOM;
    Serial.println("Random command received");
  } else if (message == "setGameDirectionBounce") {
    gameDirection = GAME_DIRECTION_BOUNCE;
    Serial.println("Bounce command received");
  } else if (message == "reset") {
    resetGame();
    Serial.println("Reset command received");
  } else if (message.substring(0,11) == "setMinTime,") {
    String min = message.substring(11,14);
    uint16_t minValue = min.toInt();
    Serial.printf("Min time command received = %s", minValue);
    if (minValue > 0 && minValue < MIN_TIME_ALLOWED_SECONDS) {
      minRoundLengthSeconds = minValue;
    }
  } else if (message.substring(0,11) == "setMaxTime,") {
    String max = message.substring(11,14);
    uint16_t maxValue = max.toInt();
    Serial.printf("Max time command received = %s", maxValue);
    if (maxValue > 0 && maxValue < MAX_TIME_ALLOWED_SECONDS) {
      maxRoundLengthSeconds = maxValue;
    }
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

  randomSeed(analogRead(0));

  pixels1.begin(); // This initializes the NeoPixel library.
  pixels2.begin(); // This initializes the NeoPixel library.
  pixels3.begin(); // This initializes the NeoPixel library.
  pixels4.begin(); // This initializes the NeoPixel library.
  pixels5.begin(); // This initializes the NeoPixel library.
  pixels6.begin(); // This initializes the NeoPixel library.
  pixels7.begin(); // This initializes the NeoPixel library.

  pixels1.setBrightness(255);
  pixels2.setBrightness(255);
  pixels3.setBrightness(255);
  pixels4.setBrightness(255);
  pixels5.setBrightness(255);
  pixels6.setBrightness(255);
  pixels7.setBrightness(255);

  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  pinMode(BUTTON3_PIN, INPUT_PULLUP);
  pinMode(BUTTON4_PIN, INPUT_PULLUP);
  pinMode(BUTTON5_PIN, INPUT_PULLUP);
  pinMode(BUTTON6_PIN, INPUT_PULLUP);
  pinMode(BUTTON7_PIN, INPUT_PULLUP);
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);

  resetButton.attach(RESET_BUTTON_PIN);
  resetButton.setPressedState(LOW);
  resetButton.interval(25);

  setStopNumber();

  timer1s.start();
  ledTimer.start();

  targetIntensity = 100;

  //Set static IP address
  // if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
  //   Serial.println("STA Failed to configure");
  // }

  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  // wifiConnected = WiFi.status() == WL_CONNECTED;

  server.on("/", handleRoot);
  server.serveStatic("/", LittleFS, "/");
  server.begin();

  if (!LittleFS.begin()) {
    Serial.println("LittleFS Mount Failed");
    return;
  }

  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);
  Serial.println("Finished starting");

}


void loop() {

  pollFast();
  // button1.update();
  // button2.update();
  resetButton.update();

  timer1s.update();
  ledTimer.update();

  if (timer1sDidFire) {
    Serial.printf("Buttons: %d,%d\n",digitalRead(BUTTON6_PIN), digitalRead(BUTTON7_PIN));
    //Serial.printf("machineState: %d\n", machineState);
    //readMacAddress();
    // Serial.print("IP: ");
    // Serial.println(WiFi.localIP());
    timer1sDidFire = 0;
    checkWiFi();
  }  

  if (ledTimerDidFire) {
    ledTimerDidFire = false;
    pollLEDs();
  }

  server.handleClient();  // Handle web server client
  webSocket.loop();       // Handle WebSocket communication

}