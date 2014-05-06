struct LED{
  int pin;
  int duration;
  int interval_begin;
  int interval_end;
  int cur_state;
  int end_state;
  unsigned long last_time;
  unsigned long begin_time; 
  boolean blinking;
};

void set_led(struct LED * led, int state) {
  led->cur_state = state;
  led->blinking = false;

}
void start_blinking(struct LED * led, int duration) {
  led->duration = duration;
  led->begin_time = millis();
  led->cur_state = digitalRead(led->pin);
  led->end_state = led->cur_state;
  led->blinking = true;
}
void stop_blinking(struct LED * led) {
  led->blinking = false;
}
void check_led(struct LED * led) {

  if(!led->blinking) {
    digitalWrite(led->pin,led->cur_state);
    return;
  }
  // are we still blinking?
  unsigned long currentMillis = millis();
  int time_elapsed = currentMillis - led->begin_time;
  /*  Serial.print("LED pin:");
   Serial.print(led->pin);
   Serial.print(" elapsed:");
   Serial.print(time_elapsed);
   Serial.print(" duration:");
   Serial.print(led->duration);
   Serial.println(""); */
  if(time_elapsed < led->duration) {
    int inst_interval = led->interval_begin 
      + ((float)time_elapsed/(float)led->duration)*(float)(led->interval_end - led->interval_begin);
    /*  Serial.print(" inst_interval:");
     Serial.print(inst_interval);
     Serial.println(""); */
    if(currentMillis - led->last_time > inst_interval) {
      if(led->cur_state == LOW) 
        led->cur_state = HIGH;
      else
        led->cur_state = LOW;
      digitalWrite(led->pin,led->cur_state);
      led->last_time = currentMillis;
    }
  } 
  else {
    digitalWrite(led->pin, led->end_state);
    led->blinking=false;
  }
}





