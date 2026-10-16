// ============================================================
//  CYBERDECK - Etapa 1: boot + tela de boas-vindas
//  ESP32 + OLED SSD1306 128x64 (I2C)
// ============================================================
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---------------- PINAGEM ----------------
#define PIN_SDA       21
#define PIN_SCL       22
#define PIN_BTN_UP    32
#define PIN_BTN_DOWN  33
#define PIN_BTN_OK    25
#define PIN_BTN_BACK  26
#define PIN_BUZZER    27
#define PIN_LED_RING  14   // usado nas proximas etapas

// ---------------- PERSONALIZE ----------------
const char* NOME = "raiza";   // use so letras sem acento (a fonte do OLED nao tem)

// ---------------- DISPLAY ----------------
#define SCREEN_W 128
#define SCREEN_H 64
#define OLED_ADDR 0x3C
Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, -1);

enum Tela { TELA_BOAS_VINDAS, TELA_MENU };
Tela telaAtual = TELA_BOAS_VINDAS;

// ---------------- ESTRELAS ----------------
struct Estrela { uint8_t x, y, tipo; uint16_t periodo, defasagem; };
// tipo 0 = pontinho, tipo 1 = cruzinha (brilha e apaga)
const Estrela estrelas[] = {
  {  6, 16, 1, 1700,    0 }, { 22, 30, 0, 1300,  400 },
  { 10, 44, 1, 2100,  900 }, { 30, 20, 0, 1500,  200 },
  { 14, 52, 0, 1800,  600 }, {104, 14, 1, 1900,  300 },
  {118, 28, 0, 1200,  700 }, {108, 42, 1, 1600, 1100 },
  { 92, 24, 0, 1400,  500 }, {120, 52, 0, 2000, 1500 },
  { 80, 12, 0, 1100,  250 }, { 44, 12, 0, 1250,  850 }
};

void desenhaEstrelas() {
  for (const Estrela &e : estrelas) {
    float r = (float)((millis() + e.defasagem) % e.periodo) / e.periodo;
    if (r < 0.5) {                       // brilho maximo
      if (e.tipo == 1) {
        display.drawFastHLine(e.x - 2, e.y, 5, WHITE);
        display.drawFastVLine(e.x, e.y - 2, 5, WHITE);
      } else {
        display.drawPixel(e.x, e.y, WHITE);
      }
    } else if (r < 0.8) {                // brilho fraco
      display.drawPixel(e.x, e.y, WHITE);
    }                                    // senao: apagada
  }
}

// ---------------- POMPOMPURIN ----------------
// cx = centro horizontal | chaoY = linha do chao
// pulo = altura do pulo em pixels | amassa = 0..3 (agachado)
void desenhaPurin(int cx, int chaoY, int pulo, int amassa) {
  int corpoH = 9 - amassa;
  int corpoW = 20 + amassa * 2;
  int pesY   = chaoY - pulo;
  int corpoY = pesY - 2 - corpoH;
  int cabecaH = 18;
  int cabecaY = corpoY - cabecaH + 3;

  // sombra no chao (diminui quando ele esta no ar)
  int sw = 22 - pulo;
  if (sw < 8) sw = 8;
  display.drawFastHLine(cx - sw / 2, chaoY + 1, sw, WHITE);

  // corpo
  display.fillRoundRect(cx - corpoW / 2, corpoY, corpoW, corpoH, 3, BLACK);
  display.drawRoundRect(cx - corpoW / 2, corpoY, corpoW, corpoH, 3, WHITE);

  // patinhas
  display.fillRoundRect(cx - 9, pesY - 2, 7, 3, 1, WHITE);
  display.fillRoundRect(cx + 2, pesY - 2, 7, 3, 1, WHITE);

  // orelhas caidinhas (balancam um pouco quando ele pula)
  int balanco = (pulo > 4) ? -2 : 0;
  display.fillRoundRect(cx - 19, cabecaY + 5 + balanco, 6, 12, 3, BLACK);
  display.drawRoundRect(cx - 19, cabecaY + 5 + balanco, 6, 12, 3, WHITE);
  display.fillRoundRect(cx + 13, cabecaY + 5 + balanco, 6, 12, 3, BLACK);
  display.drawRoundRect(cx + 13, cabecaY + 5 + balanco, 6, 12, 3, WHITE);

  // cabeca
  display.fillRoundRect(cx - 14, cabecaY, 28, cabecaH, 8, BLACK);
  display.drawRoundRect(cx - 14, cabecaY, 28, cabecaH, 8, WHITE);

  // boina
  display.fillRoundRect(cx - 10, cabecaY - 4, 20, 6, 3, WHITE);
  display.fillRect(cx - 1, cabecaY - 6, 2, 2, WHITE);

  // olhinhos
  display.fillCircle(cx - 6, cabecaY + 9, 1, WHITE);
  display.fillCircle(cx + 6, cabecaY + 9, 1, WHITE);

  // focinho + nariz
  display.drawRoundRect(cx - 4, cabecaY + 10, 8, 5, 2, WHITE);
  display.fillRect(cx - 1, cabecaY + 10, 3, 2, WHITE);

  // bochechas coradas
  display.drawFastHLine(cx - 11, cabecaY + 12, 3, WHITE);
  display.drawFastHLine(cx + 9,  cabecaY + 12, 3, WHITE);
}

