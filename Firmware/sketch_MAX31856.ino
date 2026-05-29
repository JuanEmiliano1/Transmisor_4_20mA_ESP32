#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Adafruit_MAX31856.h>

// =====================================================
// PINES ESP32
// =====================================================

// SPI compartido
#define SPI_SCK   18
#define SPI_MISO  19
#define SPI_MOSI  23

// MAX31856
#define MAX_CS    32

// TFT ST7789
#define TFT_CS    27
#define TFT_DC    26
#define TFT_RST   25

// MCP4725 I2C
#define I2C_SDA   21
#define I2C_SCL   22

// =====================================================
// OBJETOS
// =====================================================

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// MAX31856 usando SPI hardware compartido
Adafruit_MAX31856 maxthermo = Adafruit_MAX31856(MAX_CS);

// =====================================================
// MCP4725
// =====================================================

const uint8_t MCP4725_ADDR = 0x60;

// =====================================================
// PARÁMETROS DEL SISTEMA
// =====================================================

const float TEMP_MIN_C = 0.0;
const float TEMP_MAX_C = 200.0;

const float I_MIN_mA = 4.0;
const float I_MAX_mA = 20.0;

const float RSENSE = 50.0;   // 100Ω || 100Ω = 50Ω

// Cambia a 3 si la pantalla se ve invertida o mal orientada
const uint8_t DISPLAY_ROTATION = 1;

// =====================================================
// FUNCIONES GENERALES
// =====================================================

float clampf(float x, float xmin, float xmax) {
  if (x < xmin) return xmin;
  if (x > xmax) return xmax;
  return x;
}

float tempToCurrentmA(float tempC) {
  tempC = clampf(tempC, TEMP_MIN_C, TEMP_MAX_C);

  return I_MIN_mA +
         ((tempC - TEMP_MIN_C) * (I_MAX_mA - I_MIN_mA)) /
         (TEMP_MAX_C - TEMP_MIN_C);
}

float currentToControlVoltage(float current_mA) {
  // V = I * R
  // 4 mA  * 50Ω = 0.2 V
  // 20 mA * 50Ω = 1.0 V
  return (current_mA / 1000.0) * RSENSE;
}

uint16_t voltsToDACCode(float voltage, float vref = 3.3) {
  voltage = clampf(voltage, 0.0, vref);
  return (uint16_t)((voltage / vref) * 4095.0);
}

bool writeDAC12(uint16_t value) {
  value &= 0x0FFF;

  Wire.beginTransmission(MCP4725_ADDR);
  Wire.write(0x40);
  Wire.write(value >> 4);
  Wire.write((value & 0x0F) << 4);

  return Wire.endTransmission() == 0;
}

// =====================================================
// PANTALLA
// =====================================================

void drawStaticUI() {
  digitalWrite(MAX_CS, HIGH);

  tft.fillScreen(ST77XX_BLACK);

  // Marco con margen para evitar recorte
  tft.drawRoundRect(6, 6, 308, 228, 8, ST77XX_WHITE);

  // Barra superior
  tft.fillRoundRect(12, 12, 296, 28, 6, ST77XX_BLUE);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(22, 18);
  tft.print("MAX31856 4-20mA");

  // Temperatura
  tft.drawRoundRect(14, 50, 292, 64, 8, ST77XX_CYAN);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(24, 60);
  tft.print("Temperatura");

  // Corriente
  tft.drawRoundRect(14, 124, 140, 50, 8, ST77XX_GREEN);
  tft.setCursor(24, 134);
  tft.print("Corriente");

  // Voltaje de control
  tft.drawRoundRect(166, 124, 140, 50, 8, ST77XX_YELLOW);
  tft.setCursor(176, 134);
  tft.print("V control");

  // Estado
  tft.drawRoundRect(14, 184, 292, 42, 8, ST77XX_MAGENTA);
  tft.setCursor(24, 194);
  tft.print("Estado");
}

void clearDynamicAreas() {
  tft.fillRect(24, 76, 260, 30, ST77XX_BLACK);
  tft.fillRect(24, 150, 118, 18, ST77XX_BLACK);
  tft.fillRect(176, 150, 118, 18, ST77XX_BLACK);
  tft.fillRect(24, 208, 270, 12, ST77XX_BLACK);
}

void updateDisplay(float tempC, float cjTemp, float loopmA, float vctrl, bool sensorOK, bool dacOK, uint8_t fault) {
  digitalWrite(MAX_CS, HIGH);

  clearDynamicAreas();

  if (sensorOK) {
    // Temperatura
    tft.setTextColor(ST77XX_YELLOW);
    tft.setTextSize(3);
    tft.setCursor(24, 78);
    tft.print(tempC, 1);
    tft.print(" C");

    // Corriente
    tft.setTextColor(ST77XX_GREEN);
    tft.setTextSize(2);
    tft.setCursor(24, 150);
    tft.print(loopmA, 2);
    tft.print("mA");

    // Voltaje de control
    tft.setTextColor(ST77XX_CYAN);
    tft.setCursor(176, 150);
    tft.print(vctrl, 3);
    tft.print("V");

    // Estado
    tft.setTextSize(1);
    tft.setCursor(24, 208);

    if (dacOK) {
      tft.setTextColor(ST77XX_GREEN);
      tft.print("OK | CJ=");
      tft.print(cjTemp, 1);
      tft.print("C");
    } else {
      tft.setTextColor(ST77XX_RED);
      tft.print("MAX OK | DAC ERROR");
    }

  } else {
    // Error
    tft.setTextColor(ST77XX_RED);
    tft.setTextSize(3);
    tft.setCursor(24, 78);
    tft.print("ERROR");

    tft.setTextSize(1);
    tft.setCursor(24, 208);
    tft.print("FAULT=0x");
    tft.print(fault, HEX);
  }
}

