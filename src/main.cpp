#include <Arduino.h>
#include <Wire.h>
#include "HX711.h"
#include "apwifiesp32.h" // WiFi y servidor web del profesor, ampliado con dashboard

// --- Definición de Pines ---
const int PIN_HX_DT = 19;
const int PIN_HX_SCK = 18;
const int PIN_TOUCH_CENTRO = 4;
const int PIN_TOUCH_IZQ = 2;
const int PIN_TOUCH_DER = 15;
const int PIN_BUZZER = 23;
const int PIN_LED_VERDE = 5;
const int PIN_LED_ROJO = 14;

// --- Objetos y Variables ---
HX711 celda;
const int MPU_ADDR = 0x68;
const int UMBRAL_TOUCH = 800;
unsigned long tiempoAnterior = 0;

// --- Variables para evaluar las compresiones ---
int compresiones = 0;
bool comprimiendo = false;
unsigned long tiempo_ultima_compresion = 0;

void setup() {
  Serial.begin(115200);

  // 1. Iniciar WiFi AP y servidor del dashboard
  initAP("Simulador_RCP_Grupo2", "123456789");

  // 2. Iniciar I2C para MPU6050
  Wire.begin(21, 22);
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);

  // 3. Iniciar HX711 en Canal B y Encerar (ORIGINAL)
  Serial.println("Configurando celda de carga en Canal B...");
  celda.begin(PIN_HX_DT, PIN_HX_SCK);
  celda.set_gain(32);
  delay(500);
  celda.tare();

  // 4. Configurar Buzzer y LEDs (ORIGINAL)
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED_VERDE, OUTPUT);
  pinMode(PIN_LED_ROJO, OUTPUT);

  digitalWrite(PIN_LED_ROJO, HIGH);
  digitalWrite(PIN_LED_VERDE, HIGH);
  digitalWrite(PIN_BUZZER, HIGH);
  delay(500);
  digitalWrite(PIN_LED_ROJO, LOW);
  digitalWrite(PIN_LED_VERDE, LOW);
  digitalWrite(PIN_BUZZER, LOW);
  Serial.println("Sistema Listo!");
}

void loop() {
  // --- Servicio del WiFi y dashboard ---
  loopAP();

  if (millis() - tiempoAnterior >= 50) {
    tiempoAnterior = millis();

    // 1. Leer Matriz Capacitiva (ORIGINAL)
    int toque_centro = touchRead(PIN_TOUCH_CENTRO);
    int toque_izq = touchRead(PIN_TOUCH_IZQ);
    int toque_der = touchRead(PIN_TOUCH_DER);

    bool mala_postura = (toque_izq < UMBRAL_TOUCH) || (toque_der < UMBRAL_TOUCH);

    if (mala_postura) {
      digitalWrite(PIN_BUZZER, HIGH);
    } else {
      digitalWrite(PIN_BUZZER, LOW);
    }

    // 2. Leer Celda de Carga (ORIGINAL)
    long fuerza = 0;
    if (celda.is_ready()) {
      fuerza = celda.get_value(1);
      if (fuerza > -6000 && fuerza < 6000) {
        fuerza = 0;
      }
    }

    // 3. Leer MPU6050 (ORIGINAL)
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x3F);
    Wire.endTransmission(false);
    Wire.requestFrom(MPU_ADDR, 2, true);
    int16_t acel_Z = Wire.read() << 8 | Wire.read();

    // LÓGICA DE COMPRESIÓN (ORIGINAL)
    if (abs(fuerza) > 15000 && !comprimiendo) {
      comprimiendo = true;
      compresiones++;
      tiempo_ultima_compresion = millis();

      digitalWrite(PIN_BUZZER, HIGH);
      delay(30);
      digitalWrite(PIN_BUZZER, LOW);
    }
    else if (abs(fuerza) < 5000) {
      comprimiendo = false;
    }

    // LÓGICA DE LEDs DE RITMO (ORIGINAL)
    if (millis() - tiempo_ultima_compresion > 1000) {
      digitalWrite(PIN_LED_ROJO, HIGH);
      digitalWrite(PIN_LED_VERDE, LOW);
    } else {
      digitalWrite(PIN_LED_ROJO, LOW);
      digitalWrite(PIN_LED_VERDE, HIGH);
    }

    // ÚNICA ADICIÓN: enviar las lecturas existentes al servidor web.
    // No cambia las mediciones ni las condiciones de los sensores.
    datosRCP.fuerza = fuerza;
    datosRCP.aceleracionZ = acel_Z;
    datosRCP.compresiones = compresiones;
    datosRCP.malaPostura = mala_postura;
    datosRCP.centroActivo = (toque_centro < UMBRAL_TOUCH);
    // El código original no calcula compresiones/min; no inventar un valor.
    datosRCP.ritmo = 0;

    // 4. Imprimir variables para Teleplot (ORIGINAL)
    Serial.print(">Fuerza:"); Serial.println(fuerza);
    Serial.print(">AcelZ:"); Serial.println(acel_Z);
    Serial.print(">Compresiones:"); Serial.println(compresiones * 1000);
  }
}
