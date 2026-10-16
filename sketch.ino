// ============================================================
//  CYBERDECK - Etapa 2: menu principal (carrossel de icones)
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

// (precisa ficar aqui no topo: o Arduino gera os prototipos das funcoes
//  antes de qualquer funcao, e eles usam este tipo)
struct Botao { uint8_t pino; bool anterior; unsigned long proxRepeticao; };

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
  while (algumBotao()) {
    delay(10);
  }
  delay(30);
}

// Botao com deteccao de clique (e repeticao opcional ao segurar)
bool clicou(Botao &b, bool repetir) {
  bool agora = !digitalRead(b.pino);
  unsigned long t = millis();
  bool evento = false;
  if (agora && !b.anterior) {
    evento = true;
    b.proxRepeticao = t + 350;           // espera antes de comecar a repetir
  } else if (agora && repetir && t >= b.proxRepeticao) {
    evento = true;
    b.proxRepeticao = t + 170;           // velocidade da repeticao
  }
  b.anterior = agora;
  return evento;
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

void somTique()    { beep(2400, 8); }    // navegar no menu
void somConfirma() { beep(1568, 60); }   // OK
void somVoltar()   { beep(988, 60); }    // BACK

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

// ============================================================
//  ICONES (desenhados com primitivas, escalaveis)
//  Todas as coordenadas abaixo sao numa grade 32x32
// ============================================================
static int gX, gY, gS;   // canto do icone e tamanho atual

static int S(int v)  { return (v * gS) / 32; }
static int PX(int v) { return gX + S(v); }
static int PY(int v) { return gY + S(v); }

static void L(int x1, int y1, int x2, int y2) {
  display.drawLine(PX(x1), PY(y1), PX(x2), PY(y2), WHITE);
}
// modo: 0 = contorno, 1 = preenchido branco, 2 = preenchido preto
static void R(int x, int y, int w, int h, int r, int modo) {
  int ww = max(1, S(w)), hh = max(1, S(h));
  int rr = min(S(r), min(ww, hh) / 2);
  if (modo == 0)      display.drawRoundRect(PX(x), PY(y), ww, hh, rr, WHITE);
  else if (modo == 1) display.fillRoundRect(PX(x), PY(y), ww, hh, rr, WHITE);
  else                display.fillRoundRect(PX(x), PY(y), ww, hh, rr, BLACK);
}
static void C(int x, int y, int r, bool cheio) {
  int rr = S(r);
  if (cheio) display.fillCircle(PX(x), PY(y), rr, WHITE);
  else       display.drawCircle(PX(x), PY(y), max(1, rr), WHITE);
}
static void ARC(int cx, int cy, int r, int a0, int a1) {
  for (int a = a0; a <= a1; a += 3) {
    float rad = a * DEG_TO_RAD;
    display.drawPixel(PX(cx) + (int)roundf(cosf(rad) * S(r)),
                      PY(cy) + (int)roundf(sinf(rad) * S(r)), WHITE);
  }
}

#define N_ITENS 8
const char* NOMES[N_ITENS] = {
  "RELOGIO / DATA", "SCANNER WI-FI", "MONITOR SISTEMA", "MINI JOGOS",
  "PET VIRTUAL", "CONFIGURACOES", "CRONOMETRO", "EFEITOS LED"
};

void desenhaIcone(int id, int cx, int cy, int sz) {
  gS = sz; gX = cx - sz / 2; gY = cy - sz / 2;

  switch (id) {
    case 0:  // relogio
      C(16, 16, 14, false);
      L(16, 3, 16, 5);  L(16, 27, 16, 29);
      L(3, 16, 5, 16);  L(27, 16, 29, 16);
      L(16, 16, 16, 8); L(16, 16, 22, 19);
      C(16, 16, 1, true);
      break;

    case 1:  // wi-fi
      ARC(16, 25, 6,  -135, -45);
      ARC(16, 25, 12, -135, -45);
      ARC(16, 25, 18, -135, -45);
      C(16, 25, 2, true);
      break;

    case 2:  // monitor do sistema (tela com grafico)
      R(2, 4, 28, 19, 3, 0);
      L(6, 18, 11, 12); L(11, 12, 15, 16);
      L(15, 16, 20, 8); L(20, 8, 25, 14);
      L(16, 23, 16, 27);
      L(10, 28, 22, 28);
      break;

    case 3:  // mini jogos (controle)
      R(2, 9, 28, 17, 7, 0);
      L(5, 17, 13, 17); L(9, 13, 9, 21);
      C(21, 15, 2, true);
      C(26, 19, 2, true);
      break;

    case 4:  // pet virtual (carinha do Purin)
      R(2, 13, 5, 10, 2, 0);
      R(25, 13, 5, 10, 2, 0);
      R(6, 8, 20, 17, 7, 2);   // limpa as orelhas por tras
      R(6, 8, 20, 17, 7, 0);
      R(9, 4, 14, 5, 2, 1);    // boina
      R(15, 2, 2, 3, 0, 1);
      C(12, 16, 1, true);
      C(20, 16, 1, true);
      R(13, 18, 6, 4, 1, 0);   // focinho
      L(8, 20, 10, 20);        // bochechas
      L(22, 20, 24, 20);
      break;

    case 5:  // configuracoes (engrenagem)
      C(16, 16, 8, false);
      C(16, 16, 3, false);
      for (int k = 0; k < 8; k++) {
        float rad = k * 45 * DEG_TO_RAD;
        C(16 + (int)roundf(cosf(rad) * 12), 16 + (int)roundf(sinf(rad) * 12), 2, true);
      }
      break;

    case 6:  // cronometro
      C(16, 18, 11, false);
      R(13, 2, 6, 3, 1, 1);
      L(16, 5, 16, 7);
      L(25, 8, 27, 10);
      L(16, 18, 21, 13);
      C(16, 18, 1, true);
      break;

    case 7: {  // efeitos do led (aro girando)
      int aceso = (millis() / 150) % 8;
      for (int k = 0; k < 8; k++) {
        float rad = (k * 45 - 90) * DEG_TO_RAD;
        int x = 16 + (int)roundf(cosf(rad) * 12);
        int y = 16 + (int)roundf(sinf(rad) * 12);
        C(x, y, 2, k == aceso);
      }
      break;
    }
  }
}

// ============================================================
//  PURINZINHO DECORATIVO (cantinho do cabecalho)
// ============================================================
void desenhaPurinMini(int x, int y, int olhar, bool piscando) {
  // x,y = canto superior esquerdo (13 de largura, 12 de altura)
  display.fillRoundRect(x, y + 2, 13, 10, 3, BLACK);
  display.drawRoundRect(x, y + 2, 13, 10, 3, WHITE);
  display.fillRect(x + 2, y, 9, 3, WHITE);                  // boina
  display.drawFastVLine(x - 1, y + 5, 4, WHITE);            // orelhas
  display.drawFastVLine(x + 13, y + 5, 4, WHITE);
  if (piscando) {
    display.drawFastHLine(x + 2 + olhar, y + 6, 2, WHITE);
    display.drawFastHLine(x + 8 + olhar, y + 6, 2, WHITE);
  } else {
    display.drawPixel(x + 3 + olhar, y + 6, WHITE);
    display.drawPixel(x + 9 + olhar, y + 6, WHITE);
  }
  display.drawPixel(x + 6, y + 9, WHITE);                   // focinho
}

// ============================================================
//  MENU
// ============================================================
#define DUR_TRANSICAO 220      // ms da animacao de deslize
#define ESPACAMENTO   40       // distancia entre icones (px)

Botao bUp   = { PIN_BTN_UP,   false, 0 };
Botao bDown = { PIN_BTN_DOWN, false, 0 };
Botao bOk   = { PIN_BTN_OK,   false, 0 };
Botao bBack = { PIN_BTN_BACK, false, 0 };

int  selecionado = 0;
float offIni = 0;                  // deslocamento inicial da animacao
unsigned long tAnim = 0;           // quando a animacao comecou
int  olharDir = 0;                 // Purin mini olha pra onde voce foi
unsigned long tOlhar = 0;
unsigned long avisoAte = 0;        // aviso "em breve"

// deslocamento atual (decai de offIni ate 0 com ease-out)
float offAtual() {
  float u = (millis() - tAnim) / (float)DUR_TRANSICAO;
  if (u >= 1.0) return 0;
  float k = 1.0 - u;
  return offIni * k * k;
}

void navegar(int dir) {
  float off = offAtual();
  selecionado = (selecionado + dir + N_ITENS) % N_ITENS;   // circular
  offIni = constrain(off + dir, -2.0, 2.0);
  tAnim = millis();
  olharDir = dir;
  tOlhar = millis();
  avisoAte = 0;
  somTique();
}

void desenhaCantosSelecao() {
  // 4 cantoneiras em volta do icone central (36x36)
  const int x0 = 46, y0 = 12, x1 = 81, y1 = 47, c = 5;
  display.drawFastHLine(x0, y0, c, WHITE); display.drawFastVLine(x0, y0, c, WHITE);
  display.drawFastHLine(x1 - c + 1, y0, c, WHITE); display.drawFastVLine(x1, y0, c, WHITE);
  display.drawFastHLine(x0, y1, c, WHITE); display.drawFastVLine(x0, y1 - c + 1, c, WHITE);
  display.drawFastHLine(x1 - c + 1, y1, c, WHITE); display.drawFastVLine(x1, y1 - c + 1, c, WHITE);
}

void desenhaBarra() {
  // 8 segmentos de 14px com 1px de espaco = 119px
  for (int i = 0; i < N_ITENS; i++) {
    int x = 4 + i * 15;
    if (i == selecionado) display.fillRect(x, 60, 14, 3, WHITE);
    else                  display.drawFastHLine(x, 61, 14, WHITE);
  }
}

void textoCentralizado(const char* txt, int y) {
  int w = strlen(txt) * 6;
  display.setCursor((SCREEN_W - w) / 2, y);
  display.print(txt);
}

void telaMenu() {
  unsigned long t = millis();

  // ---- entrada ----
  if (clicou(bUp,   true)) navegar(-1);
  if (clicou(bDown, true)) navegar(+1);
  if (clicou(bOk,   false)) {
    somConfirma();
    avisoAte = millis() + 1200;     // modulos ainda nao existem
  }
  if (clicou(bBack, false)) {
    somVoltar();
    esperaSoltar();
    telaAtual = TELA_BOAS_VINDAS;
    return;
  }

  float off = offAtual();

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  // ---- cabecalho ----
  display.setCursor(0, 2);
  display.print("> menu");
  char pos[8];
  snprintf(pos, sizeof(pos), "%d/%d", selecionado + 1, N_ITENS);
  display.setCursor(92, 2);
  display.print(pos);

  int olhar = (t - tOlhar < 350) ? olharDir : 0;
  bool piscando = (t % 3200) < 120;
  desenhaPurinMini(114, 0, olhar, piscando);

  // ---- carrossel ----
  desenhaCantosSelecao();
  for (int rel = -3; rel <= 3; rel++) {
    int idx = (selecionado + rel + N_ITENS) % N_ITENS;
    float d  = rel + off;
    float ad = fabsf(d);
    if (ad > 2.1) continue;
    int x  = 64 + (int)roundf(d * ESPACAMENTO);
    int sz = 32 - (int)(16 * fminf(ad, 1.0));
    desenhaIcone(idx, x, 30, sz);
  }

  // ---- nome do item (o que estiver mais perto do centro) ----
  int maisPerto = selecionado;
  if (fabsf(off) > 0.5) {
    int sinal = (off > 0) ? 1 : -1;
    maisPerto = (selecionado - sinal + N_ITENS) % N_ITENS;
  }
  if (t < avisoAte) {
    if ((t / 200) % 2 == 0) textoCentralizado("[ em breve! ]", 50);
  } else {
    textoCentralizado(NOMES[maisPerto], 50);
  }

  // ---- barra de posicao ----
  desenhaBarra();

  display.display();
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
    somConfirma();
    esperaSoltar();
    telaAtual = TELA_MENU;
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
  delay(5);   // pequeno para a animacao do menu ficar fluida
}
