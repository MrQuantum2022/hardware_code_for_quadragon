#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

#define TCA_FRONT_ADDR 0x70
#define TCA_BACK_ADDR  0x71
#define AS5600_ADDR    0x36

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);

// Your exact perfboard leg mappings
int servo_pins[12] =  {5, 4, 6, 8, 10, 9, 12, 13, 15, 7, 2, 0}; 

void tcaSelect(uint8_t target_tca, uint8_t channel) {
  if (channel > 7) return;
  uint8_t idle_tca = (target_tca == TCA_FRONT_ADDR) ? TCA_BACK_ADDR : TCA_FRONT_ADDR;
  
  Wire.beginTransmission(idle_tca);
  Wire.write(0); 
  Wire.endTransmission();

  Wire.beginTransmission(target_tca);
  Wire.write(1 << channel);
  Wire.endTransmission();
}

uint16_t readAS5600() {
  Wire.beginTransmission(AS5600_ADDR);
  Wire.write(0x0E);
  if (Wire.endTransmission(false) != 0) return 0xFFFF; 
  
  Wire.requestFrom((uint8_t)AS5600_ADDR, (uint8_t)2);
  if (Wire.available() == 2) {
    return (Wire.read() << 8) | Wire.read();
  }
  return 0xFFFF; 
}

void setup() {
  Serial.begin(115200);
  while(!Serial) delay(10);
  
  Wire.begin();
  Wire.setClock(100000); // Stable I2C for encoders
  
  pwm.begin();
  pwm.setOscillatorFrequency(27000000);
  pwm.setPWMFreq(50);

  for(int i=0; i<12; i++) pwm.setPWM(servo_pins[i], 0, 375);
}

void loop() {
  // Wait for the target frame from Python
  if (Serial.available() > 0) {
    String req = Serial.readStringUntil('\n');
    
    int pulses[12];
    int lastIndex = 0;
    
    // Parse the 12 comma-separated pulses
    for(int i=0; i<12; i++) {
      int commaIndex = req.indexOf(',', lastIndex);
      if(commaIndex != -1) {
        pulses[i] = req.substring(lastIndex, commaIndex).toInt();
        lastIndex = commaIndex + 1;
      } else {
        pulses[i] = req.substring(lastIndex).toInt();
      }
      // Instantly apply to motor
      pwm.setPWM(servo_pins[i], 0, pulses[i]);
    }

    // Immediately read physical response
    uint16_t raw_enc[12];
    for(int i=0; i<12; i++) {
      uint8_t tca = (i < 6) ? TCA_FRONT_ADDR : TCA_BACK_ADDR;
      uint8_t channel = (i % 6) + 2; 
      tcaSelect(tca, channel);
      raw_enc[i] = readAS5600();
    }

    // Blast the binary data back to Python
    uint8_t header[2] = {0xAA, 0xBB};
    Serial.write(header, 2);
    Serial.write((uint8_t*)raw_enc, 24);
  }
}