#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>

#include "Air_Quality_Sensor.h"
#include "sensirion_common.h"
#include "sgp30.h"

// A0: Air quality sensor
// A1: MQ9 sensor
// I2C: VOC eCO2 sensor
AirQualitySensor airquality(A0);

// MAC address and IP address
byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
IPAddress ip(192, 168, 1, 177);

// Listen on port 80
EthernetServer server(80);


void setup() {
  Serial.begin(9600);
  while (!Serial);

  Ethernet.begin(mac, ip);
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
    while (true) {
      delay(1); // do nothing, no point running without Ethernet hardware
    }
  }
  if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println("Ethernet cable is not connected.");
  }
  server.begin();
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());  

  Serial.print("AirQualitySensor init");
  for (int i=0; i<5; i++) {
    delay(1000);
    Serial.print(".");
  }
  if (airquality.init()) {
    Serial.println(" ready.");
  }
  else {
    Serial.println(" ERROR!");
  }

  /*Init module,Reset all baseline,The initialization takes up to around 15 seconds, during which
all APIs measuring IAQ(Indoor air quality ) output will not change.Default value is 400(ppm) for co2,0(ppb) for tvoc*/
  while (sgp_probe() != STATUS_OK) {
    Serial.println("SGP failed");
    while(1);
  }
  s16 err;
  u16 scaled_ethanol_signal, scaled_h2_signal;
  /*Read H2 and Ethanol signal in the way of blocking*/
  err = sgp_measure_signals_blocking_read(&scaled_ethanol_signal,
                                          &scaled_h2_signal);
  if (err == STATUS_OK) {
    Serial.println("SGP30 signals read");
  } else {
    Serial.println("error reading SGP30 signals"); 
  }
  err = sgp_iaq_init();
  
  Serial.println("Setup complete.");
}


String SensorMq9(int pin) {
  Serial.println("Being MQ9");
  float sensor_volt;
  float RS_gas; // Get value of RS in a GAS
  float ratio; // Get ratio RS_GAS/RS_air
  int sensorValue = analogRead(pin);
  sensor_volt=(float)sensorValue/1024*5.0;
  RS_gas = (5.0-sensor_volt)/sensor_volt; // omit *RL

    /*-Replace the name "R0" with the value of R0 in the demo of First Test -*/
    ratio = RS_gas/-0.10;  // ratio = RS/R0
          /*-----------------------------------------------------------------------*/

  String jsonOut;
  jsonOut = "{ sensor_volt: ";
  jsonOut += sensor_volt;
  jsonOut += ", RS_ratio: ";
  jsonOut += RS_gas;
  jsonOut += ", Rs/R0: ";
  jsonOut += ratio;
  jsonOut += "}";
  Serial.println("End MQ9");
  return jsonOut;
}

String SensorAirQuality(void) {
  Serial.println("Begin AirQuality");
  String jsonOut;
  int quality = airquality.slope();

  jsonOut = "{ AirQuality: ";
  jsonOut += airquality.getValue();
  jsonOut += ", PollutionLevel: ";
  if (quality == AirQualitySensor::FORCE_SIGNAL) {
    jsonOut += "max";
  }
  else if (quality == AirQualitySensor::HIGH_POLLUTION) {
    jsonOut += "high";
  }
  else if (quality == AirQualitySensor::LOW_POLLUTION) {
    jsonOut += "low";
  }
  else if (quality == AirQualitySensor::FRESH_AIR) {
    jsonOut += "min";
  }
  jsonOut += " }";

  Serial.println("End AirQuality");
  return jsonOut;
}

String SensorSgp30(void) {
  Serial.println("Begin SGP30");
  s16 err=0;
  u16 tvoc_ppb, co2_eq_ppm;
  err = sgp_measure_iaq_blocking_read(&tvoc_ppb, &co2_eq_ppm);
  String jsonOut;
  if (err == STATUS_OK) {
    jsonOut = "{ tVOC: ";
    jsonOut += tvoc_ppb;
    jsonOut += "ppb, CO2: ";
    jsonOut += co2_eq_ppm;
    jsonOut += "ppm }";
  } else {
    jsonOut =("error reading IAQ values\"\n"); 
  }
  Serial.println("End SGP30");
  return jsonOut;
}


void loop() {
  // listen for incoming clients
  EthernetClient client = server.available();
  if (client) {
    Serial.println("new client");
    // an http request ends with a blank line
    bool currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");  // the connection will be closed after completion of the response
          client.println("Refresh: 5");  // refresh the page automatically every 5 sec
          client.println();
          client.println("<!DOCTYPE HTML>");
          client.println("<html>");

          client.print("MQ9: ");
          client.print(SensorMq9(A1));
          client.println("<br/>");
          client.print("AirQuality: ");
          client.print(SensorAirQuality());
          client.println("<br/>");
          client.print("SGP30: ");
          client.print(SensorSgp30());
          client.println("<br/>");

          client.println("</html>");
          break;
        }
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        } else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);
    // close the connection:
    client.stop();
    Serial.println("client disconnected");
  }
}
