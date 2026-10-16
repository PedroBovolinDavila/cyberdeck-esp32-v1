// ============================================================
//  CYBERDECK - Etapa 3: modulo Relogio / Data
//  ESP32 + OLED SSD1306 128x64 (I2C)
// ============================================================
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <time.h>

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

// Wi-Fi (usado pra pegar a hora certa na internet)
// No Wokwi deixe assim. No ESP32 de verdade, coloque o nome e a senha da sua rede.
const char* WIFI_SSID  = "Wokwi-GUEST";
const char* WIFI_SENHA = "";

// Fuso horario: Brasilia = UTC-3 (formato POSIX: o sinal e invertido de proposito)
const char* FUSO = "<-03>3";

// Hemisferio sul (Brasil): a Lua crescente aparece iluminada do lado ESQUERDO.
// Mude pra false se quiser o desenho do hemisferio norte.
#define HEMISFERIO_SUL true

// ---------------- DISPLAY ----------------
#define SCREEN_W 128
#define SCREEN_H 64
#define OLED_ADDR 0x3C
Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, -1);

enum Tela {
  TELA_BOAS_VINDAS,
  TELA_MENU,
  TELA_RELOGIO,
  TELA_TAROT
};

Tela telaAtual = TELA_BOAS_VINDAS;

// (precisa ficar aqui no topo: o Arduino gera os prototipos das funcoes
//  antes de qualquer funcao, e eles usam este tipo)
struct Botao { uint8_t pino; bool anterior; unsigned long proxRepeticao; };

// ---------------- MODULO TAROT ----------------
#define N_CARTAS_TAROT 22

struct CartaTarot {
  const char* nome;
  const char* significado;
};

const CartaTarot baralhoTarot[N_CARTAS_TAROT] = {
  {"O LOUCO",       "Novos caminhos"},
  {"O MAGO",        "Acao e habilidade"},
  {"A SACERDOTISA", "Intuicao e misterio"},
  {"A IMPERATRIZ",  "Criacao e abundancia"},
  {"O IMPERADOR",   "Ordem e estabilidade"},
  {"O HIEROFANTE",  "Tradicao e ensino"},
  {"OS ENAMORADOS", "Escolhas e uniao"},
  {"O CARRO",       "Foco e conquista"},
  {"A JUSTICA",     "Equilibrio e verdade"},
  {"O EREMITA",     "Reflexao e sabedoria"},
  {"RODA DA FORTUNA","Mudancas e ciclos"},
  {"A FORCA",       "Coragem e paciencia"},
  {"O ENFORCADO",   "Nova perspectiva"},
  {"A MORTE",       "Fim e renovacao"},
  {"A TEMPERANCA",  "Harmonia e medida"},
  {"O DIABO",       "Desejos e apegos"},
  {"A TORRE",       "Ruptura e revelacao"},
  {"A ESTRELA",     "Esperanca e inspiracao"},
  {"A LUA",         "Sonhos e incerteza"},
  {"O SOL",         "Alegria e clareza"},
  {"O JULGAMENTO",  "Despertar e decisao"},
  {"O MUNDO",       "Conclusao e plenitude"}
};

// Estado exclusivo do modulo Tarot
int cartaAtual = -1;
int cartasUsadas[N_CARTAS_TAROT];
int totalCartasUsadas = 0;

enum EstadoTarot {
  TAROT_ENTRADA,
  TAROT_EMBARALHANDO,
  TAROT_REVELANDO,
  TAROT_INFORMACAO,
  TAROT_DECISAO
};

EstadoTarot estadoTarot = TAROT_ENTRADA;

unsigned long tTarot = 0;
int opcaoTarot = 0; // 0 = outra carta, 1 = encerrar


 // Sorteia uma carta que ainda nao saiu nesta tiragem
int sortearCartaTarot() {
  if (totalCartasUsadas >= N_CARTAS_TAROT) {
    return -1;
  }

  int indice;
  bool repetida;

  do {
    indice = random(N_CARTAS_TAROT);
    repetida = false;

    for (int i = 0; i < totalCartasUsadas; i++) {
      if (cartasUsadas[i] == indice) {
        repetida = true;
        break;
      }
    }
  } while (repetida);

  cartasUsadas[totalCartasUsadas] = indice;
  totalCartasUsadas++;

  return indice;
}

// Reinicia a tiragem e libera todas as cartas
void reiniciarTarot() {
  cartaAtual = -1;
  totalCartasUsadas = 0;
}



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
void somErro() {
  tone(PIN_BUZZER, 220, 180);
  delay(220);
  tone(PIN_BUZZER, 160, 250);
  delay(270);
  noTone(PIN_BUZZER);
}
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

#define N_ITENS 9

