#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);

// Ensure these match how you plugged the servos into the PCA9685
int servo_pins[12] = {5, 4, 6, 8, 10, 9, 12, 13, 15, 7, 2, 0}; 

#define SERVO_MIN 150
#define SERVO_MAX 600
#define SERVO_CENTER 375

void setup() {
  Serial.begin(115200);
  
  Wire.begin();
  pwm.begin();
  pwm.setOscillatorFrequency(27000000);
  pwm.setPWMFreq(50); // Analog servos run at 50Hz

  // Center all physical servos on boot
  for(int i = 0; i < 12; i++) {
    pwm.setPWM(servo_pins[i], 0, SERVO_CENTER);
  }
}

void loop() {
  if (Serial.available() > 0) {
    // Read the incoming command from Python
    String data = Serial.readStringUntil('\n');
    int commaIndex = data.indexOf(',');
    
    if (commaIndex > 0) {
      int joint = data.substring(0, commaIndex).toInt();
      int pulse = data.substring(commaIndex + 1).toInt();
      
      // Safety limits before moving the physical hardware
      if (joint >= 0 && joint <= 11 && pulse >= SERVO_MIN && pulse <= SERVO_MAX) {
        pwm.setPWM(servo_pins[joint], 0, pulse);
      }
    }
  }
}