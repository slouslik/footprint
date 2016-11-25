/*
  footprint.cpp - Arduino sketch for reading a power meter using a phototransistor
  and a SY310 based water & gas pulse meter
  Copyright (c) 2014 Adel Sarhan (asarhan@gmail.com).  All rights reserved.

  Adapted from the WaterMote sketch written by Felix Rusu (felix@lowpowerlab.com)

  This code is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This code is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <EEPROM.h>
#include <TimerOne.h>
#include <SPI.h>
#include <Ethernet.h>

// MAC address for Ethernet controller
byte mac[] = { 0x00, 0xAA, 0xBB, 0xCB, 0xDE, 0x44 };
IPAddress ip(10,0,1,35);
IPAddress dnsd(10,0,1,1);
IPAddress gw(10,0,1,1);
IPAddress nm(255,255,255,0);

#define DEBUGLED      9
#define LED           13
#define GASPIN        0  // INT0 = digital pin 3 on the Leo (must be an interrupt pin!)
#define POWERPIN      1  // INT1 = digital pin 2 on the Leo (must be an interrupt pin!)
#define WATERPIN2     2  // INT2 = digital pin 0 on the Leo (must be an interrupt pin!)
#define WATERPIN      4  // INT4 = digital pin 7 on the Leo (must be an interrupt pin!)

#define TRANSMIT_INTERVAL 5                     // 5 seconds

EthernetClient client;
IPAddress server(10,0,1,27);

unsigned long lastConnectionTime = 0;           // last time you connected to the server, in milliseconds
boolean lastConnected = false;                  // state of the connection last time through the main loop
const unsigned long postingInterval = 10*1000;  // delay between updates, in milliseconds

typedef struct meter {
    float pulsesPerUnit;
    String unit;
  
    // total units
    volatile unsigned long volatileCount;
    unsigned long pulseCount;
    float units;
  
    // current rate of consumption
    volatile unsigned long intervalCount;
    float unitsPerMinute;
  
    // units for last minute
    unsigned long lastMinuteCount;
    float unitsLastMinute;
} Meter;

boolean net = false;
int seconds = 0;
Meter gasMeter = {0};
Meter powerMeter = {0};
Meter waterMeter = {0};

void setup() 
{
    // Open serial communications and wait for port to open:
    Serial.begin(115200);
    // this check is only needed on the Leonardo:
    //while (!Serial) {
    //  ;   // wait for serial port to connect. Needed for Leonardo only
    //}  
  
    // start the Ethernet connection:
    Ethernet.begin(mac, ip, dnsd, gw, nm);
    net = true;
    
    pinMode(LED, OUTPUT);
    pinMode(DEBUGLED, OUTPUT);
    
    attachInterrupt(GASPIN, gasCounter, RISING);
    attachInterrupt(POWERPIN, powerCounter, RISING);
    // attachInterrupt(WATERPIN, waterCounter, RISING);
    
    gasMeter.pulsesPerUnit = 2.0;
    gasMeter.unit = "ft³";
    
    powerMeter.pulsesPerUnit = 1000.0;
    powerMeter.unit = "kWh";
    
    waterMeter.pulsesPerUnit = 0.1;
    waterMeter.unit = "GAL";

    Timer1.initialize(TRANSMIT_INTERVAL * 1000000L);
    Timer1.attachInterrupt(Transmit);
}

void loop() 
{
    runLED();
}

void updateMeter(struct meter &m, boolean calcMinute)
{
    noInterrupts();
    unsigned long intervalCount = m.intervalCount;
    m.intervalCount = 0;
    // error correction
    if (intervalCount > 50) {
      m.volatileCount -= intervalCount;
      m.volatileCount++;
      intervalCount = -1;
    }
    m.pulseCount = m.volatileCount; 
    interrupts();
    
    m.units = ((float) m.pulseCount) / m.pulsesPerUnit;
    if (intervalCount == -1)
      m.unitsPerMinute = -1;
    else
      m.unitsPerMinute = (((float) intervalCount) / TRANSMIT_INTERVAL) * 60.0 / m.pulsesPerUnit;
    
    if (calcMinute)  {
        m.unitsLastMinute = ((float)(m.pulseCount - m.lastMinuteCount)) / m.pulsesPerUnit;
        m.lastMinuteCount = m.pulseCount;
    }
    
    Serial.print("PulseCount: "); Serial.print(m.pulseCount); Serial.print(" " + m.unit + ": "); Serial.print(m.units);
    Serial.print(" UPM: "); Serial.print(m.unitsPerMinute); Serial.print(" ULM: "); Serial.println(m.unitsLastMinute);
}

void Transmit(void) 
{
    boolean calcMinute = false;
    flashLED(1);

    seconds += TRANSMIT_INTERVAL; 
    if (seconds >= 60) {
        seconds = 0;
        calcMinute = true;
    }
        
    updateMeter(gasMeter, calcMinute);
    updateMeter(powerMeter, calcMinute);
    updateMeter(waterMeter, calcMinute);
    
    if (net == true) {
       updateServer();
    }
    else {
      Serial.println("Network not available");
    }
}

void gasCounter(void)
{
    flashLED(2);
    gasMeter.volatileCount++;
    gasMeter.intervalCount++;
}

void powerCounter(void)
{
    flashLED(3);
    powerMeter.volatileCount++;
    powerMeter.intervalCount++;
}

void waterCounter(void)
{
    flashLED(4);
    waterMeter.volatileCount++;
    waterMeter.intervalCount++;
}

void writeMeter(char *buf, struct meter &m)
{
    char tmp[20] = {0};
    strcat(buf, "{");
    strcat(buf, "\"pulses\":");
    strcat(buf, ultoa(m.pulseCount, tmp, 10));
    strcat(buf, ",\"units\":");
    dtostrf(m.units, 3, 3, tmp);
    strcat(buf, tmp);
    strcat(buf, ",\"unitsPerMinute\":");
    dtostrf(m.unitsPerMinute, 3, 3, tmp);
    strcat(buf, tmp);
    strcat(buf, ",\"unitsLastMinute\":");
    dtostrf(m.unitsLastMinute, 3, 3, tmp);
    strcat(buf, tmp);
    strcat(buf, "}");
}


char buffer[512];
char tmp[512];
char len[20];
          
void updateServer()
{
    // if there's a successful connection:
    if (client.connect(server, 1337)) {
        memcpy(buffer, 0, sizeof(buffer));
        memcpy(tmp, 0, sizeof(tmp));
        memcpy(len, 0, sizeof(len));
        
        Serial.println("connected");
        // send the HTTP PUT request:
        strcpy(buffer, "POST /meters.json HTTP/1.1\r\n");
        strcat(buffer, "Host: 10.0.1.27:1337\r\n");
        strcat(buffer, "User-Agent: arduino-ethernet\r\n");
        strcat(buffer, "Content-type: application/json\r\n");
       
        strcpy(tmp, "{\"gasMeter\":");
        writeMeter(tmp, gasMeter);
        strcat(tmp, ",\"powerMeter\":");        
        writeMeter(tmp, powerMeter);
        strcat(tmp, ",\"waterMeter\":");
        writeMeter(tmp, waterMeter);
        strcat(tmp, "}");
        strcat(buffer, "Content-Length: ");
        strcat(buffer, utoa(strlen(tmp), len, 10));
        strcat(buffer, "\r\n");
        strcat(buffer, "Connection: close\r\n\r\n");
        strcat(buffer, tmp);
        client.write(buffer, strlen(buffer));
        client.stop();
      } 
      else {
        // if you couldn't make a connection:
        Serial.println("connection failed");
        Serial.println("disconnecting.");
        client.stop();
      }
      
    if (client.available()) {
        char c = client.read();
        Serial.print(c);
    }
    
    if (!client.connected()) {
        Serial.println();
        Serial.println("disconnecting.");
        client.stop();
    }
}



