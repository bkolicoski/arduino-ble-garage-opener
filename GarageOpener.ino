#include <SoftwareSerial.h>
#include <EEPROM.h>

#define RXD2 14
#define TXD2 12

#define RELAY_PIN D2
#define BTN_PIN D1
#define LED_PIN D4

long pressStartTime = 0;
long paringStartTime = 0;

SoftwareSerial mySerial(RXD2, TXD2);

String content = "";
String incomming = "";

String allowed[8] = { };

void setup()
{
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);
  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  Serial.begin(9600);
  mySerial.begin(9600);

  delay(3000);

  initAllowedDevices();

  digitalWrite(LED_PIN, LOW);
  delay(1000);
  digitalWrite(LED_PIN, HIGH);
  delay(1000);
  digitalWrite(LED_PIN, LOW);
  delay(1000);
  digitalWrite(LED_PIN, HIGH);
  delay(1000);
}

void loop() {

  if (mySerial.available()) {
    incomming = mySerial.readStringUntil('\n');
    Serial.println("Received data");
    Serial.println(incomming);

    String at_command = incomming.substring(0, incomming.indexOf('='));
    if(at_command == "+RCV") {
      String data = incomming.substring(incomming.lastIndexOf(',') + 1, incomming.length() - 1);
      if(data == "OPEN") {
        digitalWrite(RELAY_PIN, LOW);
        delay(1000);
        digitalWrite(RELAY_PIN, HIGH);
      }
    } else if(at_command.startsWith("+++++C")) {
      //handle +++++C1,0x60BEB589C804
      String conn_no = at_command.substring(6, 7);
      String dev_address = at_command.substring(8, 22);
      Serial.println(dev_address);
      bool found = false;
      for(int i = 0; i < 8; i++) {
        if(allowed[i] == dev_address) {
          Serial.println("Found in allowed devices");
          found = true;
          break;
        }
      }
      Serial.println(found);
      if(!found) {
        //device was not found in allowed, disconnect
        Serial.println("Not found, disconnect");
        writeDataToSerial("AT+DCON=" + conn_no);
      }
    }
  }

  if(digitalRead(BTN_PIN) == LOW && pressStartTime == 0) {
    pressStartTime = millis();
    Serial.println("Start pressing");
    delay(400);
  } else if(digitalRead(BTN_PIN) == LOW && millis() > pressStartTime + 5000) {
    paringStartTime = millis();
    digitalWrite(LED_PIN, LOW);
    while(digitalRead(BTN_PIN) == LOW) {
      //wait for button to be released
      delay(400);
    }
  } else if(digitalRead(BTN_PIN) == HIGH) {
    pressStartTime = 0;
    paringStartTime = 0;
    //Serial.println("Reset timings");
  }

  if(paringStartTime != 0) {
    Serial.println("Start pairing");
    while(paringStartTime + 30000 > millis()) {
      digitalWrite(LED_PIN, LOW);
      delay(300);
      digitalWrite(LED_PIN, HIGH);
      delay(300);
      if (mySerial.available()) {
        incomming = mySerial.readString();
        Serial.println("Received data while pairing");
        Serial.println(incomming);
        if(incomming.startsWith("+++++C")) {
          //handle +++++C1,0x60BEB589C804
          String conn_no = incomming.substring(6, 7);
          String dev_address = incomming.substring(8, 22);
          bool added = false;
          for(int i = 0; i < 8; i++) {
            if(allowed[i].length() == 0) {
              //empty position add to allowed
              allowed[i] = dev_address;
              added = true;
              break;
            }
          }
          if(!added) {
            //no more space, cancel connection
            writeDataToSerial("AT+DCON=" + conn_no);
          }
          saveAllowedDevices();
          Serial.println("Printing allowed devices");
          for(int i = 0; i < 8; i++) {
            Serial.println(allowed[i]);
          }
        }
      }

      if(digitalRead(BTN_PIN) == LOW) {
        //reset all devices if clicked while pairing mode
        for(int i = 0; i < 8; i++) {
          allowed[i] = "";
          writeDataToSerial("AT+DCON=" + i);
        }
        Serial.println("Reset all");
        saveAllowedDevices();
      }
    }
    paringStartTime = 0;
    pressStartTime = 0;
  }

  if(Serial.available()) {
    String str = Serial.readString();
    str.trim();
    writeDataToSerial(str);
  }
}

void writeDataToSerial(String data) 
{
  String content = "";
  content = data + "\r\n";
  char* bufc = (char*) malloc(sizeof(char) * content.length() + 1);
  content.toCharArray(bufc, content.length() + 1);
  mySerial.write(bufc);
  free(bufc);

  if (mySerial.available()) {
    incomming = mySerial.readString();
    Serial.println("Received data after command");
    Serial.println(incomming);
  }
}

void initAllowedDevices() {
  EEPROM.begin(512);
  Serial.println("Read first two EEPROM");
  Serial.println(EEPROM.read(0));
  Serial.println(EEPROM.read(1));
  if (EEPROM.read(0) == 'T') {
    String xx = "";
    int end_ = EEPROM.read(1);
    for (int x = 4; x < end_ + 4; x++ ) {
      xx += char(EEPROM.read(x));
    }
    Serial.println("read from EEPROM");
    Serial.println(xx);
    int dev_index = 0;
    String tmp_string = "";
    for (int i = 0; i < xx.length(); i++) {
      if(xx[i] == ':') {
        //separator
        allowed[dev_index] = tmp_string;
        tmp_string = "";
        dev_index++;
      } else {
        tmp_string += xx[i];
      }
    }
  }
  EEPROM.end();
  Serial.println("Printing allowed devices");
  for(int i = 0; i < 8; i++) {
    Serial.println(allowed[i]);
  }
}

void saveAllowedDevices() {
  EEPROM.begin(512);
  String xx = "";
  for(int i = 0; i < 8; i++) {
    if(i == 0) {
      xx += allowed[i];
    } else {
      xx += ":" + allowed[i];
    }
  }
  Serial.println("Write to EEPROM");
  Serial.println(xx);
  int end_ = xx.length();
  Serial.print("Length: ");
  Serial.println(end_);
  EEPROM.write(1, end_);  // store string length
  for (int x = 4; x < end_ + 4; x++ ) {
    byte mm = xx.charAt(x - 4);
    if ( EEPROM.read(x) != mm ) { // prevents writing same thing into a memory cell
      EEPROM.write(x, mm);
    }
  }
  //mark epprom as initialized for next reboot
  if ( EEPROM.read(0) != 'T' ) { // prevents writing same thing into a memory cell
    EEPROM.write(0, 'T');
  }
  EEPROM.end();
}