const char* NOMES[N_ITENS] = {
  "RELOGIO / DATA",
  "SCANNER WI-FI",
  "MONITOR SISTEMA",
  "MINI JOGOS",
  "PET VIRTUAL",
  "CONFIGURACOES",
  "CRONOMETRO",
  "EFEITOS LED",
  "TAROT"
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
    case 8: { // TAROT — carta mistica
      // Contorno externo da carta
      R(6, 2, 20, 28, 2, 0);

      // Moldura interna
      R(9, 5, 14, 22, 1, 0);

      // Estrela central
      L(16, 9, 18, 15);
      L(18, 15, 23, 16);
      L(23, 16, 18, 18);
      L(18, 18, 16, 24);
      L(16, 24, 14, 18);
      L(14, 18, 9, 16);
      L(9, 16, 14, 15);
      L(14, 15, 16, 9);

      // Simbolos decorativos
      C(16, 7, 1, false);
      L(13, 27, 19, 27);
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
//  HORA + LUA  (usado pelo modulo Relogio)
// ============================================================
struct tm hora;                 // hora local atual (preenchida por atualizaHora)
bool   horaSync   = false;      // true = hora veio da internet (NTP)
time_t horaEpoch  = 0;
time_t epochBase  = 0;          // hora da compilacao (plano B sem internet)
bool   ntpIniciado = false;
float  luaIdade   = 0;          // idade da Lua em dias (0 a 29.5)
float  luaP       = 0;          // fase da Lua (0 = nova, 0.5 = cheia, 1 = nova)

const char* DIAS_SEMANA[7] = { "DOMINGO", "SEGUNDA", "TERCA", "QUARTA", "QUINTA", "SEXTA", "SABADO" };
const char* MESES_CURTO[12] = { "JAN", "FEV", "MAR", "ABR", "MAI", "JUN", "JUL", "AGO", "SET", "OUT", "NOV", "DEZ" };
const char* FASES[8] = {
  "Lua Nova", "Crescente", "Quarto Crescente", "Gibosa Crescente",
  "Lua Cheia", "Gibosa Minguante", "Quarto Minguante", "Minguante"
};

// Plano B: se nao tiver internet, comeca da hora em que o codigo foi compilado
void iniciaHoraCompilacao() {
  char mes[4];
  int d, a, h, m, s;
  sscanf(__DATE__, "%3s %d %d", mes, &d, &a);
  sscanf(__TIME__, "%d:%d:%d", &h, &m, &s);
  const char* meses = "JanFebMarAprMayJunJulAugSepOctNovDec";
  const char* achou = strstr(meses, mes);
  int mi = achou ? (achou - meses) / 3 : 0;

  struct tm c = {};
  c.tm_year = a - 1900;
  c.tm_mon  = mi;
  c.tm_mday = d;
  c.tm_hour = h;
  c.tm_min  = m;
  c.tm_sec  = s;
  c.tm_isdst = 0;
  epochBase = mktime(&c);
}

// Liga o Wi-Fi sem travar (a conexao acontece em segundo plano)
void wifiIniciar() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_SENHA, 6);
}

// Chame a cada volta do loop: quando o Wi-Fi conectar, inicia o NTP
void wifiAtualiza() {
  if (!ntpIniciado && WiFi.status() == WL_CONNECTED) {
    configTzTime(FUSO, "pool.ntp.org", "time.nist.gov");
    ntpIniciado = true;
  }
}

void atualizaHora() {
  time_t agora = time(nullptr);
  if (agora > 1700000000) {            // passou de 2023 = o NTP ja acertou o relogio
    horaSync = true;
  } else {
    horaSync = false;
    agora = epochBase + (time_t)(millis() / 1000);
  }
  horaEpoch = agora;
  localtime_r(&agora, &hora);

  // Fase da Lua: lua nova de referencia = 6/jan/2000 18:14 UTC (epoch 947182440)
  const double CICLO = 29.530588853;
  double dias = (double)(agora - 947182440) / 86400.0;
  double a = fmod(dias, CICLO);
  if (a < 0) a += CICLO;
  luaIdade = (float)a;
  luaP = (float)(a / CICLO);
}

