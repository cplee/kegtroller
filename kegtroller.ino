#include <WiFlyHQ.h>

/*
 * WiFlyHQ Example httpclient.ino
 *
 * This sketch implements a simple Web client that connects to a 
 * web server, sends a GET, and then sends the result to the 
 * Serial monitor.
 *
 * This sketch is released to the public domain.
 *
 */

#include <WiFlyHQ.h>

#include <SoftwareSerial.h>
SoftwareSerial wifiSerial(4,5);

#include <Wire.h>

#define IRQ   (2)
#define RESET (3)  // Not connected by default on the NFC Shield

#define PIN_TEMP 1
#define aref_voltage 3.3

#define PIN_VALVE  8
#define PIN_LED_RED 9
#define PIN_LED_YELLOW 10
#define PIN_LED_GREEN 11
#define PIN_BUTTON 12



WiFly wifly;

/* Create a file named config.h...you can start with config.h.example */
#include "Config.h"


void terminal();

#include "LED.h"
LED GREEN = {
  PIN_LED_GREEN,0,1000,50,LOW,0,0,false};
LED YELLOW = {
  PIN_LED_YELLOW,0,1000,50,LOW,0,0,false};
LED RED = {
  PIN_LED_RED,0,1000,50,HIGH,0,0,false};

#include "State.h"
State state = waiting_for_payment;
unsigned long button_timeout = 0;
unsigned long pour_timeout = 0;

void setup()
{
  char buf[32];

  Serial.begin(115200);
  analogReference(EXTERNAL);

  pinMode(PIN_VALVE, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_YELLOW, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_BUTTON, INPUT);

  // close valve
  digitalWrite(PIN_VALVE, LOW);

  // init led
  digitalWrite(PIN_LED_RED, LOW);
  digitalWrite(PIN_LED_YELLOW, LOW);
  digitalWrite(PIN_LED_GREEN, HIGH);




  Serial.println("Starting");
  Serial.print("Free memory: ");
  Serial.println(wifly.getFreeMemory(),DEC);

  wifiSerial.begin(9600);
  if (!wifly.begin(&wifiSerial, &Serial)) {
    Serial.println("Failed to start wifly");
    terminal();
  }

  // progress...
  digitalWrite(PIN_LED_YELLOW, HIGH);


  /* Join wifi network if not already associated */
  if (!wifly.isAssociated()) {
    /* Setup the WiFly to connect to a wifi network */
    Serial.print("Joining network ");
    Serial.println(WIFI_SSID);

    wifly.setSSID(WIFI_SSID);
    wifly.setPassphrase(WIFI_PASSWORD);
    wifly.enableDHCP();

    if (wifly.join()) {
      Serial.print("Joined wifi network ");
      Serial.println(WIFI_SSID);
    } 
    else {
      Serial.print("Failed to join wifi network ");
      Serial.println(WIFI_SSID);
      terminal();
    }
  } 
  else {
    Serial.print("Already joined network ");
    Serial.println(WIFI_SSID);

  }

  wifly.setDeviceID("Wifly-WebClient");

  digitalWrite(PIN_LED_RED, HIGH);
  wifly.close();
  Serial.println("Ready!");

}

void loop()
{
  switch(state) {
  case waiting_for_payment:
    loop_waiting_for_payment();
    break;
  case waiting_for_override:
    loop_waiting_for_override();
    break;
  case waiting_for_button:
    loop_waiting_for_button();
    break;
  case pouring:
    loop_pouring();
    break;
  }

  // update LEDs
  check_led(&GREEN);
  check_led(&YELLOW);
  check_led(&RED);
}

void loop_waiting_for_override() {
    unsigned long currentMillis = millis();
    unsigned long releaseTimeMillis = RED.begin_time + OVERRIDE_DURATION;
    int wiggle = 250;
    
    if(currentMillis > releaseTimeMillis + wiggle) {
        set_led(&RED,HIGH);
        stop_blinking(&RED);
        state=waiting_for_payment;
        Serial.println("Timout waiting for override release.");

    } else if(digitalRead(PIN_BUTTON) == LOW) {
      if(currentMillis >= (releaseTimeMillis - wiggle)) {
            set_led(&RED,LOW);
            set_led(&YELLOW,LOW);
            start_blinking(&GREEN,POUR_DURATION);
            state = pouring;
            pour_timeout = millis() + POUR_DURATION;
      } else {
        set_led(&RED,HIGH);
        stop_blinking(&RED);
        state=waiting_for_payment;
        
        Serial.println("Too early for override release.");
      }
    }  
}

