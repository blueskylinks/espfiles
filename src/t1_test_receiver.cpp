#include <Arduino.h>

int sdevice[8] = {0};
char motor_st = 'S';

void setup() {
  Serial.begin(9600);
  Serial.println("Receiver Ready");
}

void loop() {
  if (Serial.available()) {
    String received = Serial.readStringUntil('\n');  // read one line
    received.trim();

    // Validate length (should be 9: 8 digits + 1 char)
    if (received.length() == 9) {
      // Parse sdevice array
      for (int i = 0; i < 8; i++) {
        char c = received.charAt(i);
        if (c >= '0' && c <= '9') {
          sdevice[i] = c - '0';
        } else {
          sdevice[i] = 0;  // fallback if invalid char
        }
      }
      // Parse motor_st
      motor_st = received.charAt(8);

      // Debug print
      Serial.print("Received sdevice: ");
      for (int i = 0; i < 8; i++) {
        Serial.print(sdevice[i]);
        Serial.print(" ");
      }
      Serial.print(" motor_st: ");
      Serial.println(motor_st);
    }
  }
}
