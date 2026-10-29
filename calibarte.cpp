#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// --- HARDWARE ADDRESSES ---
#define TCA_FRONT_ADDR 0x70
#define TCA_BACK_ADDR  0x71
#define AS5600_ADDR    0x36

// --- OBJECTS ---
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);
Adafruit_MPU6050 mpu;

// --- SERVO CONFIGURATION ---
#define SERVO_MIN     150 // Minimum pulse length (0 to 4096)
#define SERVO_MAX     600 // Maximum pulse length (0 to 4096)
#define SERVO_CENTER  375 // Default center position
#define STEP_SIZE     10  // How much the servo moves per 'W' or 'S' press

// --- STATE VARIABLES ---
int current_joint = 0;              // Tracks which joint we are controlling (0-11)
int servo_pulses[12];               // Tracks the current pulse value for each servo
int servo_pins[12] = {0, 2,4, 5, 6, 7, 8, 9, 10, 12, 13, 15}; // Map to PCA9685 pins

// --- I2C ROUTINES ---

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
  if (Wire.endTransmission(false) != 0) return 0xFFFF; // Sensor offline
  
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
  
  // 1. ADD THIS: Wait for you to open the monitor
  while (!Serial) delay(10); 

  Wire.begin();
  
  // 2. CHANGE THIS: Drop to 100kHz so the AS5600s don't spike
  Wire.setClock(100000); 

  // 1. Initialize PCA9685
  pwm.begin();
  pwm.setOscillatorFrequency(27000000);
  pwm.setPWMFreq(50);  // Standard analog servos run at 50Hz

  // Center all servos on boot
  for (int i = 0; i < 12; i++) {
    servo_pulses[i] = SERVO_CENTER;
    pwm.setPWM(servo_pins[i], 0, servo_pulses[i]);
  }

  // 2. Initialize IMU
  if (!mpu.begin()) {
    Serial.println("WARNING: MPU6050 not detected!");
  } else {
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  }

  // 3. Print Menu
  Serial.println("\n=== QUADRAGON HARDWARE DEBUG ===");
  Serial.println("[A] / [D] : Select Joint (0-11)");
  Serial.println("[W] / [S] : Move Selected Servo");
  Serial.println("[SPACE]   : Re-center Selected Servo");
  Serial.println("[P]       : Print Joint & IMU Status");
  Serial.println("================================\n");
}

void loop() {
  // Non-blocking keyboard parser
  if (Serial.available()) {
    char c = Serial.read();

    // Select Previous Joint
    if (c == 'a' || c == 'A') {
      current_joint--;
      if (current_joint < 0) current_joint = 11;
      Serial.print(">>> Selected Joint: "); 
      Serial.println(current_joint);
    }
    
    // Select Next Joint
    else if (c == 'd' || c == 'D') {
      current_joint++;
      if (current_joint > 11) current_joint = 0;
      Serial.print(">>> Selected Joint: "); 
      Serial.println(current_joint);
    }

    // Move Servo Forward
    else if (c == 'w' || c == 'W') {
      servo_pulses[current_joint] += STEP_SIZE;
      if (servo_pulses[current_joint] > SERVO_MAX) servo_pulses[current_joint] = SERVO_MAX;
      pwm.setPWM(servo_pins[current_joint], 0, servo_pulses[current_joint]);
      Serial.print("Moved Joint "); Serial.print(current_joint);
      Serial.print(" -> Pulse: "); Serial.println(servo_pulses[current_joint]);
    }

    // Move Servo Backward
    else if (c == 's' || c == 'S') {
      servo_pulses[current_joint] -= STEP_SIZE;
      if (servo_pulses[current_joint] < SERVO_MIN) servo_pulses[current_joint] = SERVO_MIN;
      pwm.setPWM(servo_pins[current_joint], 0, servo_pulses[current_joint]);
      Serial.print("Moved Joint "); Serial.print(current_joint);
      Serial.print(" -> Pulse: "); Serial.println(servo_pulses[current_joint]);
    }

    // Emergency Center
    else if (c == ' ') {
      servo_pulses[current_joint] = SERVO_CENTER;
      pwm.setPWM(servo_pins[current_joint], 0, servo_pulses[current_joint]);
      Serial.println(">>> Centered Selected Joint!");
    }

    // Print Telemetry for Selected Joint
    else if (c == 'p' || c == 'P') {
      // 1. Route I2C to the correct TCA and Channel
      uint8_t tca = (current_joint < 6) ? TCA_FRONT_ADDR : TCA_BACK_ADDR;
      uint8_t channel = (current_joint % 6) + 2; // Assuming sensors are on channels 2-7
      tcaSelect(tca, channel);
      
      // 2. Read Sensor
      uint16_t enc = readAS5600();

      // 3. Read IMU
      sensors_event_t a, g, temp;
      mpu.getEvent(&a, &g, &temp);

      // 4. Print Data
      Serial.println("\n--- STATUS ---");
      Serial.print("Target Joint   : "); Serial.println(current_joint);
      Serial.print("Servo (Pulse)  : "); Serial.println(servo_pulses[current_joint]);
      Serial.print("AS5600 (Raw)   : "); Serial.println(enc);
      Serial.print("IMU Accel (XYZ): "); 
      Serial.print(a.acceleration.x); Serial.print(", ");
      Serial.print(a.acceleration.y); Serial.print(", ");
      Serial.println(a.acceleration.z);
      Serial.println("--------------\n");
    }
  }
}