#include <Arduino.h>
#include <Wire.h>

#define PCA9685_ADDR   0x40
#define TCA_FRONT_ADDR 0x70
#define TCA_BACK_ADDR  0x71
#define AS5600_ADDR    0x36
#define MPU6050_ADDR   0x68 // Default MPU6050 I2C address

bool mpu_found = false;

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

void tcaDisableAll() {
  Wire.beginTransmission(TCA_FRONT_ADDR);
  Wire.write(0);
  Wire.endTransmission();
  Wire.beginTransmission(TCA_BACK_ADDR);
  Wire.write(0);
  Wire.endTransmission();
}

bool checkDevice(uint8_t address) {
  Wire.beginTransmission(address);
  return (Wire.endTransmission() == 0);
}

void setupMPU6050() {
  tcaDisableAll(); // Ensure we are talking directly to the main spine
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(0x6B); // PWR_MGMT_1 register
  Wire.write(0x00); // Write 0 to wake up the MPU6050
  Wire.endTransmission();
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  
  Wire.begin();
  Wire.setClock(100000); 

  Serial.println("\n=== V3 PCB DIAGNOSTIC SCANNER ===");
  Serial.println("Scanning Main I2C Spine...");

  if (checkDevice(PCA9685_ADDR)) Serial.println("[OK] PCA9685 Servo Driver found (0x40)");
  else Serial.println("[FAIL] PCA9685 missing!");

  if (checkDevice(TCA_FRONT_ADDR)) Serial.println("[OK] Front TCA9548A found (0x70)");
  else Serial.println("[FAIL] Front TCA missing!");

  if (checkDevice(TCA_BACK_ADDR)) Serial.println("[OK] Back TCA9548A found (0x71)");
  else Serial.println("[FAIL] Back TCA missing!");

  if (checkDevice(MPU6050_ADDR)) {
    Serial.println("[OK] MPU6050 IMU found (0x68)");
    mpu_found = true;
    setupMPU6050();
  } else {
    Serial.println("[FAIL] MPU6050 missing!");
  }

  Serial.println("\nScanning Encoders (Front TCA - 0x70)...");
  for (uint8_t ch = 0; ch < 8; ch++) {
    tcaSelect(TCA_FRONT_ADDR, ch);
    if (checkDevice(AS5600_ADDR)) {
      Serial.print("  -> Channel "); Serial.print(ch); Serial.println(": [OK] AS5600 Found");
    } else {
      Serial.print("  -> Channel "); Serial.print(ch); Serial.println(": Empty");
    }
  }

  Serial.println("\nScanning Encoders (Back TCA - 0x71)...");
  for (uint8_t ch = 0; ch < 8; ch++) {
    tcaSelect(TCA_BACK_ADDR, ch);
    if (checkDevice(AS5600_ADDR)) {
      Serial.print("  -> Channel "); Serial.print(ch); Serial.println(": [OK] AS5600 Found");
    } else {
      Serial.print("  -> Channel "); Serial.print(ch); Serial.println(": Empty");
    }
  }
  
  Serial.println("\nScan Complete. Starting IMU Readout...\n");
  delay(2000);
}

void loop() {
  if (mpu_found) {
    tcaDisableAll(); // Isolate the main bus for clean IMU reading
    
    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(0x3B); // Start with register 0x3B (ACCEL_XOUT_H)
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)MPU6050_ADDR, (uint8_t)14, (uint8_t)true); // Request 14 bytes
    
    if (Wire.available() == 14) {
      int16_t acX = Wire.read() << 8 | Wire.read();
      int16_t acY = Wire.read() << 8 | Wire.read();
      int16_t acZ = Wire.read() << 8 | Wire.read();
      int16_t tmp = Wire.read() << 8 | Wire.read(); // Temperature (ignored here)
      int16_t gyX = Wire.read() << 8 | Wire.read();
      int16_t gyY = Wire.read() << 8 | Wire.read();
      int16_t gyZ = Wire.read() << 8 | Wire.read();

      Serial.print("Accel [X: "); Serial.print(acX);
      Serial.print(" Y: "); Serial.print(acY);
      Serial.print(" Z: "); Serial.print(acZ);
      Serial.print("]  Gyro [X: "); Serial.print(gyX);
      Serial.print(" Y: "); Serial.print(gyY);
      Serial.print(" Z: "); Serial.print(gyZ);
      Serial.println("]");
    }
  }
  delay(200); // Read 5 times a second for readability in the monitor
}