int diasNoMes(int mes0, int ano) {
  static const uint8_t d[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  bool bissexto = (ano % 4 == 0 && ano % 100 != 0) || (ano % 400 == 0);
  if (mes0 == 1 && bissexto) return 29;
  return d[mes0];
}

// Desenha a Lua com a parte iluminada de acordo com a fase p (0..1)
void desenhaLua(int cx, int cy, int r, float p) {
  float k = cosf(2.0f * PI * p);
  bool crescendo = (p < 0.5f);
  for (int y = -r; y <= r; y++) {
    float w = sqrtf((float)(r * r - y * y));
    int wi = (int)roundf(w);
    int xt = (int)roundf(w * k);      // posicao do terminador (borda luz/sombra)
    int xa, xb;
    if (crescendo) { xa = xt;  xb = wi;  }
    else           { xa = -wi; xb = -xt; }
    if (HEMISFERIO_SUL) { int ta = xa; xa = -xb; xb = -ta; }
    if (xb > xa) display.drawFastHLine(cx + xa, cy + y, xb - xa + 1, WHITE);
  }
  display.drawCircle(cx, cy, r, WHITE);
}

// ============================================================
//  MODULO RELOGIO
// ============================================================
#define N_PAGS_RELOGIO 4          // 0 digital | 1 analogico | 2 lua | 3 calendario
#define DUR_WIPE 200              // ms da animacao de troca de pagina

int pagRelogio = 0;
unsigned long tPag = 0;

void desenhaPontos(int x, int y) {
  for (int i = 0; i < N_PAGS_RELOGIO; i++) {
    if (i == pagRelogio) display.fillRect(x + i * 6, y, 3, 3, WHITE);
    else                 display.drawPixel(x + i * 6 + 1, y + 1, WHITE);
  }
}

// icone de sinal: cheio = hora da internet | piscando = ainda sincronizando
void desenhaWifi(int x, int y) {
  if (horaSync) {
    display.fillRect(x,     y + 4, 2, 2, WHITE);
    display.fillRect(x + 3, y + 2, 2, 4, WHITE);
    display.fillRect(x + 6, y,     2, 6, WHITE);
  } else if ((millis() / 500) % 2 == 0) {
    display.drawRect(x,     y + 4, 2, 2, WHITE);
    display.drawRect(x + 3, y + 2, 2, 4, WHITE);
    display.drawRect(x + 6, y,     2, 6, WHITE);
  }
}

static void ponteiro(int cx, int cy, float graus, int len, bool grosso) {
  float rad = graus * DEG_TO_RAD;
  int x = cx + (int)roundf(sinf(rad) * len);
  int y = cy - (int)roundf(cosf(rad) * len);
  display.drawLine(cx, cy, x, y, WHITE);
  if (grosso) {
    display.drawLine(cx + 1, cy, x + 1, y, WHITE);
    display.drawLine(cx, cy + 1, x, y + 1, WHITE);
  }
}

// ---------- pagina 0: digital ----------
void paginaDigital() {
  char buf[24];
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(DIAS_SEMANA[hora.tm_wday]);
  desenhaPontos(50, 2);
  desenhaWifi(117, 0);

  // hora grandona (os dois pontos piscam)
  snprintf(buf, sizeof(buf), "%02d%c%02d", hora.tm_hour, (hora.tm_sec % 2 == 0) ? ':' : ' ', hora.tm_min);
  display.setTextSize(4);
  display.setCursor(4, 11);
  display.print(buf);

  // barrinha dos segundos
  display.setTextSize(1);
  int larg = (hora.tm_sec * 120) / 59;
  display.fillRect(4, 42, max(larg, 1), 2, WHITE);
  for (int i = 0; i <= 5; i++) display.drawFastVLine(4 + i * 24, 44, 2, WHITE);

  // data + segundos + luazinha
  snprintf(buf, sizeof(buf), "%02d %s %d", hora.tm_mday, MESES_CURTO[hora.tm_mon], hora.tm_year + 1900);
  display.setCursor(0, 52);
  display.print(buf);
  snprintf(buf, sizeof(buf), "%02ds", hora.tm_sec);
  display.setCursor(76, 52);
  display.print(buf);
  desenhaLua(121, 57, 5, luaP);
}

// ---------- pagina 1: analogico ----------
void paginaAnalogico() {
  const int cx = 32, cy = 32;
  char buf[24];

  // mostrador
  display.drawCircle(cx, cy, 30, WHITE);
  for (int i = 0; i < 12; i++) {
    float rad = i * 30 * DEG_TO_RAD;
    int r1 = (i % 3 == 0) ? 23 : 26;
    int r2 = 29;
    display.drawLine(cx + (int)roundf(sinf(rad) * r1), cy - (int)roundf(cosf(rad) * r1),
                     cx + (int)roundf(sinf(rad) * r2), cy - (int)roundf(cosf(rad) * r2), WHITE);
  }

  // ponteiros
  float angH = (hora.tm_hour % 12) * 30.0f + hora.tm_min * 0.5f;
  float angM = hora.tm_min * 6.0f + hora.tm_sec * 0.1f;
  float angS = hora.tm_sec * 6.0f;
  ponteiro(cx, cy, angH, 14, true);
  ponteiro(cx, cy, angM, 22, true);
  ponteiro(cx, cy, angS + 180, 5, false);   // rabinho do ponteiro dos segundos
  ponteiro(cx, cy, angS, 25, false);
  display.fillCircle(cx, cy, 2, WHITE);
  display.drawPixel(cx, cy, BLACK);

  // painel da direita
  display.setTextSize(1);
  desenhaPontos(70, 1);
  desenhaWifi(117, 0);

  snprintf(buf, sizeof(buf), "%02d:%02d", hora.tm_hour, hora.tm_min);
  display.setTextSize(2);
  display.setCursor(68, 9);
  display.print(buf);

  display.setTextSize(1);
  display.setCursor(68, 29);
  display.print(DIAS_SEMANA[hora.tm_wday]);
  snprintf(buf, sizeof(buf), "%02d %s", hora.tm_mday, MESES_CURTO[hora.tm_mon]);
  display.setCursor(68, 39);
  display.print(buf);
  snprintf(buf, sizeof(buf), "%d", hora.tm_year + 1900);
  display.setCursor(68, 49);
  display.print(buf);

  desenhaLua(117, 51, 8, luaP);
}

// ---------- pagina 2: fases da lua ----------
void paginaLua() {
  char buf[24];
  display.setTextSize(1);
  display.setCursor(56, 2);
  display.print("LUA");
  desenhaPontos(100, 3);

  desenhaLua(26, 30, 22, luaP);

  const float CICLO = 29.530588853f;
  int ilum = (int)roundf((1.0f - cosf(2.0f * PI * luaP)) * 50.0f);
  float paraCheia = (luaP < 0.5f) ? (0.5f - luaP) * CICLO : (1.5f - luaP) * CICLO;
  float paraNova  = (1.0f - luaP) * CICLO;
  int dCheia = (int)roundf(paraCheia);
  int dNova  = (int)roundf(paraNova);

  snprintf(buf, sizeof(buf), "ilum. %d%%", ilum);
  display.setCursor(56, 14);
  display.print(buf);

  snprintf(buf, sizeof(buf), "idade %dd", (int)roundf(luaIdade));
  display.setCursor(56, 24);
  display.print(buf);

  if (dCheia == 0) snprintf(buf, sizeof(buf), "cheia hoje!");
  else             snprintf(buf, sizeof(buf), "cheia em %dd", dCheia);
  display.setCursor(56, 34);
  display.print(buf);

  if (dNova == 0 || dNova >= 30) snprintf(buf, sizeof(buf), "nova hoje!");
  else                           snprintf(buf, sizeof(buf), "nova em %dd", dNova);
  display.setCursor(56, 44);
  display.print(buf);

  // nome da fase centralizado embaixo
  int idx = (int)(luaP * 8.0f + 0.5f) % 8;
  int w = strlen(FASES[idx]) * 6;
  display.setCursor((SCREEN_W - w) / 2, 56);
  display.print(FASES[idx]);
}

// ---------- pagina 3: calendario do mes ----------
void paginaCalendario() {
  const int CAL_X0 = 36;
  char buf[16];
  display.setTextSize(1);

  // coluna da esquerda: mes, ano, luazinha
  display.setCursor(3, 2);
  display.print(MESES_CURTO[hora.tm_mon]);
  snprintf(buf, sizeof(buf), "%d", hora.tm_year + 1900);
  display.setCursor(3, 12);
  display.print(buf);
  desenhaLua(16, 34, 9, luaP);
  int ilum = (int)roundf((1.0f - cosf(2.0f * PI * luaP)) * 50.0f);
  snprintf(buf, sizeof(buf), "%d%%", ilum);
  display.setCursor((33 - (int)strlen(buf) * 6) / 2, 47);
  display.print(buf);
  desenhaPontos(4, 60);
  display.drawFastVLine(33, 0, 64, WHITE);

  // cabecalho dos dias da semana
  const char* ini = "DSTQQSS";
  for (int i = 0; i < 7; i++) {
    display.setCursor(CAL_X0 + i * 13 + 3, 1);
    display.print(ini[i]);
  }
  display.drawFastHLine(CAL_X0, 9, 91, WHITE);

  // dias do mes (o de hoje fica destacado)
  int primeiro = ((hora.tm_wday - ((hora.tm_mday - 1) % 7)) + 7) % 7;
  int nd = diasNoMes(hora.tm_mon, hora.tm_year + 1900);
  for (int dia = 1; dia <= nd; dia++) {
    int pos = primeiro + dia - 1;
    int x = CAL_X0 + (pos % 7) * 13;
    int y = 11 + (pos / 7) * 9;
    snprintf(buf, sizeof(buf), "%2d", dia);
    if (dia == hora.tm_mday) {
      display.fillRect(x - 1, y - 1, 13, 9, WHITE);
      display.setTextColor(BLACK);
      display.setCursor(x, y);
      display.print(buf);
      display.setTextColor(WHITE);
    } else {
      display.setCursor(x, y);
      display.print(buf);
    }
  }
}

void irParaPagina(int dir) {
  pagRelogio = (pagRelogio + dir + N_PAGS_RELOGIO) % N_PAGS_RELOGIO;
  tPag = millis();
  somTique();
}

// Declaracao antecipada (a tela do menu e definida mais abaixo)
extern Botao bUp, bDown, bOk, bBack;

void telaRelogio() {
  // ---- entrada ----
  if (clicou(bUp,   false)) irParaPagina(-1);
  if (clicou(bDown, false)) irParaPagina(+1);
  if (clicou(bOk,   false)) irParaPagina(+1);      // OK tambem alterna as telas
  if (clicou(bBack, false)) {
    somVoltar();
    esperaSoltar();
    telaAtual = TELA_MENU;
    return;
  }

  atualizaHora();

  display.clearDisplay();
  display.setTextColor(WHITE);
  switch (pagRelogio) {
    case 0: paginaDigital();    break;
    case 1: paginaAnalogico();  break;
    case 2: paginaLua();        break;
    case 3: paginaCalendario(); break;
  }

  // animacao de troca de pagina: "cortina" que revela da esquerda pra direita
  unsigned long dt = millis() - tPag;
  if (dt < DUR_WIPE) {
    int rx = (int)(SCREEN_W * dt / DUR_WIPE);
    display.fillRect(rx, 0, SCREEN_W - rx, SCREEN_H, BLACK);
    display.drawFastVLine(rx, 0, SCREEN_H, WHITE);
  }

  display.display();
}



void desenhaSimboloTarot(int cx, int cy, int carta, int s) {
  // Simbolos simplificados para os 22 Arcanos Maiores
  switch (carta) {
    case 0: // O Louco: caminho
      display.drawLine(cx - 7*s, cy + 7*s, cx, cy - 5*s, WHITE);
      display.drawLine(cx, cy - 5*s, cx + 7*s, cy + 7*s, WHITE);
      display.drawCircle(cx, cy - 8*s, 2*s, WHITE);
      break;

    case 1: // O Mago: bastao
      display.drawLine(cx, cy + 9*s, cx, cy - 9*s, WHITE);
      display.drawCircle(cx, cy - 10*s, 2*s, WHITE);
      display.drawLine(cx - 7*s, cy + 8*s, cx + 7*s, cy + 8*s, WHITE);
      break;

    case 2: // Sacerdotisa: lua
    case 18: // A Lua
      display.drawCircle(cx, cy, 8*s, WHITE);
      display.fillCircle(cx + 4*s, cy - 3*s, 7*s, BLACK);
      display.drawCircle(cx, cy, 8*s, WHITE);
      display.drawPixel(cx - 9*s, cy - 10*s, WHITE);
      display.drawPixel(cx + 9*s, cy + 9*s, WHITE);
      break;

    case 3: // Imperatriz: flor
      display.drawCircle(cx, cy, 3*s, WHITE);
      display.drawCircle(cx, cy - 6*s, 3*s, WHITE);
      display.drawCircle(cx + 6*s, cy, 3*s, WHITE);
      display.drawCircle(cx, cy + 6*s, 3*s, WHITE);
      display.drawCircle(cx - 6*s, cy, 3*s, WHITE);
      display.drawLine(cx, cy + 9*s, cx, cy + 14*s, WHITE);
      break;

    case 4: // Imperador: coroa
      display.drawLine(cx - 9*s, cy + 5*s, cx - 7*s, cy - 5*s, WHITE);
      display.drawLine(cx - 7*s, cy - 5*s, cx, cy + 1*s, WHITE);
      display.drawLine(cx, cy + 1*s, cx + 7*s, cy - 5*s, WHITE);
      display.drawLine(cx + 7*s, cy - 5*s, cx + 9*s, cy + 5*s, WHITE);
      display.drawLine(cx - 9*s, cy + 5*s, cx + 9*s, cy + 5*s, WHITE);
      display.drawLine(cx - 7*s, cy + 9*s, cx + 7*s, cy + 9*s, WHITE);
      break;

    case 5: // Hierofante: templo
      display.drawTriangle(cx, cy - 9*s, cx - 9*s, cy - 2*s,
                           cx + 9*s, cy - 2*s, WHITE);
      display.drawLine(cx - 9*s, cy + 9*s, cx + 9*s, cy + 9*s, WHITE);
      for (int i = -1; i <= 1; i++)
        display.drawLine(cx + i*6*s, cy, cx + i*6*s, cy + 8*s, WHITE);
      break;

    case 6: // Enamorados: uniao
      display.drawCircle(cx - 5*s, cy - 2*s, 5*s, WHITE);
      display.drawCircle(cx + 5*s, cy - 2*s, 5*s, WHITE);
      display.drawLine(cx - 9*s, cy + 1*s, cx, cy + 10*s, WHITE);
      display.drawLine(cx + 9*s, cy + 1*s, cx, cy + 10*s, WHITE);
      break;

    case 7: // Carro
      display.drawRoundRect(cx - 9*s, cy - 5*s, 18*s, 13*s, 2*s, WHITE);
      display.drawCircle(cx - 6*s, cy + 10*s, 3*s, WHITE);
      display.drawCircle(cx + 6*s, cy + 10*s, 3*s, WHITE);
      display.drawLine(cx, cy - 11*s, cx, cy - 5*s, WHITE);
      break;

    case 8: // Justica: balanca
      display.drawLine(cx, cy - 10*s, cx, cy + 10*s, WHITE);
      display.drawLine(cx - 9*s, cy - 5*s, cx + 9*s, cy - 5*s, WHITE);
      display.drawLine(cx - 7*s, cy - 5*s, cx - 10*s, cy + 2*s, WHITE);
      display.drawLine(cx - 10*s, cy + 2*s, cx - 4*s, cy + 2*s, WHITE);
      display.drawLine(cx + 7*s, cy - 5*s, cx + 4*s, cy + 2*s, WHITE);
      display.drawLine(cx + 4*s, cy + 2*s, cx + 10*s, cy + 2*s, WHITE);
      display.drawLine(cx - 6*s, cy + 10*s, cx + 6*s, cy + 10*s, WHITE);
      break;

    case 9: // Eremita: lanterna
      display.drawCircle(cx, cy - 2*s, 7*s, WHITE);
      display.drawCircle(cx, cy - 2*s, 3*s, WHITE);
      display.drawLine(cx, cy + 5*s, cx, cy + 11*s, WHITE);
      display.drawLine(cx - 5*s, cy + 11*s, cx + 5*s, cy + 11*s, WHITE);
      break;

    case 10: // Roda da Fortuna
      display.drawCircle(cx, cy, 10*s, WHITE);
      display.drawCircle(cx, cy, 4*s, WHITE);
      display.drawLine(cx, cy - 10*s, cx, cy + 10*s, WHITE);
      display.drawLine(cx - 10*s, cy, cx + 10*s, cy, WHITE);
      display.drawLine(cx - 7*s, cy - 7*s, cx + 7*s, cy + 7*s, WHITE);
      display.drawLine(cx + 7*s, cy - 7*s, cx - 7*s, cy + 7*s, WHITE);
      break;

    case 11: // Forca: infinito
      display.drawLine(cx - 9*s, cy, cx - 5*s, cy - 5*s, WHITE);
      display.drawLine(cx - 5*s, cy - 5*s, cx, cy, WHITE);
      display.drawLine(cx, cy, cx + 5*s, cy + 5*s, WHITE);
      display.drawLine(cx + 5*s, cy + 5*s, cx + 9*s, cy, WHITE);
      display.drawLine(cx + 9*s, cy, cx + 5*s, cy - 5*s, WHITE);
      display.drawLine(cx + 5*s, cy - 5*s, cx, cy, WHITE);
      display.drawLine(cx, cy, cx - 5*s, cy + 5*s, WHITE);
      display.drawLine(cx - 5*s, cy + 5*s, cx - 9*s, cy, WHITE);
      break;

    case 12: // Enforcado
      display.drawLine(cx - 8*s, cy - 8*s, cx + 8*s, cy - 8*s, WHITE);
      display.drawLine(cx, cy - 8*s, cx, cy + 4*s, WHITE);
      display.drawCircle(cx, cy + 7*s, 3*s, WHITE);
      display.drawLine(cx - 7*s, cy + 2*s, cx + 7*s, cy + 2*s, WHITE);
      break;

    case 13: // Morte: foice
      display.drawLine(cx - 5*s, cy + 10*s, cx + 6*s, cy - 9*s, WHITE);
      display.drawCircle(cx + 3*s, cy - 6*s, 6*s, WHITE);
      display.drawLine(cx + 3*s, cy - 12*s, cx + 9*s, cy - 6*s, WHITE);
      break;

    case 14: // Temperanca: dois vasos
      display.drawRoundRect(cx - 10*s, cy - 7*s, 7*s, 12*s, 2*s, WHITE);
      display.drawRoundRect(cx + 3*s, cy + 1*s, 7*s, 12*s, 2*s, WHITE);
      display.drawLine(cx - 3*s, cy - 3*s, cx + 3*s, cy + 5*s, WHITE);
      break;

    case 15: // Diabo: chifres
      display.drawCircle(cx, cy, 7*s, WHITE);
      display.drawLine(cx - 5*s, cy - 5*s, cx - 10*s, cy - 11*s, WHITE);
      display.drawLine(cx + 5*s, cy - 5*s, cx + 10*s, cy - 11*s, WHITE);
      display.drawPixel(cx - 3*s, cy, WHITE);
      display.drawPixel(cx + 3*s, cy, WHITE);
      break;

    case 16: // Torre: raio
      display.drawLine(cx + 3*s, cy - 11*s, cx - 5*s, cy, WHITE);
      display.drawLine(cx - 5*s, cy, cx + 2*s, cy, WHITE);
      display.drawLine(cx + 2*s, cy, cx - 3*s, cy + 11*s, WHITE);
      display.drawRoundRect(cx - 7*s, cy - 12*s, 14*s, 5*s, 1*s, WHITE);
      break;

    case 17: // Estrela
      display.drawLine(cx, cy - 11*s, cx + 3*s, cy - 3*s, WHITE);
      display.drawLine(cx + 3*s, cy - 3*s, cx + 11*s, cy, WHITE);
      display.drawLine(cx + 11*s, cy, cx + 3*s, cy + 3*s, WHITE);
      display.drawLine(cx + 3*s, cy + 3*s, cx, cy + 11*s, WHITE);
      display.drawLine(cx, cy + 11*s, cx - 3*s, cy + 3*s, WHITE);
      display.drawLine(cx - 3*s, cy + 3*s, cx - 11*s, cy, WHITE);
      display.drawLine(cx - 11*s, cy, cx - 3*s, cy - 3*s, WHITE);
      display.drawLine(cx - 3*s, cy - 3*s, cx, cy - 11*s, WHITE);
      break;

    case 19: // Sol
      display.drawCircle(cx, cy, 6*s, WHITE);
      for (int i = 0; i < 8; i++) {
        float a = i * PI / 4.0f;
        display.drawLine(cx + (int)(8*s*cosf(a)),
                         cy + (int)(8*s*sinf(a)),
                         cx + (int)(11*s*cosf(a)),
                         cy + (int)(11*s*sinf(a)), WHITE);
      }
      break;

    case 20: // Julgamento: trombeta
      display.drawLine(cx - 9*s, cy - 5*s, cx + 3*s, cy + 1*s, WHITE);
      display.drawLine(cx + 3*s, cy + 1*s, cx + 3*s, cy + 6*s, WHITE);
      display.drawLine(cx + 3*s, cy + 6*s, cx - 9*s, cy + 1*s, WHITE);
      display.drawLine(cx + 6*s, cy - 5*s, cx + 10*s, cy - 8*s, WHITE);
      display.drawLine(cx + 7*s, cy, cx + 12*s, cy, WHITE);
      break;

    case 21: // Mundo: circulo
      display.drawCircle(cx, cy, 10*s, WHITE);
      display.drawCircle(cx, cy, 6*s, WHITE);
      display.drawLine(cx - 4*s, cy, cx + 4*s, cy, WHITE);
      display.drawLine(cx, cy - 4*s, cx, cy + 4*s, WHITE);
      break;
  }
}

void desenhaCartaTarot(int x, int y, int w, int h,
                       int indice, bool verso) {
  display.drawRoundRect(x, y, w, h, 3, WHITE);
  display.drawRoundRect(x + 3, y + 3, w - 6, h - 6, 2, WHITE);

  if (verso) {
    int cx = x + w/2;
    int cy = y + h/2;
    display.drawCircle(cx, cy, min(w,h)/4, WHITE);
    display.drawCircle(cx, cy, min(w,h)/4 - 3, WHITE);
    display.drawLine(cx, cy - 10, cx + 3, cy - 3, WHITE);
    display.drawLine(cx + 3, cy - 3, cx + 10, cy, WHITE);
    display.drawLine(cx + 10, cy, cx + 3, cy + 3, WHITE);
    display.drawLine(cx + 3, cy + 3, cx, cy + 10, WHITE);
    display.drawLine(cx, cy + 10, cx - 3, cy + 3, WHITE);
    display.drawLine(cx - 3, cy + 3, cx - 10, cy, WHITE);
    display.drawLine(cx - 10, cy, cx - 3, cy - 3, WHITE);
    display.drawLine(cx - 3, cy - 3, cx, cy - 10, WHITE);
  } else if (indice >= 0 && indice < N_CARTAS_TAROT) {
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(x + 5, y + 5);
    display.print(indice + 1);

    int s = (w >= 42) ? 1 : 1;
    desenhaSimboloTarot(x + w/2, y + h/2 + 2, indice, s);

    display.drawFastHLine(x + 7, y + h - 7, w - 14, WHITE);
  }
}

void prepararNovaCartaTarot() {
  if (totalCartasUsadas >= N_CARTAS_TAROT) {
    estadoTarot = TAROT_DECISAO;
    opcaoTarot = 1;
    return;
  }

  cartaAtual = -1;
  estadoTarot = TAROT_EMBARALHANDO;
  tTarot = millis();
}

void telaTarot() {
  unsigned long agora = millis();
  unsigned long dt = agora - tTarot;

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);

  if (estadoTarot == TAROT_ENTRADA) {
    desenhaEstrelas();

    // Lua e moldura ornamental
    display.drawCircle(64, 18, 10, WHITE);
    display.fillCircle(69, 14, 9, BLACK);
    display.drawCircle(64, 18, 10, WHITE);
    display.drawPixel(49, 12, WHITE);
    display.drawPixel(78, 27, WHITE);
    display.drawPixel(44, 27, WHITE);
    display.drawPixel(83, 10, WHITE);

    display.drawRoundRect(14, 32, 100, 25, 4, WHITE);
    display.drawFastHLine(25, 35, 78, WHITE);

    const char* titulo = "O ORACULO";
    display.setCursor((128 - strlen(titulo)*6)/2, 39);
    display.print(titulo);

    if ((agora / 500) % 2 == 0) {
      const char* convite = "OK PARA CONSULTAR";
      display.setCursor((128 - strlen(convite)*6)/2, 49);
      display.print(convite);
    }

    display.display();
    return;
  }

  if (estadoTarot == TAROT_EMBARALHANDO) {
    desenhaEstrelas();

    display.setCursor(26, 2);
    display.print("EMBARALHANDO");

    int fase = (agora / 65) % 3;
    int x1 = 9 + fase * 5;
    int x2 = 47 - fase * 4;
    int x3 = 84 + fase * 3;

    desenhaCartaTarot(x1, 17 + (fase == 0 ? 4 : 0), 28, 39, -1, true);
    desenhaCartaTarot(x2, 13 + (fase == 1 ? 5 : 0), 30, 42, -1, true);
    desenhaCartaTarot(x3, 18 + (fase == 2 ? 3 : 0), 28, 39, -1, true);

    if (dt >= 1750) {
      cartaAtual = sortearCartaTarot();
      estadoTarot = TAROT_REVELANDO;
      tTarot = agora;
      somConfirma();
    }

    display.display();
    return;
  }

  if (estadoTarot == TAROT_REVELANDO) {
    desenhaEstrelas();

    // A carta cresce no centro, como se estivesse sendo revelada
    int w = 22 + min((int)(dt / 18), 22);
    int h = 32 + min((int)(dt / 15), 27);
    int x = (128 - w) / 2;
    int y = (64 - h) / 2;

    if (dt < 250) {
      desenhaCartaTarot(x, y, w, h, -1, true);
    } else {
      desenhaCartaTarot(x, y, w, h, cartaAtual, false);
    }

    if (dt >= 850) {
      estadoTarot = TAROT_INFORMACAO;
      tTarot = agora;
    }

    display.display();
    return;
  }

  if (estadoTarot == TAROT_INFORMACAO) {
    // Carta em miniatura à esquerda
    desenhaCartaTarot(2, 7, 36, 50, cartaAtual, false);

    // Nome e interpretacao à direita
    display.setCursor(42, 8);
    display.print(baralhoTarot[cartaAtual].nome);

    display.drawFastHLine(42, 19, 83, WHITE);

    display.setCursor(42, 24);
    display.print("SIGNIFICADO");

    const char* significado = baralhoTarot[cartaAtual].significado;
    int tamanho = strlen(significado);
    int corte = min(tamanho, 14);

    if (tamanho > corte) {
      while (corte > 0 && significado[corte] != ' ') corte--;
      if (corte == 0) corte = min(tamanho, 14);
    }

    char linha1[16];
    strncpy(linha1, significado, corte);
    linha1[corte] = '\0';

    display.setCursor(42, 36);
    display.print(linha1);

    if (significado[corte] == ' ') {
      display.setCursor(42, 46);
      display.print(significado + corte + 1);
    }

    display.setCursor(42, 57);
    display.print("OK: continuar");

    display.display();
    return;
  }

  if (estadoTarot == TAROT_DECISAO) {
    desenhaEstrelas();

    display.setCursor(24, 5);
    display.print("SUA CONSULTA");

    display.drawRoundRect(8, 17, 112, 40, 4, WHITE);

    if (totalCartasUsadas >= N_CARTAS_TAROT) {
      display.setCursor(20, 25);
      display.print("BARALHO ESGOTADO");
      display.setCursor(22, 41);
      display.print("OK: encerrar");
    } else {
      display.setCursor(22, 22);
      display.print("TIRAR OUTRA?");

      if (opcaoTarot == 0) {
        display.fillRoundRect(17, 35, 44, 15, 3, WHITE);
        display.setTextColor(BLACK);
        display.setCursor(28, 39);
        display.print("SIM");
        display.setTextColor(WHITE);

        display.drawRoundRect(68, 35, 43, 15, 3, WHITE);
        display.setCursor(78, 39);
        display.print("NAO");
      } else {
        display.drawRoundRect(17, 35, 44, 15, 3, WHITE);
        display.setCursor(28, 39);
        display.print("SIM");

        display.fillRoundRect(68, 35, 43, 15, 3, WHITE);
        display.setTextColor(BLACK);
        display.setCursor(78, 39);
        display.print("NAO");
        display.setTextColor(WHITE);
      }
    }

    display.display();
  }
}

