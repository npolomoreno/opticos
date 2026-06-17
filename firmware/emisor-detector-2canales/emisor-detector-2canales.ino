#include <Arduino.h>
#include "SAMD_PWM.h"
#include <math.h>

// =====================================================
// Emisor IR
// =====================================================
const uint8_t PWM_PIN = 9;
const float PWM_FREQ_HZ = 5000.0f;
const float PWM_DUTY_ON = 50.0f;
const float PWM_DUTY_OFF = 0.0f;
SAMD_PWM *pwm = nullptr;

// =====================================================
// ADC
// =====================================================
static const float VREF = 3.3000f;
static const float LSB = VREF / 4095.0f;

// Offsets medidos con entrada a GND. Ajustar con una medicion real.
static const float A0_OFFSET_V = 0.00634f;
static const float A1_OFFSET_V = 0.00634f;

// Ratio A0/A1 medido en gas cero o aire limpio.
// Dejar en 0.0f para reportar ratio sin calcular absorbancia relativa.
static const float BASELINE_RATIO_A0_A1 = 0.0f;

// =====================================================
// Configuracion de adquisicion
// =====================================================
const uint16_t N_AVG = 256;
const uint32_t PRINT_MS = 250;
const uint16_t SAMPLE_DELAY_US = 10;
const uint16_t EMITTER_SETTLE_MS = 8;

struct ChannelStats {
  uint32_t acc;
  uint16_t mn;
  uint16_t mx;
};

struct Frame {
  ChannelStats a0Off;
  ChannelStats a0On;
  ChannelStats a1Off;
  ChannelStats a1On;
};

static inline uint16_t readADC(uint8_t pin) {
  (void)analogRead(pin);
  return (uint16_t)analogRead(pin);
}

static inline void resetStats(ChannelStats &s) {
  s.acc = 0;
  s.mn = 4095;
  s.mx = 0;
}

static inline void addSample(ChannelStats &s, uint16_t x) {
  s.acc += x;
  if (x < s.mn) s.mn = x;
  if (x > s.mx) s.mx = x;
}

static inline float avgVolts(const ChannelStats &s) {
  return (s.acc / (float)N_AVG) * LSB;
}

static inline float minVolts(const ChannelStats &s) {
  return s.mn * LSB;
}

static inline float maxVolts(const ChannelStats &s) {
  return s.mx * LSB;
}

static void setEmitterDuty(float duty) {
  if (pwm != nullptr) {
    pwm->setPWM(PWM_PIN, PWM_FREQ_HZ, duty);
  }
}

static void samplePair(ChannelStats &a0, ChannelStats &a1) {
  resetStats(a0);
  resetStats(a1);

  for (uint16_t i = 0; i < N_AVG; i++) {
    addSample(a0, readADC(A0));
    addSample(a1, readADC(A1));

    if (SAMPLE_DELAY_US > 0) {
      delayMicroseconds(SAMPLE_DELAY_US);
    }
  }
}

static Frame acquireFrame() {
  Frame f;

  // Fase oscura: estima offset, luz ambiente y deriva electronica lenta.
  setEmitterDuty(PWM_DUTY_OFF);
  delay(EMITTER_SETTLE_MS);
  samplePair(f.a0Off, f.a1Off);

  // Fase iluminada: misma lectura con emisor activo.
  setEmitterDuty(PWM_DUTY_ON);
  delay(EMITTER_SETTLE_MS);
  samplePair(f.a0On, f.a1On);

  return f;
}

static float safeRatio(float num, float den) {
  if (fabs(den) < 1.0e-6f) {
    return NAN;
  }
  return num / den;
}

static float relativeAbsorbance(float ratio) {
  if (BASELINE_RATIO_A0_A1 <= 0.0f || isnan(ratio) || ratio <= 0.0f) {
    return NAN;
  }
  return -log(ratio / BASELINE_RATIO_A0_A1);
}

void setup() {
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && (millis() - t0 < 1500)) {}

  pwm = new SAMD_PWM(PWM_PIN, PWM_FREQ_HZ, PWM_DUTY_ON);
  pwm->setPWM();

  analogReadResolution(12);

  (void)analogRead(A0);
  (void)analogRead(A1);
  delay(50);

  Serial.println("MODE,synchronous_on_off");
  Serial.println("PWM_PIN,DUTY_ON_PCT,FREQ_HZ,N_AVG,SETTLE_MS,SAMPLE_DELAY_US");
  Serial.print(PWM_PIN);
  Serial.print(",");
  Serial.print(PWM_DUTY_ON, 2);
  Serial.print(",");
  Serial.print(PWM_FREQ_HZ, 1);
  Serial.print(",");
  Serial.print(N_AVG);
  Serial.print(",");
  Serial.print(EMITTER_SETTLE_MS);
  Serial.print(",");
  Serial.println(SAMPLE_DELAY_US);
  Serial.println("t_ms,a0_off_v,a0_on_v,a0_net_v,a0_net_corr_v,a0_min_on_v,a0_max_on_v,a1_off_v,a1_on_v,a1_net_v,a1_net_corr_v,a1_min_on_v,a1_max_on_v,ratio_a0_a1,abs_rel");
}

void loop() {
  static uint32_t tp = 0;
  uint32_t now = millis();

  if ((uint32_t)(now - tp) < PRINT_MS) {
    return;
  }
  tp = now;

  Frame f = acquireFrame();

  float a0Off = avgVolts(f.a0Off);
  float a0On = avgVolts(f.a0On);
  float a0Net = a0On - a0Off;
  float a0NetCorr = a0Net - A0_OFFSET_V;

  float a1Off = avgVolts(f.a1Off);
  float a1On = avgVolts(f.a1On);
  float a1Net = a1On - a1Off;
  float a1NetCorr = a1Net - A1_OFFSET_V;

  float ratio = safeRatio(a0NetCorr, a1NetCorr);
  float absorbance = relativeAbsorbance(ratio);

  Serial.print(now);
  Serial.print(",");
  Serial.print(a0Off, 6);
  Serial.print(",");
  Serial.print(a0On, 6);
  Serial.print(",");
  Serial.print(a0Net, 6);
  Serial.print(",");
  Serial.print(a0NetCorr, 6);
  Serial.print(",");
  Serial.print(minVolts(f.a0On), 6);
  Serial.print(",");
  Serial.print(maxVolts(f.a0On), 6);
  Serial.print(",");
  Serial.print(a1Off, 6);
  Serial.print(",");
  Serial.print(a1On, 6);
  Serial.print(",");
  Serial.print(a1Net, 6);
  Serial.print(",");
  Serial.print(a1NetCorr, 6);
  Serial.print(",");
  Serial.print(minVolts(f.a1On), 6);
  Serial.print(",");
  Serial.print(maxVolts(f.a1On), 6);
  Serial.print(",");

  if (isnan(ratio)) {
    Serial.print("nan");
  } else {
    Serial.print(ratio, 6);
  }
  Serial.print(",");

  if (isnan(absorbance)) {
    Serial.println("nan");
  } else {
    Serial.println(absorbance, 6);
  }
}
