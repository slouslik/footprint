int brightness = 0;    // how bright the LED is
int fadeAmount = 5;    // how many points to fade the LED by
int busy = 0;
int flashes = 0;

void runLED() {

  if (flashes <= 0.0) {
    return;
  }
  
  if (busy < 200) {
    busy++;
    return;
  }
  busy = 0;
  
  // set the brightness of pin 9:
  analogWrite(LED, brightness);    

  // change the brightness for next time through the loop:
  brightness = brightness + fadeAmount;

  // reverse the direction of the fading at the ends of the fade: 
  if (brightness == 0 || brightness == 255) {
     fadeAmount = -fadeAmount ;
  }
  
  if (brightness == 0) {
     digitalWrite(LED, LOW);
     flashes--;
  }
}

void flashLED(int num) {
   flashes += num;
}

void printIP() {
  
    // print your local IP address:
    Serial.print("My IP address: ");
    for (byte thisByte = 0; thisByte < 4; thisByte++) {
        // print the value of each byte of the IP address:
        Serial.print(Ethernet.localIP()[thisByte], DEC);
        Serial.print("."); 
    }
    Serial.println("");
}
