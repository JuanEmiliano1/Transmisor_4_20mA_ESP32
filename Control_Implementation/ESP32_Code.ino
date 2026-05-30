#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

// ==========================
// Pines SPI compartido
// ==========================
#define SPI_SCK   18
#define SPI_MISO  19
#define SPI_MOSI  23

// MAX6675
#define MAX_CS    32

// TFT ST7789
#define TFT_CS    27
#define TFT_DC    26
#define TFT_RST   25

// MCP4725
#define I2C_SDA   21
#define I2C_SCL   22

// ==========================
// Objetos
// ==========================
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// ==========================
// MCP4725
// ==========================
const uint8_t MCP4725_ADDR = 0x60;

// ==========================
// Parámetros
// ==========================
const float TEMP_MIN_C = 0.0;
const float TEMP_MAX_C = 200.0;

const float I_MIN_mA = 4.0;
const float I_MAX_mA = 20.0;

const float RSENSE = 50.0;

// Si la pantalla se ve mal, cambia 1 por 3
const uint8_t DISPLAY_ROTATION = 1;

// ==========================
// Funciones generales
// ==========================
float clampf(float x, float xmin, float xmax) {
  if (x < xmin) return xmin;
  if (x > xmax) return xmax;
  return x;
}

float tempToCurrentmA(float tempC) {
  tempC = clampf(tempC, TEMP_MIN_C, TEMP_MAX_C);
  return I_MIN_mA + ((tempC - TEMP_MIN_C) * (I_MAX_mA - I_MIN_mA)) / (TEMP_MAX_C - TEMP_MIN_C);
}

float currentToControlVoltage(float current_mA) {
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

// ==========================
// MAX6675 usando SPI hardware
// ==========================
uint16_t readMAX6675Raw() {
  uint16_t raw = 0;

  // Asegura que la pantalla no esté seleccionada
  digitalWrite(TFT_CS, HIGH);

  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));

  digitalWrite(MAX_CS, LOW);
  delayMicroseconds(5);

  raw = SPI.transfer16(0x0000);

  delayMicroseconds(5);
  digitalWrite(MAX_CS, HIGH);

  SPI.endTransaction();

  return raw;
}

float readMAX6675C(bool &ok, uint16_t &rawOut) {
  uint16_t raw = readMAX6675Raw();
  rawOut = raw;

  if (raw == 0x0000) {
    ok = false;
    return 0.0;
  }

  if (raw == 0xFFFF) {
    ok = false;
    return 0.0;
  }

  // Bit D2 = termopar abierto
  if (raw & 0x0004) {
    ok = false;
    return 0.0;
  }

  raw >>= 3;
  ok = true;
  return raw * 0.25;
}

// ==========================
// Pantalla
// ==========================
void drawStaticUI() {
  digitalWrite(MAX_CS, HIGH);

  tft.fillScreen(ST77XX_BLACK);

  tft.drawRoundRect(6, 6, 308, 228, 8, ST77XX_WHITE);

  tft.fillRoundRect(12, 12, 296, 28, 6, ST77XX_BLUE);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(22, 18);
  tft.print("Temp + 4-20mA");

  tft.drawRoundRect(14, 50, 292, 64, 8, ST77XX_CYAN);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(24, 60);
  tft.print("Temperatura");

  tft.drawRoundRect(14, 124, 140, 50, 8, ST77XX_GREEN);
  tft.setCursor(24, 134);
  tft.print("Corriente");

  tft.drawRoundRect(166, 124, 140, 50, 8, ST77XX_YELLOW);
  tft.setCursor(176, 134);
  tft.print("V control");

  tft.drawRoundRect(14, 184, 292, 42, 8, ST77XX_MAGENTA);
  tft.setCursor(24, 194);
  tft.print("Estado");
}

void clearDynamicAreas() {
  tft.fillRect(24, 76, 260, 30, ST77XX_BLACK);
  tft.fillRect(24, 150, 118, 18, ST77XX_BLACK);
  tft.fillRect(176, 150, 118, 18, ST77XX_BLACK);
  tft.fillRect(24, 208, 260, 12, ST77XX_BLACK);
}

