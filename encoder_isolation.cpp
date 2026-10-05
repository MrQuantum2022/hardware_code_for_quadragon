#include <Arduino.h>
#include <Wire.h>

// --- I2C CONFIGURATION ---
#define TCA_FRONT_ADDR 0x70
#define TCA_BACK_ADDR  0x71
#define AS5600_ADDR    0x36

int current_joint = 0; // Start by watching Joint 0

// --- HARDWARE FUNCTIONS ---
void tcaSelect(uint8_t tca_addr, uint8_t channel) {
  if (channel > 7) return;
  Wire.beginTransmission(tca_addr);
  Wire.write(1 << channel);
  Wire.endTransmission();
}

uint16_t readAS5600() {
  Wire.beginTransmission(AS5600_ADDR);
  Wire.write(0x0E); // Point to the RAW ANGLE register
  if (Wire.endTransmission(false) != 0) return 0xFFFF; // Error code if offline
  
  Wire.requestFrom((uint8_t)AS5600_ADDR, (uint8_t)2);
  if (Wire.available() == 2) {
    uint16_t high = Wire.read();
    uint16_t low = Wire.read();
    return (high << 8) | low;
  }
  return 0xFFFF; 
}

void setup() {
  Serial.begin(115200);
  
  // Wait for you to open the PlatformIO Serial Monitor
  while (!Serial) delay(10); 

  Wire.begin();
  
  // CRITICAL: Force 100kHz standard speed to prevent bit-flipping noise on long wires
  Wire.setClock(400000); 

  Serial.println("\n=== ENCODER ISOLATION TEST ===");
  Serial.println("Type 'n' for Next Joint, 'p' for Previous Joint");
  Serial.println("================================\n");
  delay(1000);
}

void loop() {
  // 1. Check for keyboard input to switch joints
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'n' || c == 'N') {
      current_joint++;
      if (current_joint > 11) current_joint = 0;
      Serial.print("\n>>> SWITCHED TO JOINT "); 
      Serial.println(current_joint);
    } 
    else if (c == 'p' || c == 'P') {
      current_joint--;
      if (current_joint < 0) current_joint = 11;
      Serial.print("\n>>> SWITCHED TO JOINT "); 
      Serial.println(current_joint);
    }
  }

  // 2. Route I2C to the correct TCA and Channel
  uint8_t tca = (current_joint < 6) ? TCA_FRONT_ADDR : TCA_BACK_ADDR;
  
  // Assuming your physical wires start at Channel 2 on the TCA
  uint8_t channel = (current_joint % 6) + 2; 
  
  tcaSelect(tca, channel);
  
  // 3. Read the targeted sensor
  uint16_t raw_angle = readAS5600();

  // 4. Print continuously
  Serial.print("Watching Joint [");
  Serial.print(current_joint);
  Serial.print("] (TCA 0x"); Serial.print(tca, HEX);
  Serial.print(" CH "); Serial.print(channel);
  Serial.print(") -> Raw Value: ");
  
  if (raw_angle == 0xFFFF) {
    Serial.println("OFFLINE / DISCONNECTED");
  } else {
    float degrees = (raw_angle / 4096.0) * 360.0;
    Serial.println(degrees, 2);//
  }

  // Wait 50ms so the terminal remains readable (20Hz refresh)
  delay(50);
}