void loop_waiting_for_payment() {
  if (check_for_payment()) {
    Serial.println("Pour has been authorized!");

    state = waiting_for_button;
    button_timeout = millis() + BUTTON_DURATION;
    set_led(&RED,LOW);
    start_blinking(&YELLOW,BUTTON_DURATION);
    set_led(&GREEN,LOW);
  } else if (digitalRead(PIN_BUTTON) == HIGH) {
    start_blinking(&RED, OVERRIDE_TIMEOUT);
    state=waiting_for_override;
  } else {
    // locked
    set_led(&RED,HIGH);
    set_led(&YELLOW,LOW);
    set_led(&GREEN,LOW);
  }
}

void loop_waiting_for_button() {
  if(check_for_button_release()) {
    Serial.println("Pouring!");

    set_led(&RED,LOW);
    set_led(&YELLOW,LOW);
    start_blinking(&GREEN,POUR_DURATION);
    state = pouring;
    pour_timeout = millis() + POUR_DURATION;
  } 
  else if(millis() > button_timeout) {
    state = waiting_for_payment;
    Serial.println("Timout waiting for button.");
  }
}

void loop_pouring() {
  if(millis() > pour_timeout) {
    // locked
    set_led(&RED,HIGH);
    set_led(&YELLOW,LOW);
    set_led(&GREEN,LOW);
    digitalWrite(PIN_VALVE, LOW);
    state = waiting_for_payment;
    Serial.println("Pour is finished.");
  } 
  else {
    digitalWrite(PIN_VALVE, HIGH);
  }
}

boolean waiting_for_release = false;
boolean check_for_button_release() {
  if(waiting_for_release) {
    if(digitalRead(PIN_BUTTON) == LOW) {
      waiting_for_release = false;
      return true;
    } 
    else {
      return false;
    }
  } 
  else {
    waiting_for_release = (digitalRead(PIN_BUTTON) == HIGH);
    return false;
  }
}

boolean check_for_payment() {
  if (!wifly.isConnected()) {
    Serial.println("Creating connection");
    if (wifly.open(API_HOST, API_PORT)) {
      Serial.print("Connected to ");
      Serial.print(API_HOST);
      Serial.print(":");
      Serial.println(API_PORT);


      /* Send the request */
      wifly.print("GET ");
      wifly.print(API_PATH);
      wifly.println(" HTTP/1.1");
      wifly.print("Host: ");
      wifly.println(API_HOST);
      wifly.println();
    } 
    else {
      Serial.println("Failed to connect");
      return false;
    }
  }

  // check for resp
  if (wifly.available() > 0)
  {
     char prevChar = 0;
        char curChar = 0;
        char respCode[4] = "99";
        boolean inCode = false;
        boolean haveCode = false;
        int i = 0;
        
        while(!haveCode) 
        {
          if (wifly.available() > 0)
          {
            prevChar = curChar;
            curChar = wifly.read();
            if (curChar == ' ')
            {
              if (inCode)
              {
                inCode = false;
                haveCode = true;
                respCode[i] = '\0';
              }
              else
              {
                inCode = true;
              }
            }
            else if (inCode)
            {
                if (i <= 3)
                  respCode[i++] = curChar; 
            }
            else if (curChar == '\r')
            {
               haveCode = true;
            }
          }
        }
        
    wifly.close();
    int code = atoi(respCode);
    Serial.print ("Got response: ");
    Serial.println(code);

    return (code==200);
  } 

}

/* Connect the WiFly serial to the serial monitor. */
void terminal()
{
  Serial.println("Terminal started:");
  while (1) {
    if (wifly.available() > 0) {
      Serial.write((char)wifly.read());
    }


    if (Serial.available() > 0) {
      wifly.write(Serial.read());
    }
  }
}












