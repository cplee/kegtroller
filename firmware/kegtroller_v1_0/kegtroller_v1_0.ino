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

#define BUTTON_DURATION 30000
#define POUR_DURATION 30000


WiFly wifly;


/* Change these to match your WiFi network */
const char mySSID[] = "";
const char myPassword[] = "";

const char site[] = "beer.machadolab.com";

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
    Serial.println("Joining network");
    wifly.setSSID(mySSID);
    wifly.setPassphrase(myPassword);
    wifly.enableDHCP();

    if (wifly.join()) {
      Serial.println("Joined wifi network");
    } 
    else {
      Serial.println("Failed to join wifi network");
      terminal();
    }
  } 
  else {
    Serial.println("Already joined network");
  }

  wifly.setDeviceID("Wifly-WebClient");

  digitalWrite(PIN_LED_RED, HIGH);
 
  Serial.println("Ready!");

}

void loop()
{
  switch(state) {
    case waiting_for_payment:
       loop_waiting_for_payment();
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

void loop_waiting_for_payment() {
  if (check_for_payment()) {
    Serial.println("Pour has been authorized!");
    
    state = waiting_for_button;
    button_timeout = millis() + BUTTON_DURATION;
    set_led(&RED,LOW);
    start_blinking(&YELLOW,BUTTON_DURATION);
    set_led(&GREEN,LOW);
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
  } else if(millis() > button_timeout) {
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
  } else {
    digitalWrite(PIN_VALVE, HIGH);
  }
}

boolean waiting_for_release = false;
boolean check_for_button_release() {
  if(waiting_for_release) {
    if(digitalRead(PIN_BUTTON) == LOW) {
      waiting_for_release = false;
      return true;
    } else {
      return false;
    }
  } else {
    waiting_for_release = (digitalRead(PIN_BUTTON) == HIGH);
    return false;
  }
}

boolean check_for_payment() {
  return check_for_button_release();
    /*
    if (authorizePour(cardId))
     {
     
     digitalWrite(PIN_LED_YELLOW, LOW);
     digitalWrite(PIN_LED_GREEN, HIGH);
     
     Serial.println("Auth successful - waiting for button");
     boolean pourDone = false;
     while (!pourDone)
     {
     if (digitalRead(PIN_BUTTON) == HIGH)
     {
     unsigned long nowMillis = millis();
     digitalWrite(PIN_LED_RED, HIGH);
     digitalWrite(PIN_LED_YELLOW, HIGH);
     digitalWrite(PIN_LED_GREEN, HIGH);
     	  Serial.println("Opening valve");
     digitalWrite(PIN_VALVE, HIGH);
     delay(POUR_DURATION);
     	  Serial.println("Closing valve");
     digitalWrite(PIN_VALVE, LOW);
     pourDone = true;
     }
     }
     }
     else
     {
     
     digitalWrite(PIN_LED_YELLOW, LOW);
     digitalWrite(PIN_LED_RED, HIGH);
     
     Serial.println("Auth failed - no beer for you");
     }
     */
}

/* Connect the WiFly serial to the serial monitor. */
void terminal()
{
  Serial.println("Terminal started:");
  while (1) {
    if (wifly.available() > 0) {
      Serial.write(wifly.read());
    }


    if (Serial.available() > 0) {
      wifly.write(Serial.read());
    }
  }
}


boolean authorizePour(char *id)
{

  float temperature = readTemperature();
  Serial.print("Temperature is ");
  Serial.println(temperature);
  char buf[48];
  snprintf(buf, 47, "/access.php?a=auth&id=%s&t=%d", id, int(temperature));
  int result = webRequest(site, buf);
  Serial.print("Got web result ");
  Serial.println(result);

  if (result >= 200 && result <= 299)
    return true;
  else
    return false;
}


int webRequest(const char *host, char *url)
{

  if (wifly.isConnected()) {
    Serial.println("Old connection active. Closing");
    wifly.close();
  }

  if (wifly.open(host, 80)) {
    Serial.print("Connected to ");
    Serial.println(site);

    /* Send the request */
    wifly.print("GET ");
    wifly.print(url);
    wifly.println(" HTTP/1.1");
    wifly.print("Host: ");
    wifly.println(site);
    wifly.println();

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
    return atoi(respCode);
  } 
  else {
    Serial.println("Failed to connect");
    return 1;
  }
  return 2; 
}


float readTemperature()
{

  int tempReading = analogRead(PIN_TEMP);
  Serial.print("tempReading - ");
  Serial.println(tempReading);

  // converting that reading to voltage, which is based off the reference voltage
  float voltage = tempReading * aref_voltage;
  voltage /= 1024.0; 

  // now print out the temperature
  float temperatureC = (voltage - 0.5) * 100 ;  //converting from 10 mv per degree wit 500 mV offset
  //to degrees ((volatge - 500mV) times 100) 
  // now convert to Fahrenheight
  float temperatureF = (temperatureC * 9.0 / 5.0) + 32.0;

  return temperatureF;  
}





