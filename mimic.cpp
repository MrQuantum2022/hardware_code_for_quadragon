#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);

// Mapped exactly to your datasheet: FR, FL, BL, BR
int servo_pins[12] = {0, 7, 2, 4, 5, 6, 8, 9, 10, 12, 13, 15}; 

#define SERVO_MIN 150
#define SERVO_MAX 600
#define SERVO_CENTER 375

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10); 
  
  Wire.begin();
  Wire.setClock(100000); // Stable I2C
  
  pwm.begin();
  pwm.setOscillatorFrequency(27000000);
  pwm.setPWMFreq(50); // 50Hz for analog servos

  // Center all legs safely on boot
  for(int i = 0; i < 12; i++) {
    pwm.setPWM(servo_pins[i], 0, SERVO_CENTER);
  }
  Serial.println("READY FOR COMMANDS");
}

void loop() {
  if (Serial.available() > 0) {
    String data = Serial.readStringUntil('\n');
    int commaIndex = data.indexOf(',');
    
    if (commaIndex > 0) {
      int joint = data.substring(0, commaIndex).toInt();
      int pulse = data.substring(commaIndex + 1).toInt();
      
      if (joint >= 0 && joint <= 11 && pulse >= SERVO_MIN && pulse <= SERVO_MAX) {
        pwm.setPWM(servo_pins[joint], 0, pulse);
      }
    }
  }
}