// =====================================================
// FALLAS MAX31856
// =====================================================

void printMAX31856Fault(uint8_t fault) {
  if (!fault) return;

  Serial.print("Falla MAX31856: 0x");
  Serial.println(fault, HEX);

  if (fault & MAX31856_FAULT_CJRANGE) Serial.println("CJ fuera de rango");
  if (fault & MAX31856_FAULT_TCRANGE) Serial.println("Termopar fuera de rango");
  if (fault & MAX31856_FAULT_CJHIGH)  Serial.println("Union fria alta");
  if (fault & MAX31856_FAULT_CJLOW)   Serial.println("Union fria baja");
  if (fault & MAX31856_FAULT_TCHIGH)  Serial.println("Termopar alto");
  if (fault & MAX31856_FAULT_TCLOW)   Serial.println("Termopar bajo");
  if (fault & MAX31856_FAULT_OVUV)    Serial.println("Sobre/subvoltaje");
  if (fault & MAX31856_FAULT_OPEN)    Serial.println("Termopar abierto");
}

// =====================================================
// SETUP
// =====================================================

void setup() {
  Serial.begin(115200);
  delay(800);

  Serial.println("Sistema final PCB: MAX31856 + ST7789 + MCP4725 + 4-20mA");

  // CS en reposo
  pinMode(MAX_CS, OUTPUT);
  pinMode(TFT_CS, OUTPUT);
  digitalWrite(MAX_CS, HIGH);
  digitalWrite(TFT_CS, HIGH);

  // SPI hardware
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);

  // I2C DAC
  Wire.begin(I2C_SDA, I2C_SCL);

  // DAC en salida segura: 4 mA
  float safeCurrent = 4.0;
  float safeVctrl = currentToControlVoltage(safeCurrent);
  bool dacOK = writeDAC12(voltsToDACCode(safeVctrl));

  Serial.print("MCP4725: ");
  Serial.println(dacOK ? "OK" : "NO DETECTADO");

  // Inicializar MAX31856
  if (!maxthermo.begin()) {
    Serial.println("ERROR: No se detecto MAX31856");
  } else {
    Serial.println("MAX31856 detectado");
  }

  maxthermo.setThermocoupleType(MAX31856_TCTYPE_K);

  // Inicializar TFT
  tft.init(240, 320);
  tft.setRotation(DISPLAY_ROTATION);
  drawStaticUI();

  delay(500);
}

// =====================================================
// LOOP
// =====================================================

void loop() {
  // Asegurar que la TFT no esté seleccionada al leer sensor
  digitalWrite(TFT_CS, HIGH);

  float cjTemp = maxthermo.readCJTemperature();
  float tempC  = maxthermo.readThermocoupleTemperature();
  uint8_t fault = maxthermo.readFault();

  bool sensorOK = true;

  if (fault) {
    sensorOK = false;
  }

  if (isnan(tempC)) {
    sensorOK = false;
  }

  // Si hay error, mandar salida segura de 4 mA
  if (!sensorOK) {
    float loopmA = 4.0;
    float vctrl = currentToControlVoltage(loopmA);
    bool dacOK = writeDAC12(voltsToDACCode(vctrl));

    updateDisplay(0.0, cjTemp, loopmA, vctrl, false, dacOK, fault);

    Serial.println("ERROR MAX31856");
    Serial.print("CJ = ");
    Serial.print(cjTemp, 2);
    Serial.print(" C | TC = ");
    Serial.print(tempC, 2);
    Serial.println(" C");

    printMAX31856Fault(fault);

    Serial.print("Salida segura = ");
    Serial.print(loopmA, 2);
    Serial.print(" mA | Vctrl = ");
    Serial.print(vctrl, 3);
    Serial.println(" V");

    Serial.println("----------------------");

    delay(1000);
    return;
  }

  // Limitar temperatura al rango de transmisión
  float tempLimited = clampf(tempC, TEMP_MIN_C, TEMP_MAX_C);

  // Temperatura -> corriente
  float loopmA = tempToCurrentmA(tempLimited);

  // Corriente -> voltaje de control
  float vctrl = currentToControlVoltage(loopmA);

  // Voltaje -> DAC
  uint16_t dacCode = voltsToDACCode(vctrl);
  bool dacOK = writeDAC12(dacCode);

  // Actualizar pantalla
  updateDisplay(tempLimited, cjTemp, loopmA, vctrl, true, dacOK, 0);

  // Monitor serial
  Serial.print("CJ = ");
  Serial.print(cjTemp, 2);
  Serial.print(" C | TC = ");
  Serial.print(tempC, 2);
  Serial.print(" C | TC limitada = ");
  Serial.print(tempLimited, 2);
  Serial.print(" C | Iloop = ");
  Serial.print(loopmA, 2);
  Serial.print(" mA | Vctrl = ");
  Serial.print(vctrl, 3);
  Serial.print(" V | DAC code = ");
  Serial.print(dacCode);
  Serial.print(" | DAC = ");
  Serial.println(dacOK ? "OK" : "ERROR");

  Serial.println("----------------------");

  delay(1000);
}