// ---------------- ENTRADA ----------------
bool algumBotao() {
  return !digitalRead(PIN_BTN_UP)   || !digitalRead(PIN_BTN_DOWN) ||
         !digitalRead(PIN_BTN_OK)   || !digitalRead(PIN_BTN_BACK);
}

void esperaSoltar() {
  while (algumBotao()) delay(10);
  delay(30);
}

// ---------------- SOM ----------------
void beep(int freq, int ms) {
  tone(PIN_BUZZER, freq, ms);
  delay(ms);
  noTone(PIN_BUZZER);
}

void somLigar() {
  beep(784, 80);    // sol
  beep(988, 80);    // si
  beep(1319, 140);  // mi
}

// ---------------- BOOT (terminal) ----------------
void digita(const char* texto) {
  int n = 0;
  for (const char* p = texto; *p; p++) {
    display.print(*p);
    if (++n % 2 == 0) display.display();
    delay(12);
  }
  display.display();
  display.print('\n');
  delay(140);
}

void sequenciaBoot() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  digita("> iniciando cyberdeck");
  digita("> carregando modulos...");
  digita("> sistema pronto!");
  somLigar();
  delay(400);
}

// ---------------- TELAS ----------------
void telaBoasVindas() {
  unsigned long t = millis();
  display.clearDisplay();
  desenhaEstrelas();

  // linha de terminal no topo
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.print("> ola, ");
  display.print(NOME);
  if ((t / 500) % 2 == 0) display.print("_");

  // pulinho: agacha -> sobe -> desce -> aterrissa
  float fase = (t % 1100) / 1100.0;
  int pulo = 0, amassa = 0;
  if (fase < 0.15)       { amassa = 3; }
  else if (fase < 0.75)  { float u = (fase - 0.15) / 0.60; pulo = (int)(4 * u * (1 - u) * 10); }
  else if (fase < 0.88)  { amassa = 2; }
  desenhaPurin(64, 52, pulo, amassa);

  // convite piscando
  if ((t / 700) % 2 == 0) {
    display.setCursor(7, 56);
    display.print("[ aperte um botao ]");
  }
  display.display();

  if (algumBotao()) {
    beep(1568, 60);
    esperaSoltar();
    telaAtual = TELA_MENU;
  }
}

void telaMenu() {  // placeholder: sera o menu real na Etapa 2
  display.clearDisplay();
  display.drawRect(0, 0, SCREEN_W, SCREEN_H, WHITE);
  display.setTextSize(1);
  display.setCursor(20, 10);
  display.print("MENU PRINCIPAL");
  display.setCursor(14, 28);
  display.print("(proxima etapa!)");
  display.setCursor(16, 48);
  display.print("BACK = voltar");
  display.display();

  if (!digitalRead(PIN_BTN_BACK)) {
    beep(988, 60);
    esperaSoltar();
    telaAtual = TELA_BOAS_VINDAS;
  }
}

// ---------------- SETUP / LOOP ----------------
void setup() {
  Serial.begin(115200);

  pinMode(PIN_BTN_UP,   INPUT_PULLUP);
  pinMode(PIN_BTN_DOWN, INPUT_PULLUP);
  pinMode(PIN_BTN_OK,   INPUT_PULLUP);
  pinMode(PIN_BTN_BACK, INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);

  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(400000);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED nao encontrado! Confira SDA/SCL.");
    while (true) delay(1000);
  }

  sequenciaBoot();
}

void loop() {
  switch (telaAtual) {
    case TELA_BOAS_VINDAS: telaBoasVindas(); break;
    case TELA_MENU:        telaMenu();       break;
  }
  delay(20);
}