void updateDisplay(float tempC, float loopmA, float vctrl, bool sensorOK, bool dacOK, uint16_t raw) {
  digitalWrite(MAX_CS, HIGH);

  clearDynamicAreas();

  if (sensorOK) {
    tft.setTextColor(ST77XX_YELLOW);
    tft.setTextSize(3);
    tft.setCursor(24, 78);
    tft.print(tempC, 1);
    tft.print(" C");

    tft.setTextColor(ST77XX_GREEN);
    tft.setTextSize(2);
    tft.setCursor(24, 150);
    tft.print(loopmA, 2);
    tft.print("mA");

    tft.setTextColor(ST77XX_CYAN);
    tft.setCursor(176, 150);
    tft.print(vctrl, 3);
    tft.print("V");

    tft.setTextSize(1);
    tft.setCursor(24, 208);

    if (dacOK) {
      tft.setTextColor(ST77XX_GREEN);
      tft.print("MAX6675 OK | DAC OK");
    } else {
      tft.setTextColor(ST77XX_RED);
      tft.print("MAX6675 OK | DAC ERROR");
    }

  } else {
    tft.setTextColor(ST77XX_RED);
    tft.setTextSize(3);
    tft.setCursor(24, 78);
    tft.print("ERROR");

    tft.setTextSize(1);
    tft.setCursor(24, 208);
    tft.print("RAW=0x");
    tft.print(raw, HEX);
  }
}

// ==========================
// Setup
// ==========================
void setup() {
  Serial.begin(115200);
  delay(800);

  Serial.println("Sistema final corregido: MAX6675 + ST7789 + MCP4725 + 4-20mA");

  pinMode(MAX_CS, OUTPUT);
  pinMode(TFT_CS, OUTPUT);

  digitalWrite(MAX_CS, HIGH);
  digitalWrite(TFT_CS, HIGH);

  // Inicializar SPI hardware una sola vez
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);

  // I2C MCP4725
  Wire.begin(I2C_SDA, I2C_SCL);

  // DAC en salida segura: 4 mA
  float safeCurrent = 4.0;
  float safeVctrl = currentToControlVoltage(safeCurrent);
  bool dacOK = writeDAC12(voltsToDACCode(safeVctrl));

  Serial.print("MCP4725: ");
  Serial.println(dacOK ? "OK" : "NO DETECTADO");

  // TFT
  tft.init(240, 320);
  tft.setRotation(DISPLAY_ROTATION);
  drawStaticUI();

  delay(500);
}

// ==========================
// Loop
// ==========================
void loop() {
  bool sensorOK = false;
  uint16_t raw = 0;
  float tempC = readMAX6675C(sensorOK, raw);

  if (!sensorOK) {
    float loopmA = 4.0;
    float vctrl = currentToControlVoltage(loopmA);
    bool dacOK = writeDAC12(voltsToDACCode(vctrl));

    updateDisplay(0.0, loopmA, vctrl, false, dacOK, raw);

    Serial.print("ERROR MAX6675 | RAW = 0x");
    Serial.println(raw, HEX);
    Serial.println("Revisar SO, CS, SCK, VCC, GND o termopar abierto.");
    Serial.println("----------------------");

    delay(700);
    return;
  }

  float tempLimited = clampf(tempC, TEMP_MIN_C, TEMP_MAX_C);
  float loopmA = tempToCurrentmA(tempLimited);
  float vctrl = currentToControlVoltage(loopmA);
  uint16_t dacCode = voltsToDACCode(vctrl);
  bool dacOK = writeDAC12(dacCode);

  updateDisplay(tempLimited, loopmA, vctrl, true, dacOK, raw);

// ... (código previo del loop se mantiene igual para conservar la pantalla TFT)
  
  updateDisplay(tempLimited, loopmA, vctrl, true, dacOK, raw);

  // ENVÍO DE DATOS A MATLAB (Solo el valor numérico)
  Serial.println(vctrl, 3); // Envía el voltaje con 3 decimales seguido de un salto de línea

  delay(700); // Mismo retardo de tu sistema original
}

