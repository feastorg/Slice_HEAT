// SLC_HEAT: two-channel heater controller (R1 11-2026 board).
//
// Pin map (A1, Arduino Nano):
//   D11 PWM1   heater 1 low-side gate (Q2), analogWrite
//   D10 PWM2   heater 2 low-side gate (Q4), analogWrite
//   D12 HTR_EN high = high-side P-FETs may conduct (with ESTOP_BUF high)
//   D2  ESTOP  input, buffered bus E_STOP: high = run, low = heaters off
//   D9  CLK, D7 SO, D8 CS1 (thermocouple 1), D6 CS2 (thermocouple 2):
//       MAX31855K pair, read by software SPI through the 3.3 V shifters
//
// HTR_EN drops, and both PWMs go to 0, on a thermocouple fault (three
// bad reads in a row), on either channel above MAXTEMP, or with E_STOP
// low. A thermocouple or over-temperature fault latches until a new
// command arrives.
//
// E_STOP low (polled each tick, and a falling edge on D2/INT0 between
// ticks) latches an E-stop: heaters off and both targets set to 0.
// The latch holds after E_STOP returns high, and commands are ignored,
// until a clear ("C" on serial, the byte 0x43 on I2C) with E_STOP high,
// or a reset. A clear leaves the targets at 0: send a new command to heat.
//
// Commands
//   serial: "<ch>,<target C>,<ramp C/min>\n" (ch 1 or 2, 0 = both);
//   "C\n" clears the E-stop latch
//   I2C (address 6): two floats, target and ramp, applied to both
//   channels; one byte 0x43 clears the E-stop latch; a read returns
//   channel 1's temperature and setpoint.
#include <Wire.h>

#define I2C_ADR 6
#define MAXTEMP 100.0

const uint8_t PIN_PWM[2] = {11, 10};
const uint8_t PIN_CS[2] = {8, 6};
const uint8_t PIN_HTR_EN = 12;
const uint8_t PIN_ESTOP = 2;
const uint8_t PIN_CLK = 9;
const uint8_t PIN_SO = 7;

const float KP = 20.0;  // counts per C
const float KI = 0.5;   // counts per C per second

typedef union {
  float number;
  uint8_t bytes[4];
} FloatBytes;

float temp[2] = {NAN, NAN};
float setpoint[2] = {0, 0};
float target[2] = {0, 0};
float ramp[2] = {0, 0};   // C per minute, 0 = step
float integ[2] = {0, 0};
uint8_t duty[2] = {0, 0};
uint8_t badReads[2] = {0, 0};
bool fault = false;
volatile bool estop = false;  // latched by E_STOP low; cleared by clearEstop() or a reset

volatile bool i2cNew = false;
volatile bool i2cClear = false;
volatile float i2cTarget, i2cRamp;
String line;

// MAX31855: 32 bits, MSB first; D16 = fault, D31..D18 = 14-bit signed, 0.25 C.
float readTC(uint8_t cs) {
  uint32_t d = 0;
  digitalWrite(PIN_CLK, LOW);
  digitalWrite(cs, LOW);
  delayMicroseconds(10);
  for (int8_t i = 31; i >= 0; i--) {
    digitalWrite(PIN_CLK, LOW);
    delayMicroseconds(10);
    d <<= 1;
    if (digitalRead(PIN_SO)) d |= 1;
    digitalWrite(PIN_CLK, HIGH);
    delayMicroseconds(10);
  }
  digitalWrite(cs, HIGH);
  if (d & 0x10000UL) return NAN;
  return ((int32_t)d >> 18) * 0.25;
}

void allOff() {
  digitalWrite(PIN_HTR_EN, LOW);
  for (uint8_t c = 0; c < 2; c++) {
    duty[c] = 0;
    integ[c] = 0;
    analogWrite(PIN_PWM[c], 0);
  }
}

void command(int ch, float t, float r) {
  if (estop) return;
  for (uint8_t c = 0; c < 2; c++) {
    if (ch != 0 && ch != c + 1) continue;
    target[c] = t;
    ramp[c] = r;
    if (r <= 0 || isnan(temp[c])) setpoint[c] = t;
    else setpoint[c] = temp[c];
  }
  fault = false;
}

void estopISR() {
  estop = true;
}

void clearEstop() {
  if (digitalRead(PIN_ESTOP) == HIGH) estop = false;
}

