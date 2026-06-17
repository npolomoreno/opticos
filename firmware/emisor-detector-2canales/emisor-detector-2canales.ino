#include <Arduino.h>
#include "SAMD_PWM.h"

// =====================================================
// PWM LED
// =====================================================
const uint8_t PWM_PIN  = 9;
const float   PWM_FREQ = 5000.0f;   // 5 kHz
const float   DUTY     = 50.0f;     // 50 %
SAMD_PWM *pwm = nullptr;

// =====================================================
// ADC
// =====================================================
static const float VREF = 3.3000f;
static const float LSB  = VREF / 4095.0f;

// Offset medido con A0 a GND (ajusta con tu promedio real)
static const float A0_OFFSET_V = 0.00634f;

// Offset medido con A1 a GND (ajusta con tu promedio real)
static const float A1_OFFSET_V = 0.00634f;

// =====================================================
// Configuración de lectura
// =====================================================
const uint16_t N_AVG    = 256;   // más estable que 256
const uint32_t PRINT_MS = 100;    // imprime cada 200 ms

// Usa una espera pequeña NO múltiplo del periodo de 5 kHz (200 us)
// Puedes poner 0 para probar sin pausa entre lecturas
const uint16_t SAMPLE_DELAY_US = 10;

// =====================================================
// Lectura robusta de A0
// =====================================================
static inline uint16_t readADC_A0() {
  // Primera lectura para asentar el sample/hold
  (void)analogRead(A0);
  return (uint16_t)analogRead(A0);
}

// =====================================================
// Lectura robusta de A1
// =====================================================
static inline uint16_t readADC_A1() {
  // Primera lectura para asentar el sample/hold
  (void)analogRead(A1);
  return (uint16_t)analogRead(A1);
}

void setup() {
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && (millis() - t0 < 1500)) {}

  // PWM 5 kHz en D9
  pwm = new SAMD_PWM(PWM_PIN, PWM_FREQ, DUTY);
  pwm->setPWM();

  // ADC a 12 bits
  analogReadResolution(12);

  // Lectura dummy inicial
  (void)analogRead(A0);
  (void)analogRead(A1);
  delay(50);

  Serial.println("A0_DC clean (PWM running, sin TC5/REF/demod).");
  Serial.println("t_ms | A0_DC=... V | A0_DC_CORR=... V | A0_MIN=... V | A0_MAX=... V | A1_DC=... V | A1_DC_CORR=... V | A1_MIN=... V | A1_MAX=... V");
}

void loop() {
  static uint32_t tp = 0;
  uint32_t now = millis();

  if ((uint32_t)(now - tp) >= PRINT_MS) {
    tp = now;

    uint32_t acc0 = 0;
    uint16_t mn0 = 4095;
    uint16_t mx0 = 0;

    uint32_t acc1 = 0;
    uint16_t mn1 = 4095;
    uint16_t mx1 = 0;

    for (uint16_t i = 0; i < N_AVG; i++) {
      uint16_t x0 = readADC_A0();

      acc0 += x0;
      if (x0 < mn0) mn0 = x0;
      if (x0 > mx0) mx0 = x0;

      uint16_t x1 = readADC_A1();

      acc1 += x1;
      if (x1 < mn1) mn1 = x1;
      if (x1 > mx1) mx1 = x1;

      if (SAMPLE_DELAY_US > 0) {
        delayMicroseconds(SAMPLE_DELAY_US);
      }
    }

    float A0_dc      = (acc0 / (float)N_AVG) * LSB;
    float A0_min     = mn0 * LSB;
    float A0_max     = mx0 * LSB;
    float A0_dc_corr = A0_dc - A0_OFFSET_V;

    float A1_dc      = (acc1 / (float)N_AVG) * LSB;
    float A1_min     = mn1 * LSB;
    float A1_max     = mx1 * LSB;
    float A1_dc_corr = A1_dc - A1_OFFSET_V;

    // Salida consistente para parsear luego en Python/Excel
    Serial.print(now);
    Serial.print(" | ");

    Serial.print("A0_DC=");
    Serial.print(A0_dc, 6);
    Serial.print(" V | ");

    Serial.print("A0_DC_CORR=");
    Serial.print(A0_dc_corr, 6);
    Serial.print(" V | ");

   // Serial.print("A0_MIN=");
   // Serial.print(A0_min, 6);
   // Serial.print(" V | ");

    //Serial.print("A0_MAX=");
    //Serial.print(A0_max, 6);
    //Serial.print(" V | ");

    Serial.print("A1_DC=");
    Serial.print(A1_dc, 6);
    Serial.print(" V | ");

    Serial.print("A1_DC_CORR=");
    Serial.print(A1_dc_corr, 6);
    Serial.println(" V | ");

    //Serial.print("A1_MIN=");
    //Serial.print(A1_min, 6);
    //Serial.print(" V | ");

    //Serial.print("A1_MAX=");
    //Serial.print(A1_max, 6);
    //Serial.println(" V");
  }
}