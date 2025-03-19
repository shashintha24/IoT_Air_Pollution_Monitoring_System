#include <SoftwareSerial.h>
#include <SD.h>                           // SD module library
#include <TMRpcm.h>                       // Speaker control library
#include "MQ135.h"
#include <LiquidCrystal.h>

#define SD_ChipSelectPin 4                // CS pin for SD module
#define serialCommunicationSpeed 115200
#define DEBUG true

// Define sensor and LCD
const int sensorPin = A0;
MQ135 gasSensor(sensorPin);
LiquidCrystal lcd(7, A1, A3, A2, A5, A4);

// Serial Communication
SoftwareSerial esp8266(10, 8);
SoftwareSerial mySerial(2, 3);

// Speaker Setup
TMRpcm tmrpcm;

bool alertSent = false;  // Flag to prevent repeated alerts
unsigned long alertStartTime = 0;  // Time when the alert started
bool alertInProgress = false;  // Flag to indicate if an alert is in progress

void setup() {
    Serial.begin(serialCommunicationSpeed);
    esp8266.begin(serialCommunicationSpeed);
    mySerial.begin(serialCommunicationSpeed);
    InitWifiModule();
    
    tmrpcm.speakerPin = 9;
    if (!SD.begin(SD_ChipSelectPin)) {
        if (DEBUG) Serial.println("SD card initialization failed!");
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("SD Init Failed!");
        while (true); // Halt execution
    }
    tmrpcm.setVolume(1);
    
    lcd.begin(16, 2);
    lcd.setCursor(0, 0);
    lcd.print("Setting Up...");
    lcd.setCursor(0, 1);
    static unsigned long lastReadingTime = 0;
    unsigned long currentTime = millis();
    
    if (currentTime - lastReadingTime >= 5000) { // Read sensor every 5 seconds
        float air_quality = gasSensor.getPPM();
        handleWebServer(air_quality);
        displayLCD(air_quality);
        handleAlerts(air_quality);
        lastReadingTime = currentTime;
    }
    delay(1000);
    if (alertInProgress && millis() - alertStartTime >= 11500) {
        tmrpcm.setVolume(1);
        alertInProgress = false;
    }
void loop() {
    float air_quality = gasSensor.getPPM();
    handleWebServer(air_quality);
    displayLCD(air_quality);
    handleAlerts(air_quality);
    delay(1000);
}

void handleWebServer(float air_quality) {
    if (esp8266.available() && esp8266.find("+IPD,")) {
        int connectionId = esp8266.read() - 48;
        
        String webpage = "<h1>IOT Air Pollution Monitoring System</h1><p><h2>";
        webpage += "Air Quality is ";
        sendData("AT+CIPSEND=" + String(connectionId) + "," + String(webpage.length()) + "\r\n", 3000, DEBUG);
        webpage += " PPM</h2><p>";
        
        if (air_quality <= 20) {
            webpage += "Fresh Air";
        } else if (air_quality <= 50) {
            webpage += "Poor Air";
        } else {
            webpage += "Danger! Move to Fresh Air";
        }
        webpage += "</p></body>";

        sendData("AT+CIPSEND=" + String(connectionId) + "," + String(webpage.length()) + "\r\n", 1000, DEBUG);
        sendData(webpage, 1000, DEBUG);
        sendData("AT+CIPCLOSE=" + String(connectionId) + "\r\n", 3000, DEBUG);
    }
}

void displayLCD(float air_quality) {
    lcd.setCursor(0, 0);
    lcd.print("Air Quality: ");
    lcd.print(air_quality);
    lcd.print(" PPM ");
    lcd.setCursor(0, 1);
    
    if (air_quality <= 20) {
        lcd.print("Fresh Air    ");
    } else if (air_quality <= 50) {
        lcd.print("Poor Air    ");
    } else {
        lcd.print("DANGER! Move!");
    }
}

void handleAlerts(float air_quality) {
    if (air_quality > 50 && !alertSent) {
        SendMessage();
        SendMessage2();
        tmrpcm.setVolume(4);
        tmrpcm.play("1.wav");
        delay(11500);
        alertStartTime = millis();
        alertInProgress = true;
        alertSent = true;  // Prevent repeated alerts
    } else if (air_quality <= 50) {
        alertSent = false;  // Reset alert when air quality improves
    }
}

String sendData(String command, const int timeout, boolean debug) {
    String response = "";
    esp8266.print(command);
    long int time = millis();
    
    while ((time + timeout) > millis()) {
        while (esp8266.available()) {
            char c = esp8266.read();
            response += c;
        }
    }
    if (debug) Serial.print(response);
    return response;
}

void InitWifiModule() {
    sendData("AT+RST\r\n", 2000, DEBUG);
    sendData("AT+CWJAP=\"ESP\",\"wifi0021\"\r\n", 2000, DEBUG);
    delay(3000);
    sendData("AT+CWMODE=1\r\n", 1500, DEBUG);
    sendData("AT+CIFSR\r\n", 1500, DEBUG);
    sendData("AT+CIPMUX=1\r\n", 1500, DEBUG);
    sendData("AT+CIPSERVER=1,80\r\n", 1500, DEBUG);
}

void SendMessage() {
    mySerial.println("AT+CMGF=1");
    delay(1000);
    mySerial.println("AT+CMGS=\"9143xxxxxx\"\r");
    delay(1000);
    mySerial.println("Danger! Move to Fresh Air");
    delay(100);
    mySerial.println((char)26);
    delay(1000);
}

void SendMessage2() {
    mySerial.println("AT+CMGF=1");
    delay(1000);
    mySerial.println("AT+CMGS=\"9934xxxxxx\"\r");
    delay(1000);
    mySerial.println("Danger! Move to Fresh Air");
    delay(100);
    mySerial.println((char)26);
    delay(1000);
}
