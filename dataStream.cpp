#include <Arduino.h>
#include <Wire.h>

// --- I2C CONFIGURATION ---
#define TCA_FRONT_ADDR 0x70
#define TCA_BACK_ADDR  0x71
#define AS5600_ADDR    0x36

// --- TELEMETRY PROTOCOL ---
// #pragma pack(push, 1) ensures the C++ compiler doesn't pad the bytes,
// matching the Python struct.unpack format exactly.
#pragma pack(push, 1) 
struct TelemetryPacket {
  uint16_t header;       // Magic bytes (0xBBAA) for Python alignment
  uint16_t encoders[12]; // Raw AS5600 readings (0 - 4095)
  int16_t accel[3];      // MPU6050 Accelerometer (X, Y, Z)
  int16_t gyro[3];       // MPU6050 Gyroscope (X, Y, Z)
  uint8_t checksum;      // XOR checksum to catch corrupted USB frames
};
#pragma pack(pop)

TelemetryPacket telemetry;
unsigned long last_telemetry_time = 0;

// --- HARDWARE FUNCTIONS ---

void tcaSelect(uint8_t target_tca, uint8_t channel) {
  if (channel > 7) return;

  // 1. Identify which TCA we are NOT using right now
  uint8_t idle_tca = (target_tca == TCA_FRONT_ADDR) ? TCA_BACK_ADDR : TCA_FRONT_ADDR;
  
  // 2. Tell the idle TCA to close all of its channels (send a 0)
  Wire.beginTransmission(idle_tca);
  Wire.write(0); 
  Wire.endTransmission();

  // 3. Tell the target TCA to open the specific channel we want
  Wire.beginTransmission(target_tca);
  Wire.write(1 << channel);
  Wire.endTransmission();
}
uint16_t readAS5600() {
  Wire.beginTransmission(AS5600_ADDR);
  Wire.write(0x0E); // Point to the RAW ANGLE register
  if (Wire.endTransmission(false) != 0) return 0xFFFF; 
  
  Wire.requestFrom((uint8_t)AS5600_ADDR, (uint8_t)2);
  if (Wire.available() == 2) {
    uint16_t high = Wire.read();
    uint16_t low = Wire.read();
    return (high << 8) | low;
  }
  return 0xFFFF; // Return error code if sensor disconnects
}

void sendTelemetry() {
  telemetry.header = 0xBBAA; // 0xAA 0xBB in little-endian
  
  // Calculate XOR checksum of everything except the checksum byte itself
  telemetry.checksum = 0;
  uint8_t* ptr = (uint8_t*)&telemetry;
  for (size_t i = 0; i < sizeof(TelemetryPacket) - 1; i++) {
    telemetry.checksum ^= ptr[i];
  }
  
  // Blast the 39 bytes over USB
  Serial.write((uint8_t*)&telemetry, sizeof(TelemetryPacket));
  Serial.flush();
}

// --- MAIN SETUP & LOOP ---

void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(400000); // 400kHz Fast Mode I2C
  
  // Initialize IMU arrays to zero (can add actual MPU6050 reads later)
  for(int i=0; i<3; i++) {
    telemetry.accel[i] = 0;
    telemetry.gyro[i] = 0;
  }
}

void loop() {
  // Fire exactly every 10ms (100Hz) to match standard RL control loops
  if (millis() - last_telemetry_time >= 10) {
    last_telemetry_time = millis();

    // Read all 12 joints sequentially
    for (int i = 0; i < 12; i++) {
      // Joints 0-5 on Front TCA, Joints 6-11 on Back TCA
      uint8_t tca = (i < 6) ? TCA_FRONT_ADDR : TCA_BACK_ADDR;
      
      // Assuming you wired them to TCA channels 2 through 7
      uint8_t channel = (i % 6) + 2; 
      
      tcaSelect(tca, channel);
      telemetry.encoders[i] = readAS5600();
    }

    sendTelemetry();
  }
}