void controlarTarot() {
  // BACK sempre encerra a consulta e retorna ao menu
  if (clicou(bBack, false)) {
    somVoltar();
    esperaSoltar();
    telaAtual = TELA_MENU;
    return;
  }

  if (estadoTarot == TAROT_ENTRADA) {
    if (clicou(bOk, false)) {
      esperaSoltar();
      somConfirma();
      prepararNovaCartaTarot();
    }
    return;
  }

  if (estadoTarot == TAROT_INFORMACAO) {
    if (clicou(bOk, false)) {
      esperaSoltar();
      somConfirma();
      estadoTarot = TAROT_DECISAO;
      opcaoTarot = 0;
    }
    return;
  }

  if (estadoTarot == TAROT_DECISAO) {
    if (totalCartasUsadas >= N_CARTAS_TAROT) {
      if (clicou(bOk, false)) {
        esperaSoltar();
        somVoltar();
        telaAtual = TELA_MENU;
      }
      return;
    }

    if (clicou(bUp, false) || clicou(bDown, false)) {
      opcaoTarot = 1 - opcaoTarot;
      somTique();
    }

    if (clicou(bOk, false)) {
      esperaSoltar();
      somConfirma();

      if (opcaoTarot == 0) {
        prepararNovaCartaTarot();
      } else {
        somVoltar();
        telaAtual = TELA_MENU;
      }
    }
  }
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
    if (selecionado == 0) {            // RELOGIO / DATA
      esperaSoltar();
      tPag = millis();
      telaAtual = TELA_RELOGIO;
      return;
    }
    if (selecionado == 8) {
      reiniciarTarot();
      cartaAtual = -1;
      estadoTarot = TAROT_ENTRADA;
      tTarot = millis();
      opcaoTarot = 0;

      esperaSoltar();
      telaAtual = TELA_TAROT;
      return;
    }
    avisoAte = millis() + 1200;        // os outros modulos ainda nao existem
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
  randomSeed(micros());

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

  // hora: fuso + plano B + Wi-Fi em segundo plano
  setenv("TZ", FUSO, 1);
  tzset();
  iniciaHoraCompilacao();
  wifiIniciar();

  sequenciaBoot();
}

void loop() {
  wifiAtualiza();
  switch (telaAtual) {
    case TELA_BOAS_VINDAS: telaBoasVindas(); break;
    case TELA_MENU:        telaMenu();       break;
    case TELA_RELOGIO:     telaRelogio();    break;
    case TELA_TAROT:
      controlarTarot();
      telaTarot();
      break;
  }
  delay(5);   // pequeno para a animacao do menu ficar fluida
}