void receiveEvent(int n) {
  FloatBytes a, b;
  for (int i = 0; i < n; i++) {
    uint8_t v = Wire.read();
    if (i < 4) a.bytes[i] = v;
    else if (i < 8) b.bytes[i - 4] = v;
  }
  if (n == 1 && a.bytes[0] == 'C') i2cClear = true;
  if (n >= 8) {
    i2cTarget = a.number;
    i2cRamp = b.number;
    i2cNew = true;
  }
}

void requestEvent() {
  FloatBytes a, b;
  a.number = temp[0];
  b.number = setpoint[0];
  Wire.write(a.bytes, 4);
  Wire.write(b.bytes, 4);
}

void setup() {
  pinMode(PIN_HTR_EN, OUTPUT);
  digitalWrite(PIN_HTR_EN, LOW);
  for (uint8_t c = 0; c < 2; c++) {
    pinMode(PIN_PWM[c], OUTPUT);
    analogWrite(PIN_PWM[c], 0);
    pinMode(PIN_CS[c], OUTPUT);
    digitalWrite(PIN_CS[c], HIGH);
  }
  pinMode(PIN_CLK, OUTPUT);
  pinMode(PIN_SO, INPUT);
  pinMode(PIN_ESTOP, INPUT);  // R17 1M pulls it low: unwired = off
  attachInterrupt(digitalPinToInterrupt(PIN_ESTOP), estopISR, FALLING);  // a press shorter than a loop tick still latches

  Serial.begin(115200);
  line.reserve(40);
  Wire.begin(I2C_ADR);
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);
}

void loop() {
  static unsigned long last = 0;
  if (millis() - last < 250) return;
  last += 250;
  const float dt = 0.25;

  if (i2cNew) {
    noInterrupts();
    float t = i2cTarget, r = i2cRamp;
    i2cNew = false;
    interrupts();
    command(0, t, r);
  }
  if (i2cClear) {
    i2cClear = false;
    clearEstop();
  }

  for (uint8_t c = 0; c < 2; c++) {
    float v = readTC(PIN_CS[c]);
    if (isnan(v)) {
      if (badReads[c] < 255) badReads[c]++;
    } else {
      badReads[c] = 0;
      temp[c] = v;
    }
    if (badReads[c] >= 3 || temp[c] > MAXTEMP) fault = true;
  }

  if (digitalRead(PIN_ESTOP) == LOW) estop = true;
  if (estop) {
    for (uint8_t c = 0; c < 2; c++) {
      target[c] = 0;
      setpoint[c] = 0;
    }
  }
  if (fault || estop) {
    allOff();
  } else {
    digitalWrite(PIN_HTR_EN, HIGH);
    for (uint8_t c = 0; c < 2; c++) {
      if (ramp[c] > 0 && setpoint[c] != target[c]) {
        float step = ramp[c] / 60.0 * dt;
        if (setpoint[c] < target[c]) setpoint[c] = min(setpoint[c] + step, target[c]);
        else setpoint[c] = max(setpoint[c] - step, target[c]);
      }
      float err = setpoint[c] - temp[c];
      if (target[c] <= 0 || isnan(temp[c])) {
        integ[c] = 0;
        duty[c] = 0;
      } else {
        integ[c] = constrain(integ[c] + KI * err * dt, 0, 255);
        duty[c] = (uint8_t)constrain(KP * err + integ[c], 0, 255);
      }
      analogWrite(PIN_PWM[c], duty[c]);
    }
  }

  static uint8_t n = 0;
  if (++n >= 4) {  // once a second: ch1 sp, temp, duty%, ch2 sp, temp, duty%, flags
    n = 0;
    for (uint8_t c = 0; c < 2; c++) {
      Serial.print(setpoint[c]);
      Serial.print(',');
      Serial.print(temp[c]);
      Serial.print(',');
      Serial.print(duty[c] * 100 / 255);
      Serial.print(',');
    }
    Serial.println(estop ? "ESTOP" : (fault ? "FAULT" : "RUN"));
  }

  while (Serial.available()) {
    char ch = (char)Serial.read();
    if (ch != '\n') {
      if (line.length() < 39) line += ch;
      continue;
    }
    int a = line.indexOf(','), b = line.indexOf(',', a + 1);
    if (line.charAt(0) == 'C') {
      clearEstop();
    } else if (a > 0 && b > a) {
      command(line.substring(0, a).toInt(), line.substring(a + 1, b).toFloat(), line.substring(b + 1).toFloat());
    }
    line = "";
  }
}
