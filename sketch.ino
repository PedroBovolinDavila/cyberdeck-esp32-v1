// ============================================================
//  CYBERDECK - Etapa 3: modulo Relogio / Data
//  ESP32 + OLED SSD1306 128x64 (I2C)
// ============================================================
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <time.h>
#include <Preferences.h>

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

int jogoSel = 0;

// ---------------- DISPLAY ----------------
#define SCREEN_W 128
#define SCREEN_H 64
#define OLED_ADDR 0x3C
Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, -1);

enum Tela {
  TELA_BOAS_VINDAS,
  TELA_MENU,
  TELA_RELOGIO,
  TELA_TAROT,
  TELA_PET_QUARTO,
  TELA_PET_STATUS,
  TELA_PET_ACOES,
  TELA_CONFIGURACOES,
  TELA_CFG_TELA,
  TELA_CFG_SOM,
  TELA_JOGOS_MENU,
  TELA_DINO,
  TELA_PONG,
  TELA_VELHA_MENU,
  TELA_VELHA_JOGO,
  TELA_SUDOKU,
  TELA_MEMORIA,
};

Tela telaAtual = TELA_BOAS_VINDAS;

// ===== Variaveis Jogo da Velha =====

char velhaTabuleiro[9];
int velhaCursor = 4;
int velhaModo = 0;          // 0 = contra Purin, 1 = dois jogadores
int velhaDificuldade = 1;   // 0 = facil, 1 = medio, 2 = dificil
int velhaMenuOpcao = 0;
char velhaJogador = 'X';
bool velhaFim = false;
char velhaVencedor = ' ';
int velhaVitoriasX = 0;
int velhaVitoriasO = 0;
int velhaEmpates = 0;


// ===== CONFIGURACOES DO CYBERDECK =====
Preferences preferencias;

int cfgContraste = 200;        // 0 a 255
int cfgVolume = 2;             // 0=mudo, 1=baixo, 2=medio, 3=alto
bool cfgSomTeclas = true;
int cfgTempoDescanso = 60;     // segundos; 0 = desativado

// ===== MENU CONFIGURACOES =====
int cfgCategoria = 0;
int cfgOpcao = 0;
int cfgTelaOpcao = 0;
int cfgSomOpcao = 0;               // 0 = volume | 1 = som das teclas

// protecao de tela (descanso)
bool descansando = false;
unsigned long ultimaAtividade = 0;

const char* cfgCategorias[] = {
  "TELA",
  "SOM",
  "SISTEMA"
};

const int CFG_NUM_CATEGORIAS = 3;

unsigned long cfgUltimaEstrela = 0;

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

int petAcaoSelecionada = 0;
unsigned long petAcaoAte = 0;
const char* petAcaoMensagem = "";
bool petAcaoAnimando = false;
int petTipoAnimacao = -1;
int petHumor = 0;
unsigned long petInicioAnimacao = 0;
bool petGirando = false;
unsigned long tPetGiro = 0;
unsigned long tPetQuarto = 0;
int petFome = 75;
int petEnergia = 85;
int petFelicidade = 90;
unsigned long petUltimaAtualizacao = 0;
int petTicks = 0;
bool petDormindo = false;
bool petAcordando = false;
bool petHorarioValido = false;
int petHoraAtual = 12;
unsigned long petUltimaRecuperacaoSono = 0;


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

void carregarConfiguracoes() {
  preferencias.begin("cyberdeck", false);

  cfgContraste = preferencias.getInt("contraste", 200);
  cfgVolume = preferencias.getInt("volume", 2);
  cfgSomTeclas = preferencias.getBool("somTeclas", true);
  cfgTempoDescanso = preferencias.getInt("descanso", 60);

  // Protecao contra valores invalidos
  cfgContraste = constrain(cfgContraste, 0, 255);
  cfgVolume = constrain(cfgVolume, 0, 3);

  if (cfgTempoDescanso != 0 &&
      cfgTempoDescanso != 30 &&
      cfgTempoDescanso != 60 &&
      cfgTempoDescanso != 120 &&
      cfgTempoDescanso != 300) {
    cfgTempoDescanso = 60;
  }
}

void salvarConfiguracoes() {
  preferencias.putInt("contraste", cfgContraste);
  preferencias.putInt("volume", cfgVolume);
  preferencias.putBool("somTeclas", cfgSomTeclas);
  preferencias.putInt("descanso", cfgTempoDescanso);
}

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
// ---------------- SOM ----------------
// Volume: 0 = mudo | 1 = baixo | 2 = medio | 3 = alto
// (no buzzer passivo o volume vem do "duty" do PWM)
const uint8_t DUTY_VOLUME[4] = { 0, 12, 50, 128 };   // 0 a 255

void iniciaBuzzer() {
  ledcAttach(PIN_BUZZER, 2000, 8);   // pino, frequencia inicial, resolucao
  ledcWrite(PIN_BUZZER, 0);
}

// tecla = true  -> som de interface (respeita "Som teclas")
// tecla = false -> som do sistema (so o MUDO silencia)
void tocaTom(int freq, int ms, bool tecla) {
  if (cfgVolume == 0) return;
  if (tecla && !cfgSomTeclas) return;
  ledcChangeFrequency(PIN_BUZZER, freq, 8);
  ledcWrite(PIN_BUZZER, DUTY_VOLUME[cfgVolume]);
  delay(ms);
  ledcWrite(PIN_BUZZER, 0);
}

void beep(int freq, int ms) { tocaTom(freq, ms, true); }

void somLigar() {
  tocaTom(784, 80, false);    // sol
  tocaTom(988, 80, false);    // si
  tocaTom(1319, 140, false);  // mi
}

void somTique()    { beep(2400, 8); }    // navegar no menu
void somConfirma() { beep(1568, 60); }   // OK
void somVoltar()   { beep(988, 60); }    // BACK
void somErro() {
  tocaTom(220, 180, false);
  delay(40);
  tocaTom(160, 250, false);
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


void atualizaHorarioPet() {
  time_t agora = time(nullptr);

  // Verifica se o relogio ja recebeu um horario valido
  if (agora > 1700000000) {
    // Horario de Brasilia: UTC-3
    agora -= 3 * 60 * 60;

    struct tm horario;
    gmtime_r(&agora, &horario);

    petHoraAtual = horario.tm_hour;
    petHorarioValido = true;
  }
}
void abrirMenuJogos() {
  carregarRecordes();
  jogoSel = 0;
  telaAtual = TELA_JOGOS_MENU;
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
    if (selecionado == 5) {            // CONFIGURACOES
      esperaSoltar();
      cfgCategoria = 0;
      telaAtual = TELA_CONFIGURACOES;
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
    if (selecionado == 4) {
      esperaSoltar();
      petGirando = false;
      tPetQuarto = millis();
      telaAtual = TELA_PET_QUARTO;
      return;
    }
    if (selecionado == 3) {            // MINI JOGOS
      esperaSoltar();
      abrirMenuJogos();
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


 // ============================================================
 // PET VIRTUAL — QUARTO DO POMPOMPURIN
 // ============================================================


void atualizaPet() {
  unsigned long agora = millis();

  if (petUltimaAtualizacao == 0) {
    petUltimaAtualizacao = agora;
    return;
  }

  if (agora - petUltimaAtualizacao >= 60000UL) {
    petUltimaAtualizacao = agora;
    petTicks++;

    // A fome diminui mesmo durante o sono
    if (petFome > 0) petFome--;

    if (petDormindo) {
      // Recuperacao gradual de energia
      petEnergia += 5;
      if (petEnergia > 100) petEnergia = 100;
    } else {
      if (petEnergia > 0 && petTicks % 2 == 0) {
        petEnergia--;
      }

      if (petFelicidade > 0 && petTicks % 3 == 0) {
        petFelicidade--;
      }
    }
  }
}





void desenhaQuarto() {
  bool noite = petHorarioValido &&
               (petHoraAtual >= 18 || petHoraAtual < 6);

  // Janela
  display.drawRoundRect(5, 5, 27, 23, 3, WHITE);
  display.drawFastVLine(18, 7, 19, WHITE);
  display.drawFastHLine(7, 16, 23, WHITE);

  if (noite) {
    // Lua crescente estilizada
    display.drawCircle(11, 11, 4, WHITE);
    display.fillCircle(13, 9, 3, BLACK);
    display.drawPixel(24, 10, WHITE);
    display.drawPixel(26, 22, WHITE);
    display.drawPixel(8, 21, WHITE);
    display.drawPixel(24, 13, WHITE);
  } else {
    // Sol durante o dia
    display.drawCircle(11, 11, 3, WHITE);
    display.drawPixel(11, 6, WHITE);
    display.drawPixel(11, 16, WHITE);
    display.drawPixel(6, 11, WHITE);
    display.drawPixel(16, 11, WHITE);
    display.drawPixel(24, 10, WHITE);
  }

  // Prateleira
  display.drawFastHLine(4, 31, 28, WHITE);
  display.drawFastVLine(7, 31, 4, WHITE);
  display.drawFastVLine(29, 31, 4, WHITE);

  // Quadrinho
  display.drawRoundRect(99, 5, 23, 19, 2, WHITE);
  display.drawPixel(110, 10, WHITE);
  display.drawLine(104, 18, 110, 12, WHITE);
  display.drawLine(110, 12, 117, 19, WHITE);

  // Chao
  display.drawFastHLine(0, 58, 128, WHITE);

  // Caminha
  display.drawRoundRect(83, 43, 40, 15, 3, WHITE);
  display.drawFastHLine(86, 47, 34, WHITE);
  display.drawRoundRect(87, 44, 11, 6, 2, WHITE);

  // Pes da cama
  display.drawFastVLine(88, 58, 3, WHITE);
  display.drawFastVLine(118, 58, 3, WHITE);

  // Estrelinhas decorativas
  display.drawPixel(40, 12, WHITE);
  display.drawFastHLine(37, 19, 3, WHITE);
  display.drawFastVLine(39, 17, 5, WHITE);

  display.drawPixel(91, 29, WHITE);
  display.drawFastHLine(88, 30, 5, WHITE);
  display.drawFastVLine(90, 28, 5, WHITE);

  // Mais estrelas a noite
  if (noite) {
    display.drawPixel(23, 8, WHITE);
    display.drawPixel(26, 18, WHITE);
    display.drawPixel(15, 22, WHITE);
  }

  // Plantinha
  display.drawLine(9, 51, 9, 43, WHITE);
  display.drawLine(9, 47, 5, 44, WHITE);
  display.drawLine(9, 46, 13, 42, WHITE);
  display.drawLine(5, 52, 13, 52, WHITE);
  display.drawLine(6, 53, 12, 53, WHITE);
}


void desenhaPurinQuarto(int cx, int cy, bool piscando,
                        bool bocaAberta, bool deCostas,
                        int respiracao) {
  int y = cy + respiracao;

  // Silhueta traseira durante a rodadinha
  if (deCostas) {
    display.fillRoundRect(cx - 15, y - 13, 30, 23, 9, BLACK);
    display.drawRoundRect(cx - 15, y - 13, 30, 23, 9, WHITE);

    // Orelhinhas
    display.fillRoundRect(cx - 20, y - 9, 7, 13, 3, BLACK);
    display.drawRoundRect(cx - 20, y - 9, 7, 13, 3, WHITE);
    display.fillRoundRect(cx + 13, y - 9, 7, 13, 3, BLACK);
    display.drawRoundRect(cx + 13, y - 9, 7, 13, 3, WHITE);

    // Boina
    display.fillRoundRect(cx - 10, y - 17, 20, 6, 3, WHITE);
    display.fillRect(cx - 1, y - 19, 3, 2, WHITE);

    // Asterisco caracteristico
    display.drawFastHLine(cx - 3, y + 1, 7, WHITE);
    display.drawFastVLine(cx, y - 2, 7, WHITE);
    display.drawLine(cx - 3, y - 2, cx + 3, y + 4, WHITE);
    display.drawLine(cx - 3, y + 4, cx + 3, y - 2, WHITE);
    return;
  }

  // Patinhas
  display.fillRoundRect(cx - 10, y + 7, 8, 4, 2, WHITE);
  display.fillRoundRect(cx + 2, y + 7, 8, 4, 2, WHITE);

  // Orelhas caidinhas
  display.fillRoundRect(cx - 20, y - 8, 7, 15, 3, BLACK);
  display.drawRoundRect(cx - 20, y - 8, 7, 15, 3, WHITE);
  display.fillRoundRect(cx + 13, y - 8, 7, 15, 3, BLACK);
  display.drawRoundRect(cx + 13, y - 8, 7, 15, 3, WHITE);

  // Cabeca
  display.fillRoundRect(cx - 16, y - 18, 32, 27, 10, BLACK);
  display.drawRoundRect(cx - 16, y - 18, 32, 27, 10, WHITE);

  // Boina marrom representada em pixels brancos
  display.fillRoundRect(cx - 11, y - 23, 22, 6, 3, WHITE);
  display.fillRect(cx - 1, y - 25, 3, 2, WHITE);

  // Olhos: piscada ou olhos abertos
  if (piscando) {
    display.drawFastHLine(cx - 8, y - 7, 4, WHITE);
    display.drawFastHLine(cx + 5, y - 7, 4, WHITE);
  } else {
    display.fillCircle(cx - 6, y - 7, 1, WHITE);
    display.fillCircle(cx + 6, y - 7, 1, WHITE);
  }

  // Focinho
  display.drawRoundRect(cx - 5, y - 4, 10, 6, 2, WHITE);
  display.drawPixel(cx, y - 3, WHITE);

  // Boquinha muda entre fechada e aberta
  if (bocaAberta) {
    display.drawRoundRect(cx - 2, y + 1, 5, 4, 2, WHITE);
    display.drawPixel(cx, y + 2, WHITE);
  } else {
    display.drawFastHLine(cx - 2, y + 2, 5, WHITE);
  }

  // Bochechinhas
  display.drawFastHLine(cx - 12, y - 1, 3, WHITE);
  display.drawFastHLine(cx + 10, y - 1, 3, WHITE);
}

void telaPetQuarto() {
  unsigned long agora = millis();
  unsigned long dt = agora - tPetGiro;

  display.clearDisplay();
  display.setTextColor(WHITE);
  desenhaQuarto();

  // Estrelas cintilantes
  if ((agora / 350) % 2 == 0) {
    display.drawPixel(47, 24, WHITE);
    display.drawFastHLine(44, 25, 7, WHITE);
    display.drawFastVLine(47, 22, 7, WHITE);
  }

  if ((agora / 500) % 2 == 0) {
    display.drawPixel(76, 14, WHITE);
    display.drawFastHLine(74, 15, 5, WHITE);
    display.drawFastVLine(76, 13, 5, WHITE);
  }

  
  // -------- ANIMACAO DE DORMIR --------
  if (petDormindo) {
    unsigned long tempoSono = millis() - petInicioAnimacao;

    int cx = 103;
    int cy = 43;

    // Fase 1: Purin pula em direcao a caminha
    if (tempoSono < 700) {
      cx = 62 + (41L * tempoSono / 700);

      // O corpinho sobe e depois desce
      if (tempoSono < 250) {
        cy = 43 - (13L * tempoSono / 250);
      } else if (tempoSono < 500) {
        cy = 30 + (13L * (tempoSono - 250) / 250);
      } else {
        cy = 43;
      }

      desenhaPurinQuarto(
        cx, cy,
        false, false,
        false, 0
      );
    } else {
      // Fase 2: Purin se acomoda na caminha
      bool respirar = ((tempoSono / 700) % 2 == 0);

      desenhaPurinQuarto(
        103, 43,
        true, false,
        false, respirar ? -1 : 0
      );

      // Fase 3: O cobertor sobe e cobre o corpinho
      display.fillRoundRect(85, 43, 36, 13, 3, BLACK);
      display.drawRoundRect(85, 43, 36, 13, 3, WHITE);

      // Dobras do cobertor
      display.drawFastHLine(89, 47, 27, WHITE);
      display.drawLine(94, 48, 98, 54, WHITE);
      display.drawLine(98, 54, 103, 48, WHITE);

      // Cabecinha continua visivel acima do cobertor
      display.drawPixel(96, 32, WHITE);
      display.drawPixel(109, 32, WHITE);

      // Fase 4: Z's flutuam enquanto ele dorme
      if (tempoSono > 1100) {
        int ciclo = (tempoSono / 180) % 12;
        int zy = 29 - ciclo;

        display.setTextSize(1);
        display.setCursor(108, zy);
        display.print("z");

        display.setCursor(98, zy + 7);
        display.print("Z");

        if ((tempoSono / 500) % 2 == 0) {
          display.drawPixel(117, zy - 2, WHITE);
        }
      }
    }

    display.display();
    return;
  }
  // ------ FIM DA ANIMACAO DE DORMIR ------

  
  // -------- ANIMACAO DE DESPERTAR --------
  if (petAcordando) {
    unsigned long tempoAcordar = millis() - petInicioAnimacao;

    int cx = 103;
    int cy = 43;
    bool olhosFechados = false;
    bool bocaAberta = false;

    if (tempoAcordar < 400) {
      // 1. Mexe-se debaixo do cobertor
      cx = 103 + ((tempoAcordar / 100) % 2 == 0 ? -2 : 2);
      olhosFechados = true;

      desenhaPurinQuarto(
        cx, cy, olhosFechados, false, false, 0
      );

      display.fillRoundRect(85, 43, 36, 13, 3, BLACK);
      display.drawRoundRect(85, 43, 36, 13, 3, WHITE);
      display.drawFastHLine(89, 47, 27, WHITE);

    } else if (tempoAcordar < 800) {
      // 2. O cobertor escorrega para baixo
      int deslocamento = (tempoAcordar - 400) * 15 / 400;

      desenhaPurinQuarto(
        103, 43, true, false, false, 0
      );

      display.fillRoundRect(
        85, 43 + deslocamento, 36, 13, 3, BLACK
      );
      display.drawRoundRect(
        85, 43 + deslocamento, 36, 13, 3, WHITE
      );

    } else if (tempoAcordar < 1250) {
      // 3. Espreguica-se
      int fase = (tempoAcordar / 150) % 2;
      cy = fase == 0 ? 41 : 38;
      bocaAberta = true;

      desenhaPurinQuarto(
        103, cy, false, bocaAberta, false, 0
      );

      display.drawPixel(91, 30, WHITE);
      display.drawPixel(115, 30, WHITE);
      display.drawFastHLine(88, 27, 4, WHITE);
      display.drawFastHLine(114, 27, 4, WHITE);

    } else if (tempoAcordar < 1750) {
      // 4. Da um pulinho para sair da cama
      unsigned long salto = tempoAcordar - 1250;

      cx = 103 - (41L * salto / 500);

      if (salto < 250) {
        cy = 43 - (12L * salto / 250);
      } else {
        cy = 31 + (12L * (salto - 250) / 250);
      }

      desenhaPurinQuarto(
        cx, cy, false, true, false, 0
      );

      display.drawPixel(cx - 12, cy - 13, WHITE);
      display.drawPixel(cx + 12, cy - 13, WHITE);

    } else {
      // Animacao concluida
      petAcordando = false;
    }

    if (petAcordando) {
      display.display();
      return;
    }
  }
  // ------ FIM DA ANIMACAO DE DESPERTAR ------


  // Respiracao suave
  int respiracao = ((agora % 1800) < 900) ? 0 : -1;

  // Animacao da rodadinha
  bool deCostas = false;
  int cx = 62;

  if (petGirando) {
    if (dt >= 1000) {
      petGirando = false;
    } else {
      int fase = dt / 200;

      // Pequeno deslocamento lateral para dar sensacao de giro
      if (fase == 1 || fase == 3) cx = 68;
      if (fase == 2) deCostas = true;
    }
  }

  bool piscando = ((agora % 3000) < 150);
  bool bocaAberta = ((agora % 2300) > 1650 &&
                     (agora % 2300) < 1900);

  desenhaPurinQuarto(cx, 43, piscando, bocaAberta,
                     deCostas, respiracao);

  // Indicacao discreta de navegacao
  display.setTextSize(1);
  display.setCursor(39, 59);

  display.display();
}


void telaPetStatus() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);

  // Titulo
  display.setCursor(35, 0);
  display.print("MEU PURIN");

  display.drawFastHLine(0, 9, 128, WHITE);

  unsigned long agora = millis();

  // Desenho do Purin: a expressao muda conforme os atributos
  bool cansado = petEnergia < 25;
  bool faminto = petFome < 25;
  bool piscando = petDormindo || ((agora % 3000) < 150);
  bool bocaAberta = !petDormindo &&
                  ((agora % 2300) > 1650 &&
                   (agora % 2300) < 1900);

  desenhaPurinQuarto(
    20, 44,
    piscando,
    bocaAberta,
    false,
    0
  );

  if (petDormindo) {
    display.setTextSize(1);
    display.setCursor(76, 34);
    display.print("Zzz");
  }

  // FOME
  display.setCursor(43, 12);
  display.print("FOME ");
  display.print(petFome);

  display.drawRect(43, 21, 81, 6, WHITE);
  if (petFome > 0) {
    display.fillRect(
      45, 23,
      (petFome * 77) / 100, 2,
      WHITE
    );
  }

  // ENERGIA
  display.setCursor(43, 30);
  display.print("ENERGIA ");
  display.print(petEnergia);

  display.drawRect(43, 39, 81, 6, WHITE);
  if (petEnergia > 0) {
    display.fillRect(
      45, 41,
      (petEnergia * 77) / 100, 2,
      WHITE
    );
  }

  // FELICIDADE
  display.setCursor(43, 48);
  display.print("ALEGRIA ");
  display.print(petFelicidade);

  display.drawRect(43, 57, 81, 6, WHITE);
  if (petFelicidade > 0) {
    display.fillRect(
      45, 59,
      (petFelicidade * 77) / 100, 2,
      WHITE
    );
  }

  display.display();
}


void controlarPetStatus() {
  if (clicou(bBack, false) || clicou(bDown, false)) {
    somVoltar();
    esperaSoltar();
    telaAtual = TELA_PET_QUARTO;
    return;
  }

  if (clicou(bOk, false)) {
    somConfirma();
    esperaSoltar();
    petAcaoSelecionada = 0;
    telaAtual = TELA_PET_ACOES;
    return;
  }
}


void controlarPetAcoes() {
  if (clicou(bBack, false)) {
    somVoltar();
    esperaSoltar();
    telaAtual = TELA_PET_STATUS;
    return;
  }

  if (clicou(bUp, false)) {
    petAcaoSelecionada--;
    if (petAcaoSelecionada < 0) petAcaoSelecionada = 3;
    somConfirma();
    esperaSoltar();
    return;
  }

  if (clicou(bDown, false)) {
    petAcaoSelecionada++;
    if (petAcaoSelecionada > 3) petAcaoSelecionada = 0;
    somConfirma();
    esperaSoltar();
    return;
  }

  if (clicou(bOk, false)) {
    esperaSoltar();

    switch (petAcaoSelecionada) {
      case 0:
        // Alimentar
        petFome += 35;
        if (petFome > 100) petFome = 100;

        petAcaoMensagem = "NHAM!!";
        break;

      case 1:
        // Brincar
        petFelicidade += 20;
        if (petFelicidade > 100) petFelicidade = 100;

        petEnergia -= 15;
        if (petEnergia < 0) petEnergia = 0;

        petFome -= 8;
        if (petFome < 0) petFome = 0;

        petAcaoMensagem = "UHUU VAMOS!";
        break;

      case 2:
        // Carinho
        petFelicidade += 15;
        if (petFelicidade > 100) petFelicidade = 100;

        petAcaoMensagem = "AU AU! <3";
        break;

      
      case 3:
        // Iniciar sono real
        petDormindo = true;
        petUltimaRecuperacaoSono = millis();

        petTipoAnimacao = 3;
        petInicioAnimacao = millis();
        petAcaoAnimando = false;
        petGirando = false;

        petAcaoMensagem = "ZZZ...";
        somConfirma();
        telaAtual = TELA_PET_QUARTO;
        return;

    }

    petTipoAnimacao = petAcaoSelecionada;
    petInicioAnimacao = millis();

    petAcaoAte = millis() + 2200;
    petAcaoAnimando = true;

    somConfirma();
  }
}


void telaPetAcoes() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);

  display.setCursor(25, 0);
  display.print("CUIDAR DO PURIN");
  display.drawFastHLine(0, 9, 128, WHITE);

  const char* nomes[] = {
    "Alimentar",
    "Brincar",
    "Carinho",
    "Dormir"
  };

  unsigned long agora = millis();

  if (petAcaoAnimando && agora < petAcaoAte) {
    unsigned long tempo = agora - petInicioAnimacao;

    // Movimento suave para cada acao
    int cx = 35;
    int cy = 39;
    int respiracao = 0;
    bool costas = false;
    bool olhosFechados = false;
    bool bocaAberta = false;

    switch (petTipoAnimacao) {
      case 0: // COMER
        bocaAberta = (tempo % 400 < 200);
        cy = 39;
        break;

      case 1: // BRINCAR
        cx = 35 + ((tempo / 180) % 2 == 0 ? -5 : 5);
        cy = 36 + ((tempo / 180) % 2 == 0 ? -4 : 2);
        bocaAberta = true;
        break;

      case 2: // CARINHO
        olhosFechados = (tempo % 700 < 450);
        respiracao = (tempo % 500 < 250) ? -1 : 0;
        bocaAberta = true;
        break;

      case 3: // DORMIR
        olhosFechados = true;
        respiracao = (tempo % 1000 < 500) ? -1 : 0;
        cy = 42;
        break;
    }

    desenhaPurinQuarto(
      cx, cy,
      olhosFechados,
      bocaAberta,
      costas,
      respiracao
    );

    // Reacoes ao lado do Purin
    display.setCursor(66, 16);

    switch (petTipoAnimacao) {
      case 0:
        display.print("NHAM!");
        display.setCursor(66, 25);
        display.print("NHAM!");
        break;

      case 1:
        display.print("UHUU!");
        display.setCursor(66, 25);
        display.print("* * *");
        display.drawPixel(92, 35, WHITE);
        display.drawPixel(102, 31, WHITE);
        break;

      case 2:
        display.print("<3 <3");
        display.setCursor(66, 25);
        display.print("AMEI!");
        break;

      case 3:
        display.print("Z z z");
        display.setCursor(66, 25);
        display.print("ZZZ...");
        break;
    }

    display.setCursor(66, 41);
    display.print("F:");
    display.print(petFome);

    display.setCursor(66, 51);
    display.print("E:");
    display.print(petEnergia);

  } else {
    petAcaoAnimando = false;

    for (int i = 0; i < 4; i++) {
      int y = 14 + i * 11;

      if (i == petAcaoSelecionada) {
        display.fillRoundRect(5, y - 1, 118, 10, 2, WHITE);
        display.setTextColor(BLACK);
        display.setCursor(10, y);
        display.print(">");
        display.setCursor(23, y);
        display.print(nomes[i]);
        display.setTextColor(WHITE);
      } else {
        display.setCursor(23, y);
        display.print(nomes[i]);
      }
    }
  }

  display.display();
}





void controlarPetQuarto() {
  if (clicou(bBack, false)) {
    somVoltar();
    esperaSoltar();
    telaAtual = TELA_MENU;
    return;
  }

  // Dormindo: OK acorda; outras acoes ficam bloqueadas
  
  if (petDormindo) {
    if (clicou(bOk, false)) {
      petDormindo = false;
      petAcordando = true;

      petInicioAnimacao = millis();
      petAcaoAnimando = false;
      petGirando = false;

      somConfirma();
      esperaSoltar();
    }
    return;
  }

  // Enquanto acorda, espera a animacao terminar
  if (petAcordando) {
    return;
  }


  // UP abre status quando esta acordado
  if (clicou(bUp, false)) {
    somConfirma();
    esperaSoltar();
    telaAtual = TELA_PET_STATUS;
    return;
  }

  if (clicou(bOk, false) && !petGirando) {
    petGirando = true;
    tPetGiro = millis();
    somConfirma();
  }
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

// -- CONFIGURACOES --

void telaConfiguracoes() {
  display.clearDisplay();

  // Estrelinhas decorativas
  for (int i = 0; i < 9; i++) {
    int x = (i * 19 + 7) % 128;
    int y = (i * 13 + 3) % 64;

    if ((millis() / 350 + i) % 3 != 0) {
      display.drawPixel(x, y, WHITE);
    } else {
      display.drawPixel(x - 1, y, WHITE);
      display.drawPixel(x + 1, y, WHITE);
      display.drawPixel(x, y - 1, WHITE);
      display.drawPixel(x, y + 1, WHITE);
    }
  }

  display.drawRoundRect(2, 2, 124, 60, 5, WHITE);

  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(15, 7);
  display.print("CONFIGURACOES");

  display.drawFastHLine(8, 18, 112, WHITE);

  for (int i = 0; i < CFG_NUM_CATEGORIAS; i++) {
    int y = 23 + i * 12;

    if (i == cfgCategoria) {
      display.fillRoundRect(9, y - 2, 110, 11, 3, WHITE);
      display.setTextColor(BLACK);
      display.setCursor(15, y);
      display.print(">");
      display.print(cfgCategorias[i]);
      display.setTextColor(WHITE);
    } else {
      display.setCursor(15, y);
      display.print(" ");
      display.print(cfgCategorias[i]);
    }
  }

  display.display();
}

void controlarConfiguracoes() {
  if (clicou(bUp, false)) {
    cfgCategoria--;
    if (cfgCategoria < 0) {
      cfgCategoria = CFG_NUM_CATEGORIAS - 1;
    }
    somTique();
    esperaSoltar();
  }

  if (clicou(bDown, false)) {
    cfgCategoria++;
    if (cfgCategoria >= CFG_NUM_CATEGORIAS) {
      cfgCategoria = 0;
    }
    somTique();
    esperaSoltar();
  }

  if (clicou(bBack, false)) {
    somVoltar();
    esperaSoltar();
    telaAtual = TELA_MENU;
    return;
  }

  if (clicou(bOk, false)) {
    somConfirma();
    esperaSoltar();

    if (cfgCategoria == 0) {
      cfgTelaOpcao = 0;
      telaAtual = TELA_CFG_TELA;
    } else if (cfgCategoria == 1) {
      cfgSomOpcao = 0;
      telaAtual = TELA_CFG_SOM;
    } else {
      cfgOpcao = 0;
      // O submenu SISTEMA sera implementado na proxima etapa.
    }
  }
}

void telaConfigTela() {
  display.clearDisplay();

  // Estrelinhas animadas
  for (int i = 0; i < 8; i++) {
    int x = (i * 23 + 11) % 128;
    int y = (i * 17 + 5) % 64;

    if ((millis() / 300 + i) % 3 == 0) {
      display.drawPixel(x, y, WHITE);
      display.drawPixel(x - 1, y, WHITE);
      display.drawPixel(x + 1, y, WHITE);
      display.drawPixel(x, y - 1, WHITE);
      display.drawPixel(x, y + 1, WHITE);
    } else {
      display.drawPixel(x, y, WHITE);
    }
  }

  display.drawRoundRect(2, 2, 124, 60, 5, WHITE);

  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(12, 7);
  display.print("CONFIG > TELA");

  display.drawFastHLine(8, 18, 112, WHITE);

  // Opcao 0: contraste
  if (cfgTelaOpcao == 0) {
    display.fillRoundRect(7, 22, 114, 19, 3, WHITE);
    display.setTextColor(BLACK);
  } else {
    display.setTextColor(WHITE);
  }

  display.setCursor(11, 24);
  display.print("Contraste");
  display.setCursor(83, 24);
  display.print(cfgContraste);

  // Barra de contraste
  int largura = map(cfgContraste, 0, 255, 0, 48);
  display.drawRect(68, 34, 50, 4, cfgTelaOpcao == 0 ? BLACK : WHITE);
  if (largura > 0) {
    display.fillRect(69, 35, largura, 2,
                     cfgTelaOpcao == 0 ? BLACK : WHITE);
  }

  // Opcao 1: tempo de descanso
  display.setTextColor(cfgTelaOpcao == 1 ? BLACK : WHITE);
  if (cfgTelaOpcao == 1) {
    display.fillRoundRect(7, 44, 114, 13, 3, WHITE);
  }

  display.setCursor(11, 47);
  display.print("Descanso:");

  display.setCursor(72, 47);
  if (cfgTempoDescanso == 0) {
    display.print("OFF");
  } else if (cfgTempoDescanso == 30) {
    display.print("30s");
  } else if (cfgTempoDescanso == 60) {
    display.print("1min");
  } else if (cfgTempoDescanso == 120) {
    display.print("2min");
  } else {
    display.print("5min");
  }

  display.display();
}

void controlarConfigTela() {
  if (clicou(bBack, false)) {
    somVoltar();
    salvarConfiguracoes();
    esperaSoltar();
    telaAtual = TELA_CONFIGURACOES;
    return;
  }

  if (clicou(bUp, false)) {
    if (cfgTelaOpcao > 0) {
      cfgTelaOpcao--;
    } else {
      if (cfgContraste < 255) cfgContraste += 5;
      cfgContraste = constrain(cfgContraste, 0, 255);
      display.ssd1306_command(SSD1306_SETCONTRAST);
      display.ssd1306_command(cfgContraste);
    }

    somTique();
    esperaSoltar();
  }

  if (clicou(bDown, false)) {
    if (cfgTelaOpcao == 0) {
      if (cfgContraste > 0) cfgContraste -= 5;
      cfgContraste = constrain(cfgContraste, 0, 255);
      display.ssd1306_command(SSD1306_SETCONTRAST);
      display.ssd1306_command(cfgContraste);
    } else {
      const int tempos[] = {0, 30, 60, 120, 300};
      int indice = 0;

      for (int i = 0; i < 5; i++) {
        if (tempos[i] == cfgTempoDescanso) indice = i;
      }

      indice = (indice + 1) % 5;
      cfgTempoDescanso = tempos[indice];
    }

    somTique();
    esperaSoltar();
  }

  if (clicou(bOk, false)) {
    cfgTelaOpcao = 1 - cfgTelaOpcao;
    somConfirma();
    esperaSoltar();
  }
}

// ============================================================
//  CONFIG > SOM
//  CIMA/BAIXO escolhem a linha | OK muda o valor | BACK salva e volta
// ============================================================
void telaConfigSom() {
  display.clearDisplay();
  display.drawRoundRect(2, 2, 124, 60, 5, WHITE);

  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(15, 7);
  display.print("CONFIG > SOM");
  display.drawFastHLine(8, 18, 112, WHITE);

  for (int i = 0; i < 2; i++) {
    int y = 22 + i * 20;
    bool sel = (i == cfgSomOpcao);
    uint16_t cor = sel ? BLACK : WHITE;

    if (sel) display.fillRoundRect(7, y, 114, 17, 3, WHITE);
    display.setTextColor(cor);

    if (i == 0) {
      display.setCursor(11, y + 5);
      display.print("Volume");

      if (cfgVolume == 0) {
        display.setCursor(80, y + 5);
        display.print("MUDO");
      } else {
        // 3 barrinhas crescentes
        for (int b = 0; b < 3; b++) {
          int bx = 80 + b * 12;
          int bh = 4 + b * 3;
          int topo = (y + 14) - bh;
          if (cfgVolume > b) display.fillRect(bx, topo, 8, bh, cor);
          else               display.drawRect(bx, topo, 8, bh, cor);
        }
      }
    } else {
      display.setCursor(11, y + 5);
      display.print("Som teclas");
      display.setCursor(92, y + 5);
      display.print(cfgSomTeclas ? "ON" : "OFF");
    }
  }

  display.setTextColor(WHITE);
  display.display();
}

void controlarConfigSom() {
  if (clicou(bBack, false)) {
    somVoltar();
    salvarConfiguracoes();
    esperaSoltar();
    telaAtual = TELA_CONFIGURACOES;
    return;
  }

  if (clicou(bUp, false) || clicou(bDown, false)) {
    cfgSomOpcao = 1 - cfgSomOpcao;
    somTique();
    esperaSoltar();
  }

  if (clicou(bOk, false)) {
    if (cfgSomOpcao == 0) {
      cfgVolume = (cfgVolume + 1) % 4;       // 0 > 1 > 2 > 3 > 0
      tocaTom(1568, 80, false);              // toca um teste no novo volume
    } else {
      cfgSomTeclas = !cfgSomTeclas;
      beep(1568, 60);                        // so soa quando liga
    }
    esperaSoltar();
  }
}

// ============================================================
//  DESCANSO (protecao de tela)
//  Depois de X segundos sem apertar nada, o Purin dorme com a
//  tela fraquinha (poupa o OLED). Qualquer botao acorda.
// ============================================================
void entrarDescanso() {
  descansando = true;
  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(8);                // brilho bem baixo
}

void sairDescanso() {
  descansando = false;
  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(cfgContraste);     // volta ao brilho escolhido
  ultimaAtividade = millis();
}

void telaDescanso() {
  unsigned long t = millis();
  atualizaHora();

  // o desenho passeia devagar pela tela (evita "queimar" o OLED)
  int cx = 64 + (int)(sinf(t / 4000.0f) * 14);
  int cy = 34 + (int)(sinf(t / 5300.0f) * 5);
  int respiracao = ((t % 3000) < 1500) ? 0 : -1;

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);

  desenhaPurinQuarto(cx, cy, true, false, false, respiracao);

  // Z's subindo
  int ciclo = (t / 250) % 14;
  display.setCursor(cx + 22, cy - 14 - ciclo / 2);
  display.print("z");
  display.setCursor(cx + 28, cy - 20 - ciclo / 2);
  display.print("Z");

  // hora discreta embaixo (so se veio da internet)
  if (horaSync) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", hora.tm_hour, hora.tm_min);
    display.setCursor(49 + (cx - 64) / 2, 56);
    display.print(buf);
  }

  display.display();
}

// ============================================================
//  MINI JOGOS - etapa 1: base + Dino Purin + Pong
//  (nao mude a ordem: os tipos precisam vir antes das funcoes)
// ============================================================
#define N_JOGOS 5
const char* NOMES_JOGOS[N_JOGOS] = { "Dino Purin", "Pong", "Jogo da Velha", "Memoria", "Sudoku" };

enum EstadoJogo { JG_INTRO, JG_JOGANDO, JG_PONTO, JG_VITORIA, JG_FIM };


unsigned long avisoJogoAte = 0;
int recordes[N_JOGOS] = { 0, 0, 0, 0, 0 };   // 0 dino | 1 pong | 2 velha | 3 memoria | 4 sudoku



void telaVelhaMenu() {
  display.clearDisplay();

  display.drawRoundRect(1, 1, 126, 62, 5, WHITE);
  display.drawRoundRect(4, 4, 120, 56, 3, WHITE);

  display.fillRoundRect(12, 5, 104, 10, 2, WHITE);
  display.setTextColor(BLACK);
  display.setTextSize(1);
  display.setCursor(27, 7);
  display.print("JOGO DA VELHA");

  // Estrelas decorativas
  for (int i = 0; i < 5; i++) {
    int x = 15 + i * 24;
    int y = 19 + (i % 2) * 2;
    if ((millis() / 300 + i) % 2 == 0) {
      display.drawPixel(x, y, WHITE);
    }
  }

  const char* opcoes[] = {
    "Contra o Purin",
    "2 jogadores",
    "Dificuldade",
    "Jogar!"
  };

  for (int i = 0; i < 4; i++) {
    int y = 23 + i * 9;

    if (velhaMenuOpcao == i) {
      display.fillRoundRect(10, y, 108, 8, 2, WHITE);
      display.setTextColor(BLACK);
    } else {
      display.setTextColor(WHITE);
    }

    display.setCursor(14, y + 1);
    display.print(opcoes[i]);

    if (i == 0) {
      display.setCursor(88, y + 1);
      display.print(velhaModo == 0 ? "ON" : "OFF");
    } else if (i == 1) {
      display.setCursor(88, y + 1);
      display.print(velhaModo == 1 ? "ON" : "OFF");
    } else if (i == 2) {
      display.setCursor(83, y + 1);
      if (velhaDificuldade == 0) display.print("FACIL");
      else if (velhaDificuldade == 1) display.print("MED");
      else display.print("DIF");
    }
  }

  display.display();
}


void controlarVelhaMenu() {
  if (clicou(bUp, true)) {
    velhaMenuOpcao = (velhaMenuOpcao + 3) % 4;
    somTique();
  }

  if (clicou(bDown, true)) {
    velhaMenuOpcao = (velhaMenuOpcao + 1) % 4;
    somTique();
  }

  if (clicou(bBack, false)) {
    somVoltar();
    esperaSoltar();
    telaAtual = TELA_JOGOS_MENU;
    return;
  }

  if (clicou(bOk, false)) {
    somConfirma();

    if (velhaMenuOpcao == 0) {
      velhaModo = 0;
    } else if (velhaMenuOpcao == 1) {
      velhaModo = 1;
    } else if (velhaMenuOpcao == 2) {
      velhaDificuldade = (velhaDificuldade + 1) % 3;
    } else {
      for (int i = 0; i < 9; i++) {
        velhaTabuleiro[i] = ' ';
      }

      velhaCursor = 4;
      velhaJogador = 'X';
      velhaFim = false;
      velhaVencedor = ' ';
      telaAtual = TELA_VELHA_JOGO;
    }

    esperaSoltar();
  }
}


// ---------------- RECORDES (salvos na memoria) ----------------
void carregarRecordes() {
  char chave[8];
  for (int i = 0; i < N_JOGOS; i++) {
    snprintf(chave, sizeof(chave), "rec%d", i);
    recordes[i] = preferencias.getInt(chave, 0);
  }
}

// Salva se for recorde. Devolve true quando bateu o recorde.
bool registrarPontuacao(int jogo, int pontos) {
  if (pontos > recordes[jogo]) {
    recordes[jogo] = pontos;
    char chave[8];
    snprintf(chave, sizeof(chave), "rec%d", jogo);
    preferencias.putInt(chave, pontos);
    return true;
  }
  return false;
}

// ---------------- COMEMORACAO ----------------
void somRecorde() {
  tocaTom(1047, 90, false);
  tocaTom(1319, 90, false);
  tocaTom(1568, 90, false);
  tocaTom(2093, 220, false);
}

// confete caindo (t = ms desde o inicio da comemoracao)
void desenhaConfete(unsigned long t) {
  for (int i = 0; i < 22; i++) {
    int x = (i * 37 + 11) % 128;
    int vel = 20 + (i * 7) % 25;                           // px por segundo
    int y = (int)(((t * vel) / 1000 + i * 13) % 74) - 8;
    if (y < 0 || y > 63) continue;
    if (i % 3 == 0)      display.drawPixel(x, y, WHITE);
    else if (i % 3 == 1) display.drawFastHLine(x, y, 2, WHITE);
    else { display.drawPixel(x, y, WHITE); display.drawPixel(x + 1, y + 1, WHITE); }
  }
}

// Tela de fim de jogo (serve para todos os jogos)
void desenhaFimJogo(const char* rotulo, int pontos, bool novoRec, int recorde, unsigned long t) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  if (novoRec) {
    desenhaConfete(t);
    float fase = (t % 700) / 700.0f;
    int pulo = (int)(4 * fase * (1 - fase) * 8);          // Purin pulando de alegria
    desenhaPurin(24, 58, pulo, 0);
    if ((t / 250) % 2 == 0) {
      display.setCursor(52, 6);  display.print("NOVO");
      display.setCursor(52, 16); display.print("RECORDE!");
    }
  } else {
    desenhaPurin(24, 58, 0, 0);
    display.setCursor(52, 6);  display.print("FIM DE");
    display.setCursor(52, 16); display.print("JOGO");
  }

  display.setCursor(52, 28);
  display.print(rotulo); display.print(": "); display.print(pontos);
  if (!novoRec) {
    display.setCursor(52, 37);
    display.print("rec: "); display.print(recorde);
  }
  display.setCursor(52, 46); display.print("OK: de novo");
  display.setCursor(52, 55); display.print("BACK: sair");
  display.display();
}

// ============================================================
//  DINO PURIN
//  OK ou CIMA = pular | BAIXO (segurar) = abaixar / cair rapido
// ============================================================
#define DINO_MAX_OBS 3
const int DINO_CHAO = 56;     // linha do chao
const int DINO_X    = 14;     // posicao horizontal do Purin

// tipos: 0 pequeno | 1 medio | 2 alto | 3 duplo | 4 passaro (voa baixo: abaixe!)
const uint8_t OBS_W[5] = { 6, 8, 6, 14, 12 };
const uint8_t OBS_H[5] = { 8, 12, 16, 8, 7 };
const uint8_t OBS_Y[5] = { 0, 0, 0, 0, 14 };   // altura do "pe" do obstaculo acima do chao

struct ObsDino { bool ativo; float x; uint8_t tipo; };
ObsDino obsDino[DINO_MAX_OBS];

EstadoJogo estadoDino = JG_INTRO;
float dinoAltura = 0, dinoVel = 0;
bool  dinoAbaixado = false;
float dinoPontos = 0, dinoVelMundo = 80, dinoDistChao = 0, dinoProxima = 120;
int   dinoMarco = 0;
unsigned long dinoTMarco = 0, dinoUltimoQuadro = 0, dinoTFim = 0;
bool  dinoNovoRecorde = false;
int   dinoFinal = 0;

void desenhaPurinCorrida(int x, int pesY, int frame, bool abaixado, bool noAr) {
  if (abaixado) {
    display.fillRoundRect(x - 2, pesY - 12, 18, 10, 4, BLACK);
    display.drawRoundRect(x - 2, pesY - 12, 18, 10, 4, WHITE);
    display.fillRoundRect(x + 1, pesY - 15, 12, 4, 2, WHITE);      // boina
    display.drawPixel(x + 11, pesY - 8, WHITE);                    // olho
    display.drawFastHLine(x + 7, pesY - 6, 2, WHITE);              // bochecha
    display.fillRect(x + 1, pesY - 2, 4, 2, WHITE);                // patinhas
    display.fillRect(x + 9, pesY - 2, 4, 2, WHITE);
    return;
  }
  // corpo + cabeca
  display.fillRoundRect(x + 1, pesY - 9, 12, 7, 2, BLACK);
  display.drawRoundRect(x + 1, pesY - 9, 12, 7, 2, WHITE);
  display.fillRoundRect(x - 1, pesY - 19, 16, 11, 4, BLACK);
  display.drawRoundRect(x - 1, pesY - 19, 16, 11, 4, WHITE);
  display.fillRoundRect(x + 1, pesY - 22, 12, 4, 2, WHITE);        // boina
  display.drawFastVLine(x - 2, pesY - 16, 4, WHITE);               // orelha
  display.drawPixel(x + 10, pesY - 14, WHITE);                     // olho
  display.drawPixel(x + 13, pesY - 12, WHITE);                     // focinho
  display.drawFastHLine(x + 7, pesY - 11, 2, WHITE);               // bochecha
  // patinhas alternando (recolhidas no ar)
  int a = noAr ? 1 : ((frame % 2 == 0) ? 1 : 0);
  int b = noAr ? 1 : ((frame % 2 == 0) ? 0 : 1);
  display.fillRect(x + 2, pesY - 2 - a, 4, 2, WHITE);
  display.fillRect(x + 9, pesY - 2 - b, 4, 2, WHITE);
}

void desenhaObsDino(int i) {
  int t = obsDino[i].tipo;
  int x = (int)obsDino[i].x;
  int h = OBS_H[t];
  int y = (DINO_CHAO - OBS_Y[t]) - h;
  switch (t) {
    case 0: case 1: case 2:
      display.fillRoundRect(x, y, OBS_W[t], h, 2, WHITE);
      display.drawFastHLine(x + 1, y + h / 2, OBS_W[t] - 2, BLACK);
      break;
    case 3:
      display.fillRoundRect(x, y, 6, h, 2, WHITE);
      display.fillRoundRect(x + 8, y, 6, h, 2, WHITE);
      break;
    case 4: {
      bool asaCima = ((millis() / 120) % 2 == 0);
      display.fillRoundRect(x + 2, y + 2, 9, 4, 2, WHITE);          // corpo
      display.drawFastHLine(x, y + 3, 2, WHITE);                    // bico
      if (asaCima) display.drawLine(x + 5, y + 2, x + 8, y, WHITE);
      else         display.drawLine(x + 5, y + 5, x + 8, y + 7, WHITE);
      break;
    }
  }
}

void dinoGeraObstaculo() {
  for (int i = 0; i < DINO_MAX_OBS; i++) {
    if (obsDino[i].ativo) continue;
    obsDino[i].ativo = true;
    obsDino[i].x = SCREEN_W;
    if (dinoPontos >= 120 && random(100) < 25) obsDino[i].tipo = 4;   // passaros depois dos 120 pts
    else                                       obsDino[i].tipo = random(0, 4);
    break;
  }
  dinoProxima = dinoVelMundo * 0.65f + random(30, 81) + 12;
}

void dinoComecar() {
  dinoAltura = 0; dinoVel = 0; dinoAbaixado = false;
  dinoPontos = 0; dinoVelMundo = 80; dinoDistChao = 0;
  dinoMarco = 0; dinoProxima = 120;
  for (int i = 0; i < DINO_MAX_OBS; i++) obsDino[i].ativo = false;
  dinoUltimoQuadro = millis();
  estadoDino = JG_JOGANDO;
}

void dinoTerminar() {
  dinoFinal = (int)dinoPontos;
  dinoNovoRecorde = registrarPontuacao(0, dinoFinal);
  estadoDino = JG_FIM;
  if (dinoNovoRecorde) somRecorde(); else somErro();
  dinoTFim = millis();
}

void iniciaDino() {
  estadoDino = JG_INTRO;
  dinoUltimoQuadro = millis();
}

void dinoAtualiza(float dt) {
  // ---- entrada ----
  bool pediuPulo = false;
  if (clicou(bOk, false)) pediuPulo = true;
  if (clicou(bUp, false)) pediuPulo = true;
  bool baixo = !digitalRead(PIN_BTN_DOWN);

  if (pediuPulo && dinoAltura <= 0.01f && dinoVel <= 0.01f) {
    dinoVel = 170;
    tocaTom(1400, 10, false);
  }

  // ---- fisica do pulo ----
  if (dinoAltura > 0 || dinoVel > 0) {
    float g = baixo ? 1100.0f : 560.0f;            // segurar BAIXO = cai mais rapido
    dinoVel -= g * dt;
    dinoAltura += dinoVel * dt;
    if (dinoAltura <= 0) { dinoAltura = 0; dinoVel = 0; }
  }
  dinoAbaixado = baixo && dinoAltura <= 0.01f;

  // ---- cenario ----
  float vel = 80.0f + dinoPontos * 0.12f;
  if (vel > 175.0f) vel = 175.0f;
  dinoVelMundo = vel;
  float avanco = vel * dt;
  dinoPontos += avanco / 10.0f;
  dinoDistChao += avanco;

  int marco = (int)dinoPontos / 100;
  if (marco > dinoMarco) {
    dinoMarco = marco;
    dinoTMarco = millis();
    tocaTom(1800, 30, false);
  }

  for (int i = 0; i < DINO_MAX_OBS; i++) {
    if (!obsDino[i].ativo) continue;
    obsDino[i].x -= avanco;
    if (obsDino[i].x + OBS_W[obsDino[i].tipo] < 0) obsDino[i].ativo = false;
  }
  dinoProxima -= avanco;
  if (dinoProxima <= 0) dinoGeraObstaculo();

  // ---- colisao ----
  int pesY = DINO_CHAO - (int)dinoAltura;
  int px0 = DINO_X, px1 = DINO_X + 14, py0 = pesY - 19, py1 = pesY;
  if (dinoAbaixado) { px0 = DINO_X - 1; px1 = DINO_X + 15; py0 = pesY - 12; }
  for (int i = 0; i < DINO_MAX_OBS; i++) {
    if (!obsDino[i].ativo) continue;
    int t = obsDino[i].tipo;
    int ox0 = (int)obsDino[i].x, ox1 = ox0 + OBS_W[t];
    int oy1 = DINO_CHAO - OBS_Y[t], oy0 = oy1 - OBS_H[t];
    if (px0 < ox1 - 1 && px1 > ox0 + 1 && py0 < oy1 - 1 && py1 > oy0 + 1) {
      dinoTerminar();
      return;
    }
  }
}

void dinoDesenha() {
  unsigned long agora = millis();
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  // placar (pisca um pouquinho a cada 100 pontos)
  display.setCursor(0, 0);
  display.print("REC "); display.print(recordes[0]);
  if (agora - dinoTMarco > 600 || (agora / 100) % 2 == 0) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%05d", (int)dinoPontos);
    display.setCursor(98, 0);
    display.print(buf);
  }

  // nuvens (andam devagar)
  int n1 = SCREEN_W - ((int)(dinoDistChao * 0.2f) % 170);
  int n2 = SCREEN_W - (((int)(dinoDistChao * 0.2f) + 85) % 170);
  display.drawRoundRect(n1, 16, 16, 5, 2, WHITE);
  display.drawRoundRect(n2, 26, 12, 4, 2, WHITE);

  // chao rolando
  display.drawFastHLine(0, DINO_CHAO + 1, SCREEN_W, WHITE);
  for (int i = 0; i < 12; i++) {
    int gx = (((i * 41 - (int)dinoDistChao) % 128) + 128) % 128;
    display.drawFastHLine(gx, DINO_CHAO + 3 + (i % 3) * 2, 1 + (i % 3), WHITE);
  }

  for (int i = 0; i < DINO_MAX_OBS; i++) if (obsDino[i].ativo) desenhaObsDino(i);

  int pesY = DINO_CHAO - (int)dinoAltura;
  desenhaPurinCorrida(DINO_X, pesY, (int)(dinoDistChao / 12), dinoAbaixado, dinoAltura > 0);
  display.display();
}

void dinoDesenhaIntro() {
  unsigned long t = millis();
  display.clearDisplay();
  display.setTextSize(1);

  display.fillRect(0, 0, SCREEN_W, 10, WHITE);
  display.setTextColor(BLACK);
  display.setCursor(34, 1);
  display.print("DINO PURIN");
  display.setTextColor(WHITE);

  display.setCursor(36, 15); display.print("OK/CIMA: pula");
  display.setCursor(36, 25); display.print("BAIXO: abaixa");
  display.setCursor(36, 37); display.print("recorde: "); display.print(recordes[0]);

  display.drawFastHLine(0, 51, SCREEN_W, WHITE);
  desenhaPurinCorrida(10, 50, (int)(t / 150), false, false);

  if ((t / 600) % 2 == 0) {
    display.setCursor(19, 55);
    display.print("[ OK p/ jogar ]");
  }
  display.display();
}

void telaDino() {
  ultimaAtividade = millis();                      // jogando = acordado
  unsigned long agora = millis();
  float dt = (agora - dinoUltimoQuadro) / 1000.0f;
  if (dt > 0.04f) dt = 0.04f;
  dinoUltimoQuadro = agora;

  if (estadoDino == JG_INTRO) {
    bool ok = clicou(bOk, false);
    bool voltar = clicou(bBack, false);
    if (voltar) { somVoltar(); esperaSoltar(); telaAtual = TELA_JOGOS_MENU; return; }
    if (ok)     { somConfirma(); dinoComecar(); return; }
    dinoDesenhaIntro();
  }
  else if (estadoDino == JG_JOGANDO) {
    if (clicou(bBack, false)) { dinoTerminar(); return; }
    dinoAtualiza(dt);
    if (estadoDino == JG_JOGANDO) dinoDesenha();
  }
  else {  // JG_FIM
    bool ok = clicou(bOk, false);
    bool voltar = clicou(bBack, false);
    if (agora - dinoTFim > 700) {                  // evita apertar sem querer
      if (voltar) { somVoltar(); esperaSoltar(); telaAtual = TELA_JOGOS_MENU; return; }
      if (ok)     { somConfirma(); dinoComecar(); return; }
    }
    desenhaFimJogo("pts", dinoFinal, dinoNovoRecorde, recordes[0], agora - dinoTFim);
  }
}

// ============================================================
//  PONG (voce na esquerda x computador)
//  CIMA/BAIXO (segurar) = mover | ganhe 5 pontos = 1 vitoria
//  Pontuacao = vitorias SEGUIDAS (o computador fica mais rapido)
// ============================================================
const int PONG_TOPO  = 10;     // primeira linha da quadra
const int PONG_RAQ_H = 14;
const int PONG_RAQ_W = 3;
const int PONG_X_JOG = 4;
const int PONG_X_PC  = 121;

EstadoJogo estadoPong = JG_INTRO;
float pongJogY = 29, pongPcY = 29;
float pongBolaX = 63, pongBolaY = 35, pongBolaVX = 0, pongBolaVY = 0;
int   pongPlacarJog = 0, pongPlacarPc = 0, pongSequencia = 0;
float pongErroPc = 0;
bool  pongUltimoPontoJog = true;
const char* pongMsg = "";
unsigned long pongTEvento = 0, pongUltimoQuadro = 0;
bool  pongNovoRecorde = false;
int   pongFinal = 0;

void pongNovaPartida() {
  pongPlacarJog = 0; pongPlacarPc = 0;
  pongJogY = 29; pongPcY = 29;
  pongBolaX = 63; pongBolaY = 35; pongBolaVX = 0; pongBolaVY = 0;
  pongMsg = "PRONTO?";
  pongTEvento = millis();
  estadoPong = JG_PONTO;
}

void pongSacar() {
  float v = 75.0f + pongSequencia * 4.0f;
  if (v > 100.0f) v = 100.0f;
  pongBolaX = 63; pongBolaY = 35;
  pongBolaVX = pongUltimoPontoJog ? v : -v;               // saca para quem perdeu o ponto
  pongBolaVY = (random(2) ? 1 : -1) * (float)random(15, 46);
  pongErroPc = (float)random(-9, 10);
  estadoPong = JG_JOGANDO;
}

void pongComecar() {
  pongSequencia = 0;
  pongUltimoPontoJog = true;
  pongNovaPartida();
}

void pongTerminar() {
  pongFinal = pongSequencia;
  pongNovoRecorde = registrarPontuacao(1, pongFinal);
  estadoPong = JG_FIM;
  if (pongNovoRecorde) somRecorde(); else somErro();
  pongTEvento = millis();
}

void iniciaPong() {
  estadoPong = JG_INTRO;
  pongUltimoQuadro = millis();
}

void pongMoveJogador(float dt) {
  if (!digitalRead(PIN_BTN_UP))   pongJogY -= 75.0f * dt;
  if (!digitalRead(PIN_BTN_DOWN)) pongJogY += 75.0f * dt;
  pongJogY = constrain(pongJogY, (float)PONG_TOPO, (float)(64 - PONG_RAQ_H));
}

void pongMovePc(float dt) {
  float velMax = 42.0f + pongSequencia * 4.0f;            // fica mais rapido a cada vitoria
  if (velMax > 85.0f) velMax = 85.0f;
  float centro = pongPcY + PONG_RAQ_H / 2.0f;
  float alvo = 36.0f;                                     // bola longe: volta ao centro
  if (pongBolaVX > 0 && pongBolaX > 40) alvo = pongBolaY + 1.5f + pongErroPc;
  float dif = alvo - centro;
  float passo = velMax * dt;
  if (dif > 2.0f)       pongPcY += (passo < dif ? passo : dif);
  else if (dif < -2.0f) pongPcY -= (passo < -dif ? passo : -dif);
  pongPcY = constrain(pongPcY, (float)PONG_TOPO, (float)(64 - PONG_RAQ_H));
}

void pongRebate(float raqY, int direcao) {
  float rel = ((pongBolaY + 1.5f) - (raqY + PONG_RAQ_H / 2.0f)) / (PONG_RAQ_H / 2.0f + 1.5f);
  rel = constrain(rel, -1.0f, 1.0f);
  float vel = fabsf(pongBolaVX) * 1.05f;                  // acelera a cada rebatida
  if (vel > 140.0f) vel = 140.0f;
  pongBolaVX = direcao * vel;
  pongBolaVY = rel * 80.0f;                               // o angulo depende de onde bateu
  tocaTom(1100, 10, false);
}

void pongPonto(bool jogadorMarcou) {
  if (jogadorMarcou) pongPlacarJog++; else pongPlacarPc++;
  pongUltimoPontoJog = jogadorMarcou;
  pongTEvento = millis();

  if (jogadorMarcou) tocaTom(1568, 50, false); else tocaTom(300, 110, false);

  if (pongPlacarJog >= 5) {
    pongSequencia++;
    pongMsg = "VITORIA!";
    estadoPong = JG_VITORIA;
    tocaTom(1319, 80, false); tocaTom(1568, 80, false); tocaTom(2093, 160, false);
  } else if (pongPlacarPc >= 5) {
    pongTerminar();
  } else {
    pongMsg = jogadorMarcou ? "PONTO!" : "PONTO DO PC";
    estadoPong = JG_PONTO;
  }
}

void pongMoveBola(float dt) {
  pongBolaX += pongBolaVX * dt;
  pongBolaY += pongBolaVY * dt;

  if (pongBolaY < PONG_TOPO)  { pongBolaY = PONG_TOPO;  pongBolaVY = -pongBolaVY; tocaTom(700, 6, false); }
  if (pongBolaY > 64 - 3)     { pongBolaY = 64 - 3;     pongBolaVY = -pongBolaVY; tocaTom(700, 6, false); }

  // raquete do jogador
  if (pongBolaVX < 0 && pongBolaX <= PONG_X_JOG + PONG_RAQ_W && pongBolaX + 3 >= PONG_X_JOG &&
      pongBolaY + 3 >= pongJogY && pongBolaY <= pongJogY + PONG_RAQ_H) {
    pongBolaX = PONG_X_JOG + PONG_RAQ_W;
    pongRebate(pongJogY, +1);
    pongErroPc = (float)random(-9, 10);                   // o PC "erra a mira" um pouquinho
  }
  // raquete do computador
  if (pongBolaVX > 0 && pongBolaX + 3 >= PONG_X_PC && pongBolaX <= PONG_X_PC + PONG_RAQ_W &&
      pongBolaY + 3 >= pongPcY && pongBolaY <= pongPcY + PONG_RAQ_H) {
    pongBolaX = PONG_X_PC - 3;
    pongRebate(pongPcY, -1);
  }

  if (pongBolaX < -4)            pongPonto(false);
  else if (pongBolaX > SCREEN_W) pongPonto(true);
}

void pongDesenha() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.setCursor(0, 0);   display.print("VOCE "); display.print(pongPlacarJog);
  display.setCursor(104, 0); display.print("PC ");   display.print(pongPlacarPc);
  char buf[10];
  snprintf(buf, sizeof(buf), "SEQ %d", pongSequencia);
  display.setCursor((SCREEN_W - (int)strlen(buf) * 6) / 2, 0);
  display.print(buf);

  display.drawFastHLine(0, PONG_TOPO - 1, SCREEN_W, WHITE);
  for (int y = PONG_TOPO + 2; y < 64; y += 6) display.drawFastVLine(64, y, 3, WHITE);

  display.fillRect(PONG_X_JOG, (int)pongJogY, PONG_RAQ_W, PONG_RAQ_H, WHITE);
  display.fillRect(PONG_X_PC,  (int)pongPcY,  PONG_RAQ_W, PONG_RAQ_H, WHITE);
  display.fillRect((int)pongBolaX, (int)pongBolaY, 3, 3, WHITE);

  if (estadoPong == JG_PONTO || estadoPong == JG_VITORIA) {
    int w = (int)strlen(pongMsg) * 6 + 10;
    int x = (SCREEN_W - w) / 2;
    display.fillRect(x, 26, w, 13, BLACK);
    display.drawRect(x, 26, w, 13, WHITE);
    textoCentralizado(pongMsg, 29);
  }
  display.display();
}

void pongDesenhaIntro() {
  unsigned long t = millis();
  display.clearDisplay();
  display.setTextSize(1);

  display.fillRect(0, 0, SCREEN_W, 10, WHITE);
  display.setTextColor(BLACK);
  display.setCursor(52, 1);
  display.print("PONG");
  display.setTextColor(WHITE);

  display.setCursor(13, 14); display.print("CIMA/BAIXO: mover");
  display.setCursor(22, 24); display.print("ganhe 5 pontos");
  display.setCursor(7, 34);  display.print("venca em sequencia!");
  display.setCursor(34, 45); display.print("recorde: "); display.print(recordes[1]);

  if ((t / 600) % 2 == 0) {
    display.setCursor(19, 55);
    display.print("[ OK p/ jogar ]");
  }
  display.display();
}

void telaPong() {
  ultimaAtividade = millis();
  unsigned long agora = millis();
  float dt = (agora - pongUltimoQuadro) / 1000.0f;
  if (dt > 0.04f) dt = 0.04f;
  pongUltimoQuadro = agora;

  if (estadoPong == JG_INTRO) {
    bool ok = clicou(bOk, false);
    bool voltar = clicou(bBack, false);
    if (voltar) { somVoltar(); esperaSoltar(); telaAtual = TELA_JOGOS_MENU; return; }
    if (ok)     { somConfirma(); pongComecar(); return; }
    pongDesenhaIntro();
  }
  else if (estadoPong == JG_FIM) {
    bool ok = clicou(bOk, false);
    bool voltar = clicou(bBack, false);
    if (agora - pongTEvento > 700) {
      if (voltar) { somVoltar(); esperaSoltar(); telaAtual = TELA_JOGOS_MENU; return; }
      if (ok)     { somConfirma(); pongComecar(); return; }
    }
    desenhaFimJogo("seq", pongFinal, pongNovoRecorde, recordes[1], agora - pongTEvento);
  }
  else {  // JOGANDO, PONTO ou VITORIA
    if (clicou(bBack, false)) { pongTerminar(); return; }
    pongMoveJogador(dt);

    if (estadoPong == JG_JOGANDO) {
      pongMovePc(dt);
      pongMoveBola(dt);
    } else if (estadoPong == JG_PONTO) {
      if (agora - pongTEvento > 900) pongSacar();
    } else if (estadoPong == JG_VITORIA) {
      if (agora - pongTEvento > 1500) pongNovaPartida();
    }
    if (estadoPong != JG_FIM) pongDesenha();
  }
}

// ============================================================
//  MENU DOS MINI JOGOS
// ============================================================

void abrirJogo(int i) {
  switch (i) {
    case 0:
      iniciaDino();
      telaAtual = TELA_DINO;
      break;

    case 1:
      iniciaPong();
      telaAtual = TELA_PONG;
      break;

    case 2:
      vhAbrir();
      break;

    case 3:
      memAbrir();
      break;

    case 4:
      sudAbrir();
      break;
  }
}



void telaJogosMenu() {
  if (clicou(bUp, true)) {
    jogoSel = (jogoSel + N_JOGOS - 1) % N_JOGOS;
    somTique();
  }

  if (clicou(bDown, true)) {
    jogoSel = (jogoSel + 1) % N_JOGOS;
    somTique();
  }

  if (clicou(bOk, false)) {
    somConfirma();
    esperaSoltar();
    abrirJogo(jogoSel);
    return;
  }

  if (clicou(bBack, false)) {
    somVoltar();
    esperaSoltar();
    telaAtual = TELA_MENU;
    return;
  }

  unsigned long t = millis();
  display.clearDisplay();

  // Moldura externa do fliperama
  display.drawRoundRect(1, 1, 126, 62, 5, WHITE);
  display.drawRoundRect(4, 4, 120, 56, 3, WHITE);

  // Titulo
  display.fillRoundRect(12, 5, 104, 10, 2, WHITE);
  display.setTextColor(BLACK);
  display.setTextSize(1);
  display.setCursor(30, 7);
  display.print("GAME CENTER");

  // Estrelas animadas
  for (int i = 0; i < 6; i++) {
    int x = 8 + i * 22;
    int y = 19 + ((i % 2) * 3);

    if ((t / 250 + i) % 3 == 0) {
      display.drawPixel(x, y, WHITE);
      display.drawPixel(x - 1, y, WHITE);
      display.drawPixel(x + 1, y, WHITE);
      display.drawPixel(x, y - 1, WHITE);
      display.drawPixel(x, y + 1, WHITE);
    } else {
      display.drawPixel(x, y, WHITE);
    }
  }

  // Cartao do jogo selecionado
  display.drawRoundRect(13, 22, 102, 29, 4, WHITE);

  // Indicadores de navegacao
  display.setTextColor(WHITE);
  display.setCursor(5, 32);
  display.print("<");
  display.setCursor(117, 32);
  display.print(">");

  // Icones em pixel art: Dino Purin e Pong
  
  // Icones em pixel art centralizados no cartao
  if (jogoSel == 0) {
    // Carinha do Purin centralizada
    display.fillRoundRect(48, 28, 23, 17, 5, WHITE);
    display.fillRoundRect(53, 25, 13, 5, 2, WHITE);
    display.fillCircle(54, 35, 1, BLACK);
    display.fillCircle(65, 35, 1, BLACK);
    display.drawPixel(59, 39, BLACK);
    display.drawPixel(60, 39, BLACK);

    // Obstaculos do Dino
    display.drawRect(76, 38, 4, 7, WHITE);
    display.drawRect(83, 35, 4, 10, WHITE);
    display.drawPixel(79, 36, WHITE);

  } else if (jogoSel == 1) {
    // Pong centralizado
    display.fillRoundRect(44, 29, 3, 13, 1, WHITE);
    display.fillRoundRect(81, 29, 3, 13, 1, WHITE);

    // Bola animada
    if ((t / 250) % 2 == 0) {
      display.fillCircle(62, 35, 2, WHITE);
    } else {
      display.fillCircle(67, 35, 2, WHITE);
    }

    // Linha central
    for (int y = 27; y < 46; y += 5) {
      display.drawPixel(64, y, WHITE);
    }

  } else if (jogoSel == 2) {
    vhIconeCard();
  } else if (jogoSel == 4) {
    sudIconeCard();
  } else if (jogoSel == 3) { 
    memIconeCard(); 
  } else {
    // Icone generico centralizado
    display.drawRoundRect(51, 27, 26, 19, 3, WHITE);
    display.drawRoundRect(55, 30, 18, 13, 2, WHITE);
    display.drawPixel(61, 34, WHITE);
    display.drawPixel(67, 34, WHITE);
    display.drawPixel(61, 38, WHITE);
    display.drawPixel(67, 38, WHITE);
  }


  // Nome do jogo
  display.setTextColor(WHITE);
  display.setTextSize(1);

  const char* nome = NOMES_JOGOS[jogoSel];
  int larguraNome = strlen(nome) * 6;
  int xNome = (SCREEN_W - larguraNome) / 2;

  if (larguraNome > 116) xNome = 6;

  display.fillRect(8, 52, 112, 7, BLACK);
  display.setCursor(xNome, 52);
  display.print(nome);

  // Recorde
  if (jogoSel >= 0 && jogoSel < N_JOGOS && recordes[jogoSel] > 0) {
    display.setCursor(88, 52);
    display.print("R:");
    display.print(recordes[jogoSel]);
  }

  display.display();
}


// ============================================================
//  JOGO DA VELHA - menu novo + jogo completo
//  Contra o Purin (voce = X, Purin = O) ou 2 jogadores
//  Recorde = maior sequencia de pontos contra o Purin
// ============================================================
const uint8_t VH_LINHAS[8][3] = { {0,1,2},{3,4,5},{6,7,8},{0,3,6},{1,4,7},{2,5,8},{0,4,8},{2,4,6} };
const char* VH_NIVEIS[3] = { "FACIL", "MEDIO", "DIFICIL" };

const int VH_JOGANDO = 0, VH_PENSANDO = 1, VH_RESULTADO = 2, VH_FIM = 3;
const int VH_X0 = 5, VH_Y0 = 12, VH_C = 17;           // posicao e tamanho das casas

char vhTab[9] = { ' ',' ',' ',' ',' ',' ',' ',' ',' ' };
int  vhCursor = 4;
int  vhModo = 0;              // 0 = contra o Purin | 1 = dois jogadores
int  vhNivel = 1;             // 0 facil | 1 medio | 2 dificil
int  vhMenuSel = 0;           // 0 modo | 1 nivel | 2 jogar
char vhVez = 'X';
char vhInicia = 'X';          // quem comeca a rodada (alterna)
int  vhEstado = VH_JOGANDO;
char vhResultado = ' ';       // 'X', 'O' ou 'E' (empate)
int  vhLinhaVit = -1;
int  vhPlacarX = 0, vhPlacarO = 0, vhPlacarE = 0;
int  vhPontos = 0, vhGanho = 0;
bool vhNovoRec = false;
unsigned long vhT = 0, vhEspera = 800;

// ---------------- REGRAS ----------------
int vhVencedorTab() {                      // indice da linha vencedora, ou -1
  for (int l = 0; l < 8; l++) {
    char a = vhTab[VH_LINHAS[l][0]];
    if (a != ' ' && a == vhTab[VH_LINHAS[l][1]] && a == vhTab[VH_LINHAS[l][2]]) return l;
  }
  return -1;
}

int vhLivres() {
  int n = 0;
  for (int i = 0; i < 9; i++) if (vhTab[i] == ' ') n++;
  return n;
}

// valor do ponto de vista do Purin (O): positivo = O ganha, negativo = X ganha
int vhMinimax(bool vezO, int prof) {
  int l = vhVencedorTab();
  if (l >= 0) return (vhTab[VH_LINHAS[l][0]] == 'O') ? (10 - prof) : (prof - 10);
  if (vhLivres() == 0) return 0;
  int melhor = vezO ? -100 : 100;
  for (int i = 0; i < 9; i++) {
    if (vhTab[i] != ' ') continue;
    vhTab[i] = vezO ? 'O' : 'X';
    int v = vhMinimax(!vezO, prof + 1);
    vhTab[i] = ' ';
    if (vezO) { if (v > melhor) melhor = v; }
    else      { if (v < melhor) melhor = v; }
  }
  return melhor;
}

// casa que faz 'm' completar uma linha agora (ou -1)
int vhJogadaQueFecha(char m) {
  for (int i = 0; i < 9; i++) {
    if (vhTab[i] != ' ') continue;
    vhTab[i] = m;
    bool fecha = (vhVencedorTab() >= 0);
    vhTab[i] = ' ';
    if (fecha) return i;
  }
  return -1;
}

int vhEscolheJogadaPurin() {
  int livres[9], n = 0;
  for (int i = 0; i < 9; i++) if (vhTab[i] == ' ') livres[n++] = i;
  if (n == 0) return -1;

  if (vhNivel == 2) {                                     // DIFICIL: perfeito
    if (n == 9) { const int ini[5] = { 0, 2, 4, 6, 8 }; return ini[random(5)]; }
    int melhorV = -1000, cand[9], nc = 0;
    for (int k = 0; k < n; k++) {
      vhTab[livres[k]] = 'O';
      int v = vhMinimax(false, 1);
      vhTab[livres[k]] = ' ';
      if (v > melhorV) { melhorV = v; nc = 0; }
      if (v == melhorV) cand[nc++] = livres[k];
    }
    return cand[random(nc)];
  }

  int errar = (vhNivel == 0) ? 65 : 20;                   // % de jogadas "distraidas"
  if (random(100) < errar) return livres[random(n)];

  int j = vhJogadaQueFecha('O');                          // ganha se puder
  if (j >= 0) return j;
  j = vhJogadaQueFecha('X');                              // bloqueia voce
  if (j >= 0) return j;

  if (vhNivel == 1) {                                     // MEDIO: gosta de centro e cantos
    if (vhTab[4] == ' ') return 4;
    int cantos[4], nc = 0;
    const int c[4] = { 0, 2, 6, 8 };
    for (int k = 0; k < 4; k++) if (vhTab[c[k]] == ' ') cantos[nc++] = c[k];
    if (nc > 0) return cantos[random(nc)];
  }
  return livres[random(n)];
}

// ---------------- FLUXO DO JOGO ----------------
void vhMoveCursor(int dir) {               // anda pelas casas livres
  for (int k = 1; k <= 9; k++) {
    int c = (vhCursor + dir * k + 90) % 9;
    if (vhTab[c] == ' ') { vhCursor = c; return; }
  }
}

void vhPensarOuJogar() {
  if (vhModo == 0 && vhVez == 'O') {
    vhEstado = VH_PENSANDO;
    vhT = millis();
    vhEspera = random(550, 1100);
  } else {
    vhEstado = VH_JOGANDO;
  }
}

void vhNovaRodada() {
  for (int i = 0; i < 9; i++) vhTab[i] = ' ';
  vhResultado = ' ';
  vhLinhaVit = -1;
  vhGanho = 0;
  vhCursor = 4;
  vhVez = vhInicia;
  vhPensarOuJogar();
}

void vhComecar() {                         // chamado pelo menu
  vhPlacarX = 0; vhPlacarO = 0; vhPlacarE = 0;
  vhPontos = 0; vhNovoRec = false;
  vhInicia = 'X';
  vhNovaRodada();
  telaAtual = TELA_VELHA_JOGO;
}

void vhAbrir() {                           // chamado pelo menu dos jogos
  vhMenuSel = 0;
  telaAtual = TELA_VELHA_MENU;
}

void vhEncerrarSequencia() {
  vhNovoRec = registrarPontuacao(2, vhPontos);
  vhEstado = VH_FIM;
  if (vhNovoRec) somRecorde();
  vhT = millis();
}

void vhFimRodada(char res) {
  vhResultado = res;
  if (res == 'X') vhPlacarX++; else if (res == 'O') vhPlacarO++; else vhPlacarE++;

  vhGanho = 0;
  if (vhModo == 0) {                       // pontos so contam contra o Purin
    if (res == 'X')                    vhGanho = (vhNivel == 0) ? 1 : (vhNivel == 1 ? 3 : 8);
    else if (res == 'E' && vhNivel == 2) vhGanho = 3;     // empatar com o perfeito vale!
    vhPontos += vhGanho;
  }

  if (res == 'E') { tocaTom(659, 90, false); tocaTom(523, 140, false); }
  else if (vhModo == 0 && res == 'O') somErro();
  else { tocaTom(1319, 80, false); tocaTom(1568, 80, false); tocaTom(2093, 160, false); }

  vhEstado = VH_RESULTADO;
  vhT = millis();
}

void vhJoga(int casa) {
  vhTab[casa] = vhVez;
  tocaTom(vhVez == 'X' ? 1500 : 1100, 25, false);

  int l = vhVencedorTab();
  if (l >= 0)          { vhLinhaVit = l; vhFimRodada(vhVez); return; }
  if (vhLivres() == 0) { vhFimRodada('E'); return; }

  vhVez = (vhVez == 'X') ? 'O' : 'X';
  if (vhTab[vhCursor] != ' ') vhMoveCursor(1);
  vhPensarOuJogar();
}

// ---------------- DESENHO ----------------
void vhDesenhaMarca(char m, int ox, int oy) {            // ox,oy = canto da casa
  if (m == 'X') {
    display.drawLine(ox + 3,  oy + 3, ox + 13, oy + 13, WHITE);
    display.drawLine(ox + 4,  oy + 3, ox + 14, oy + 13, WHITE);
    display.drawLine(ox + 13, oy + 3, ox + 3,  oy + 13, WHITE);
    display.drawLine(ox + 14, oy + 3, ox + 4,  oy + 13, WHITE);
  } else if (m == 'O') {
    display.drawCircle(ox + 8, oy + 8, 6, WHITE);
    display.drawCircle(ox + 8, oy + 8, 5, WHITE);
  }
}

void vhDesenhaTabuleiro(unsigned long t, bool mostrarCursor) {
  // grade
  for (int k = 1; k <= 2; k++) {
    display.fillRect(VH_X0 + k * VH_C - 1, VH_Y0 + 2, 2, 3 * VH_C - 4, WHITE);
    display.fillRect(VH_X0 + 2, VH_Y0 + k * VH_C - 1, 3 * VH_C - 4, 2, WHITE);
  }
  for (int i = 0; i < 9; i++) {
    int ox = VH_X0 + (i % 3) * VH_C;
    int oy = VH_Y0 + (i / 3) * VH_C;

    bool naLinha = false;
    if (vhLinhaVit >= 0)
      for (int k = 0; k < 3; k++) if (VH_LINHAS[vhLinhaVit][k] == i) naLinha = true;
    bool esconde = naLinha && ((t / 220) % 2 == 1);      // marcas vencedoras piscam

    if (vhTab[i] != ' ' && !esconde) vhDesenhaMarca(vhTab[i], ox, oy);

    if (mostrarCursor && i == vhCursor) {
      display.drawPixel(ox + 8, oy + 8, WHITE);
      if ((t / 250) % 2 == 0) display.drawRect(ox + 1, oy + 1, 15, 15, WHITE);
    }
  }
  if (vhLinhaVit >= 0) {                                  // risco por cima da linha
    int a = VH_LINHAS[vhLinhaVit][0], c = VH_LINHAS[vhLinhaVit][2];
    int x1 = VH_X0 + (a % 3) * VH_C + 8, y1 = VH_Y0 + (a / 3) * VH_C + 8;
    int x2 = VH_X0 + (c % 3) * VH_C + 8, y2 = VH_Y0 + (c / 3) * VH_C + 8;
    display.drawLine(x1, y1, x2, y2, WHITE);
    display.drawLine(x1 + 1, y1, x2 + 1, y2, WHITE);
    display.drawLine(x1, y1 + 1, x2, y2 + 1, WHITE);
  }
}

void vhDesenhaJogo(unsigned long t) {
  char buf[16];
  display.clearDisplay();
  display.setTextSize(1);

  // barra do topo
  display.fillRect(0, 0, SCREEN_W, 10, WHITE);
  display.setTextColor(BLACK);
  display.setCursor(3, 1);
  display.print("VELHA");
  if (vhModo == 0) {
    display.setCursor(40, 1);
    display.print(VH_NIVEIS[vhNivel]);
    bool pisca = (vhEstado == VH_RESULTADO && vhGanho > 0 && (t / 250) % 2 == 0);
    if (!pisca) {
      snprintf(buf, sizeof(buf), "PTS %d", vhPontos);
      display.setCursor(125 - (int)strlen(buf) * 6, 1);
      display.print(buf);
    }
  } else {
    display.setCursor(125 - 6 * 6, 1);
    display.print("2 JOG.");
  }
  display.setTextColor(WHITE);

  vhDesenhaTabuleiro(t, vhEstado == VH_JOGANDO);

  // placar (um pontinho marca de quem e a vez)
  const int px = 61;
  bool vezX = (vhEstado == VH_JOGANDO || vhEstado == VH_PENSANDO) && vhVez == 'X';
  bool vezO = (vhEstado == VH_JOGANDO || vhEstado == VH_PENSANDO) && vhVez == 'O';
  if (vezX) display.fillRect(57, 16, 3, 3, WHITE);
  if (vezO) display.fillRect(57, 26, 3, 3, WHITE);

  display.setCursor(px, 14);
  if (vhModo == 0) snprintf(buf, sizeof(buf), "VOCE: %d", vhPlacarX);
  else             snprintf(buf, sizeof(buf), "X: %d", vhPlacarX);
  display.print(buf);
  display.setCursor(px, 24);
  if (vhModo == 0) snprintf(buf, sizeof(buf), "PURIN: %d", vhPlacarO);
  else             snprintf(buf, sizeof(buf), "O: %d", vhPlacarO);
  display.print(buf);
  display.setCursor(px, 34);
  snprintf(buf, sizeof(buf), "EMPATE: %d", vhPlacarE);
  display.print(buf);

  // mensagens
  const char* l1 = "";
  const char* l2 = "";
  if (vhEstado == VH_JOGANDO) {
    if (vhModo == 0)       l1 = "SUA VEZ (X)";
    else if (vhVez == 'X') l1 = "VEZ DO X";
    else                   l1 = "VEZ DO O";
    l2 = "OK: marcar";
  } else if (vhEstado == VH_PENSANDO) {
    int d = (t / 300) % 4;
    l1 = (d == 0) ? "PENSANDO" : (d == 1) ? "PENSANDO." : (d == 2) ? "PENSANDO.." : "PENSANDO...";
  } else if (vhEstado == VH_RESULTADO) {
    if (vhResultado == 'E')       l1 = "EMPATE!";
    else if (vhModo == 0)         l1 = (vhResultado == 'X') ? "VOCE VENCEU" : "PURIN GANHA";
    else                          l1 = (vhResultado == 'X') ? "X VENCEU" : "O VENCEU";
    if (!(vhModo == 0 && vhResultado == 'O')) l2 = "OK: proxima";
  }
  display.setCursor(px, 47); display.print(l1);
  display.setCursor(px, 56); display.print(l2);

  display.display();
}

// mini tabuleiro animado (enfeite do menu e do cartao dos jogos)
void vhDesenhaMini(int x0, int y0, unsigned long t) {
  const int c = 9;                                        // casas de 8px + 1px de grade
  display.drawFastVLine(x0 + 8,  y0, 26, WHITE);
  display.drawFastVLine(x0 + 17, y0, 26, WHITE);
  display.drawFastHLine(x0, y0 + 8,  26, WHITE);
  display.drawFastHLine(x0, y0 + 17, 26, WHITE);

  const int jogadas[5] = { 4, 0, 2, 3, 6 };               // X4 O0 X2 O3 X6 -> X vence na diagonal
  unsigned long ph = t % 3600;
  int n = (int)(ph / 420);
  if (n > 5) n = 5;
  for (int i = 0; i < n; i++) {
    int ox = x0 + (jogadas[i] % 3) * c, oy = y0 + (jogadas[i] / 3) * c;
    if (i % 2 == 0) {
      display.drawLine(ox + 1, oy + 1, ox + 6, oy + 6, WHITE);
      display.drawLine(ox + 6, oy + 1, ox + 1, oy + 6, WHITE);
    } else {
      display.drawCircle(ox + 4, oy + 4, 3, WHITE);
    }
  }
  if (ph >= 2100 && (t / 250) % 2 == 0) {
    display.drawLine(x0 + 22, y0 + 4, x0 + 4, y0 + 22, WHITE);
    display.drawLine(x0 + 23, y0 + 4, x0 + 5, y0 + 22, WHITE);
  }
}

// desenho do cartao no "Game Center" (opcional)
void vhIconeCard() {
  vhDesenhaMini(51, 24, millis());
}

// ---------------- MENU ----------------
void vhLinhaMenu(int y, const char* texto, bool sel, bool ativa) {
  if (sel) {
    display.fillRoundRect(34, y, 92, 14, 3, WHITE);
    display.setTextColor(BLACK);
  } else {
    display.drawRoundRect(34, y, 92, 14, 3, WHITE);
    display.setTextColor(WHITE);
  }
  display.setCursor(38, y + 3);
  display.print(texto);
  if (!ativa) {                                           // linha desativada: risca de pontilhado
    for (int x = 38; x < 122; x += 3) display.drawPixel(x, y + 7, sel ? BLACK : WHITE);
  }
  display.setTextColor(WHITE);
}

void vhMoveMenu(int dir) {
  do { vhMenuSel = (vhMenuSel + dir + 3) % 3; } while (vhMenuSel == 1 && vhModo == 1);
  somTique();
}

void vhTelaMenu() {
  if (clicou(bUp, true))   vhMoveMenu(-1);
  if (clicou(bDown, true)) vhMoveMenu(+1);
  if (clicou(bOk, false)) {
    if (vhMenuSel == 0)      { vhModo = 1 - vhModo; somConfirma(); }
    else if (vhMenuSel == 1) { vhNivel = (vhNivel + 1) % 3; somConfirma(); }
    else { somConfirma(); esperaSoltar(); vhComecar(); return; }
  }
  if (clicou(bBack, false)) { somVoltar(); esperaSoltar(); telaAtual = TELA_JOGOS_MENU; return; }

  unsigned long t = millis();
  char buf[24];
  display.clearDisplay();
  display.setTextSize(1);

  // titulo com X e O nos cantos
  display.fillRect(0, 0, SCREEN_W, 10, WHITE);
  display.drawLine(3, 2, 8, 7, BLACK);
  display.drawLine(8, 2, 3, 7, BLACK);
  display.drawCircle(122, 5, 3, BLACK);
  display.setTextColor(BLACK);
  display.setCursor(25, 1);
  display.print("JOGO DA VELHA");
  display.setTextColor(WHITE);

  // tabuleiro animado + recorde
  vhDesenhaMini(4, 16, t);
  if (recordes[2] > 0) {
    display.setCursor(4, 45);
    display.print("REC");
    display.setCursor(4, 54);
    display.print(recordes[2]);
  }

  // opcoes
  snprintf(buf, sizeof(buf), "Modo: %s", vhModo == 0 ? "Purin" : "2 jogador");
  vhLinhaMenu(14, buf, vhMenuSel == 0, true);

  if (vhModo == 0) snprintf(buf, sizeof(buf), "Nivel: %s", vhNivel == 0 ? "Facil" : (vhNivel == 1 ? "Medio" : "Dificil"));
  else             snprintf(buf, sizeof(buf), "Nivel: --");
  vhLinhaMenu(30, buf, vhMenuSel == 1, vhModo == 0);

  bool jogarSel = (vhMenuSel == 2);
  if (jogarSel) {
    display.fillRoundRect(34, 46, 92, 14, 3, WHITE);
    display.setTextColor(BLACK);
  } else {
    display.drawRoundRect(34, 46, 92, 14, 3, WHITE);
  }
  display.setCursor(56, 49);
  display.print(jogarSel ? "> JOGAR! <" : "  JOGAR!  ");
  display.setTextColor(WHITE);

  display.display();
}

void vhSair() {
  if (vhModo == 0 && vhPontos > 0) { vhEncerrarSequencia(); return; }   // salva e mostra o resultado
  somVoltar();
  esperaSoltar();
  telaAtual = TELA_VELHA_MENU;
}

void vhTelaJogo() {
  ultimaAtividade = millis();
  unsigned long t = millis();
  bool cUp = clicou(bUp, true);
  bool cDown = clicou(bDown, true);
  bool cOk = clicou(bOk, false);
  bool cBack = clicou(bBack, false);

  if (vhEstado == VH_FIM) {
    if (t - vhT > 700) {                                   // evita apertar sem querer
      if (cBack) { somVoltar(); esperaSoltar(); telaAtual = TELA_VELHA_MENU; return; }
      if (cOk)   { somConfirma(); vhComecar(); return; }
    }
    desenhaFimJogo("pts", vhPontos, vhNovoRec, recordes[2], t - vhT);
    return;
  }

  if (cBack) { vhSair(); return; }

  if (vhEstado == VH_JOGANDO) {
    if (cUp)   { vhMoveCursor(-1); somTique(); }
    if (cDown) { vhMoveCursor(+1); somTique(); }
    if (cOk && vhTab[vhCursor] == ' ') { vhJoga(vhCursor); return; }
  }
  else if (vhEstado == VH_PENSANDO) {
    if (t - vhT > vhEspera) { vhJoga(vhEscolheJogadaPurin()); return; }
  }
  else if (vhEstado == VH_RESULTADO) {
    if (vhModo == 0 && vhResultado == 'O') {               // perdeu: a sequencia acaba
      if (t - vhT > 1700) { vhEncerrarSequencia(); return; }
    } else if (cOk && t - vhT > 400) {
      vhInicia = (vhInicia == 'X') ? 'O' : 'X';            // quem comeca alterna
      vhNovaRodada();
      return;
    }
  }
  vhDesenhaJogo(t);
}

// ============================================================
//  SUDOKU FOFO  (4x4, 6x6 ou 9x9 | facil, medio ou dificil)
//  CIMA/BAIXO = escolher casa | OK = editar a casa
//  editando: CIMA/BAIXO = numero | OK = salvar | BACK = cancelar
//  Numeros que voce escreveu ficam "branquinhos"; se um deles
//  ficar repetido por um tempinho, ele comeca a piscar.
//  Recorde = melhor tempo de cada tamanho e nivel.
// ============================================================
#define SUD_E_TAM   0     // escolher tamanho
#define SUD_E_DIF   1     // escolher nivel
#define SUD_E_JOGO  2     // jogando
#define SUD_E_SAIR  3     // "quer sair?"
#define SUD_E_VIT   4     // comemorando
#define SUD_E_FIM   5     // tela final

#define SUD_ATRASO_ERRO 4000     // ms ate o numero repetido comecar a piscar
#define SUD_MAX_NOS     30000    // limite de tentativas do conferidor de solucao unica

const char* SUD_NOME_DIF[3] = { "FAC", "MED", "DIF" };
const char* SUD_ROT_DIF[3]  = { "Facil", "Medio", "Dificil" };
// quantas casas esconder [tamanho][nivel] (o gerador para antes se a solucao deixar de ser unica)
const int SUD_REMOVER[3][3] = { { 6, 8, 10 }, { 14, 18, 22 }, { 36, 46, 52 } };

const uint8_t BMP_CORACAO[6] = { 0b0110110, 0b1111111, 0b1111111, 0b0111110, 0b0011100, 0b0001000 };

// fonte minuscula 3x5 (digitos 1 a 9)
const uint8_t FONTE3X5[10][5] = {
  {0,0,0,0,0},
  {2,6,2,2,7}, {7,1,7,4,7}, {7,1,7,1,7}, {5,5,7,1,1}, {7,4,7,1,7},
  {7,4,7,5,7}, {7,1,1,2,2}, {7,5,7,5,7}, {7,5,7,1,7}
};

int sudEstado = SUD_E_TAM;
int sudTamSel = 0, sudDifSel = 0;               // escolhas nos menus
int sudTam = 0, sudDif = 0;                     // partida atual
int sudN = 9, sudBR = 3, sudBC = 3, sudPasso = 7, sudX0 = 2, sudY0 = 0, sudRemover = 40;
int8_t sudSol[81], sudGrade[81];
bool sudFixo[81], sudErro[81];
unsigned long sudErroT[81];                     // desde quando a casa esta repetida
int sudCursor = 0, sudValorEdit = 0, sudVagas = 0, sudSegs = 0;
bool sudEditando = false, sudNovoRec = false;
unsigned long sudTInicio = 0, sudTFim = 0, sudTPausaIni = 0;
int sudMelhor[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };   // segundos [tamanho*3 + nivel], 0 = sem recorde
unsigned long sudNos = 0;

// ---------------- decoracao ----------------
void brilhinhoCor(int x, int y, int r, uint16_t cor) {
  display.drawFastHLine(x - r, y, 2 * r + 1, cor);
  display.drawFastVLine(x, y - r, 2 * r + 1, cor);
}

void brilhinho(int x, int y, int r) {
  brilhinhoCor(x, y, r, WHITE);
}

// desenha um bitmap de 7 colunas centralizado em (cx,cy), ampliado "esc" vezes
void desenhaBmp7(int cx, int cy, const uint8_t* linhas, int n, int esc) {
  int x0 = cx - (7 * esc) / 2;
  int y0 = cy - (n * esc) / 2;
  for (int r = 0; r < n; r++)
    for (int c = 0; c < 7; c++)
      if (linhas[r] & (1 << (6 - c)))
        display.fillRect(x0 + c * esc, y0 + r * esc, esc, esc, WHITE);
}

// estrelinhas piscando espalhadas numa regiao (sem = muda o "desenho" do ceu)
void ceuEstrelado(int x0, int y0, int x1, int y1, int n, int sem) {
  unsigned long t = millis();
  int w = x1 - x0 + 1, h = y1 - y0 + 1;
  if (w <= 0 || h <= 0) return;
  for (int i = 0; i < n; i++) {
    uint32_t a = (uint32_t)(i + 1 + sem * 53) * 2654435761u;
    uint32_t b = (a ^ (a >> 15)) * 2246822519u;
    b ^= (b >> 13);
    int x = x0 + (int)((a >> 16) % (uint32_t)w);
    int y = y0 + (int)((b >> 16) % (uint32_t)h);
    int per = 1100 + (int)((a >> 4) % 1400);
    float r = (float)((t + ((b >> 5) % 2000)) % per) / per;
    if (r < 0.45f) {
      if (i % 3 == 0) brilhinho(x, y, 1);
      else            display.drawPixel(x, y, WHITE);
    } else if (r < 0.75f) {
      display.drawPixel(x, y, WHITE);
    }
  }
}

void sudFormataTempo(char* buf, int tam, int segs) {
  if (segs > 5999) segs = 5999;
  if (segs < 0) segs = 0;
  snprintf(buf, tam, "%02d:%02d", segs / 60, segs % 60);
}

// ---------------- configuracao e recordes ----------------
void sudConfigura(int tam, int dif) {
  sudTam = tam;
  sudDif = dif;
  if (tam == 0)      { sudN = 4; sudBR = 2; sudBC = 2; sudPasso = 14; sudX0 = 4; sudY0 = 3; }
  else if (tam == 1) { sudN = 6; sudBR = 2; sudBC = 3; sudPasso = 10; sudX0 = 3; sudY0 = 1; }
  else               { sudN = 9; sudBR = 3; sudBC = 3; sudPasso = 7;  sudX0 = 2; sudY0 = 0; }
  sudRemover = SUD_REMOVER[tam][dif];
}

// menor tempo (de qualquer nivel) de um tamanho; 0 = nenhum
int sudMelhorTam(int tam) {
  int m = 0;
  for (int d = 0; d < 3; d++) {
    int v = sudMelhor[tam * 3 + d];
    if (v > 0 && (m == 0 || v < m)) m = v;
  }
  return m;
}

void sudAbrir() {
  char k[8];
  for (int i = 0; i < 9; i++) {
    snprintf(k, sizeof(k), "sud%d", i);
    sudMelhor[i] = preferencias.getInt(k, 0);
  }
  sudEstado = SUD_E_TAM;
  telaAtual = TELA_SUDOKU;
}

// ---------------- gerador (com solucao unica garantida) ----------------
void sudEmbaralha(int* a, int n) {
  for (int i = n - 1; i > 0; i--) {
    int j = random(i + 1);
    int tmp = a[i]; a[i] = a[j]; a[j] = tmp;
  }
}

int sudCaixa(int r, int c) {
  return (r / sudBR) * (sudN / sudBC) + (c / sudBC);
}

// conta solucoes (ate "limite") usando sempre a casa com menos candidatos
int sudResolve(int8_t* g, uint16_t* lm, uint16_t* cm, uint16_t* bm, int limite) {
  if (++sudNos > SUD_MAX_NOS) return limite;           // desiste: trata como "varias"
  int N = sudN;
  uint16_t todos = (uint16_t)((1u << (N + 1)) - 2);    // bits 1..N
  int melhor = -1, menorQtd = 100;
  uint16_t melhorMask = 0;
  for (int i = 0; i < N * N; i++) {
    if (g[i] != 0) continue;
    int r = i / N, c = i % N;
    uint16_t livres = todos & ~(lm[r] | cm[c] | bm[sudCaixa(r, c)]);
    int q = __builtin_popcount(livres);
    if (q == 0) return 0;
    if (q < menorQtd) {
      menorQtd = q; melhor = i; melhorMask = livres;
      if (q == 1) break;
    }
  }
  if (melhor < 0) return 1;                            // tudo preenchido: achou uma solucao
  int r = melhor / N, c = melhor % N, b = sudCaixa(r, c);
  int total = 0;
  for (int d = 1; d <= N; d++) {
    if (!(melhorMask & (1u << d))) continue;
    g[melhor] = d;
    lm[r] |= (1u << d); cm[c] |= (1u << d); bm[b] |= (1u << d);
    total += sudResolve(g, lm, cm, bm, limite - total);
    g[melhor] = 0;
    lm[r] &= ~(1u << d); cm[c] &= ~(1u << d); bm[b] &= ~(1u << d);
    if (total >= limite) break;
  }
  return total;
}

// o tabuleiro atual (sudGrade) tem exatamente 1 solucao?
bool sudUnica() {
  int N = sudN;
  int8_t g[81];
  uint16_t lm[9], cm[9], bm[9];
  for (int i = 0; i < 9; i++) { lm[i] = 0; cm[i] = 0; bm[i] = 0; }
  for (int i = 0; i < N * N; i++) {
    g[i] = sudGrade[i];
    if (g[i] != 0) {
      int r = i / N, c = i % N;
      lm[r] |= (1u << g[i]); cm[c] |= (1u << g[i]); bm[sudCaixa(r, c)] |= (1u << g[i]);
    }
  }
  sudNos = 0;
  return sudResolve(g, lm, cm, bm, 2) == 1;
}

void sudCalculaErros() {
  int N = sudN;
  for (int i = 0; i < N * N; i++) sudErro[i] = false;
  for (int i = 0; i < N * N; i++) {
    int v = sudGrade[i];
    if (v == 0 || sudFixo[i]) continue;                // so marca o que VOCE escreveu
    int r = i / N, c = i % N;
    for (int j = 0; j < N * N; j++) {
      if (j == i || sudGrade[j] != v) continue;
      int rj = j / N, cj = j % N;
      if (rj == r || cj == c || (rj / sudBR == r / sudBR && cj / sudBC == c / sudBC)) {
        sudErro[i] = true;
        break;
      }
    }
  }
}

// marca desde quando cada casa esta repetida (pra so piscar depois de um tempinho)
void sudAtualizaErros() {
  sudCalculaErros();
  unsigned long agora = millis();
  for (int i = 0; i < sudN * sudN; i++) {
    if (sudErro[i]) { if (sudErroT[i] == 0) sudErroT[i] = (agora | 1); }
    else            { sudErroT[i] = 0; }
  }
}

bool sudTemErro() {
  for (int i = 0; i < sudN * sudN; i++) if (sudErro[i]) return true;
  return false;
}

void sudContaVagas() {
  sudVagas = 0;
  for (int i = 0; i < sudN * sudN; i++) if (sudGrade[i] == 0) sudVagas++;
}

void sudDesenhaGerando() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  ceuEstrelado(0, 0, 127, 63, 14, 36);
  display.fillRoundRect(28, 14, 72, 36, 4, BLACK);
  display.drawRoundRect(28, 14, 72, 36, 4, WHITE);
  display.setCursor(31, 19);
  display.print("montando...");
  desenhaPurinMini(57, 32, 0, false);
  display.display();
}

// gera uma grade valida (padrao base + embaralhar) e esconde casas mantendo solucao unica
void sudGerar() {
  int N = sudN, br = sudBR, bc = sudBC;
  int nb = N / br, ns = N / bc;
  int bandas[3], pilhas[3], dentro[3], linhas[9], colunas[9], digs[9];

  for (int i = 0; i < nb; i++) bandas[i] = i;
  sudEmbaralha(bandas, nb);
  int k = 0;
  for (int b = 0; b < nb; b++) {
    for (int j = 0; j < br; j++) dentro[j] = j;
    sudEmbaralha(dentro, br);
    for (int j = 0; j < br; j++) linhas[k++] = bandas[b] * br + dentro[j];
  }
  for (int i = 0; i < ns; i++) pilhas[i] = i;
  sudEmbaralha(pilhas, ns);
  k = 0;
  for (int s = 0; s < ns; s++) {
    for (int j = 0; j < bc; j++) dentro[j] = j;
    sudEmbaralha(dentro, bc);
    for (int j = 0; j < bc; j++) colunas[k++] = pilhas[s] * bc + dentro[j];
  }
  for (int i = 0; i < N; i++) digs[i] = i + 1;
  sudEmbaralha(digs, N);

  for (int r = 0; r < N; r++) {
    for (int c = 0; c < N; c++) {
      int rr = linhas[r], cc = colunas[c];
      int p = (bc * (rr % br) + rr / br + cc) % N;
      sudSol[r * N + c] = digs[p];
    }
  }

  // esconde casas uma a uma, so mantendo se a solucao continuar unica
  int idx[81];
  for (int i = 0; i < N * N; i++) { idx[i] = i; sudGrade[i] = sudSol[i]; sudFixo[i] = true; sudErroT[i] = 0; }
  sudEmbaralha(idx, N * N);
  int removidas = 0;
  for (int q = 0; q < N * N && removidas < sudRemover; q++) {
    int i = idx[q];
    int8_t salvo = sudGrade[i];
    sudGrade[i] = 0;
    if (sudUnica()) { sudFixo[i] = false; removidas++; }
    else            { sudGrade[i] = salvo; }
    yield();
  }

  sudCursor = 0;
  for (int i = 0; i < N * N; i++) if (!sudFixo[i]) { sudCursor = i; break; }
  sudEditando = false;
  sudNovoRec = false;
  sudAtualizaErros();
  sudContaVagas();
  sudTInicio = millis();
  sudEstado = SUD_E_JOGO;
}

void sudNovoJogo() {
  sudDesenhaGerando();
  sudGerar();
}

void sudMoveCursor(int dir) {
  int NN = sudN * sudN;
  for (int passo = 1; passo <= NN; passo++) {
    int idx = (((sudCursor + dir * passo) % NN) + NN) % NN;
    if (!sudFixo[idx]) { sudCursor = idx; return; }
  }
}

// ---------------- desenho do tabuleiro ----------------
// desenha o numero d dentro da casa cujo canto interno e (cx,cy)
void sudDesenhaNum(int cx, int cy, int d, uint16_t cor) {
  int inner = sudPasso - 1;
  if (sudPasso >= 13) {                  // 4x4: numeros grandes (fonte 3x5 ampliada)
    int ox = cx + (inner - 6) / 2, oy = cy + (inner - 10) / 2;
    for (int r = 0; r < 5; r++)
      for (int c = 0; c < 3; c++)
        if (FONTE3X5[d][r] & (4 >> c)) display.fillRect(ox + c * 2, oy + r * 2, 2, 2, cor);
  } else if (sudPasso >= 9) {            // 6x6: fonte normal
    display.setTextColor(cor);
    display.setCursor(cx + (inner - 5) / 2, cy + (inner - 7) / 2);
    display.print(d);
    display.setTextColor(WHITE);
  } else {                               // 9x9: fonte minuscula
    int ox = cx + (inner - 3) / 2, oy = cy + (inner - 5) / 2;
    for (int r = 0; r < 5; r++)
      for (int c = 0; c < 3; c++)
        if (FONTE3X5[d][r] & (4 >> c)) display.drawPixel(ox + c, oy + r, cor);
  }
}

void sudDesenha(unsigned long t) {
  int N = sudN, p = sudPasso;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  // estrelinhas do painel
  ceuEstrelado(112, 1, 127, 9, 3, 21);
  ceuEstrelado(122, 33, 127, 62, 3, 22);

  // grade (linhas das caixas cheias, linhas das casas pontilhadas)
  for (int i = 0; i <= N; i++) {
    int x = sudX0 + i * p;
    int y = sudY0 + i * p;
    if (i % sudBC == 0) display.drawFastVLine(x, sudY0, N * p + 1, WHITE);
    else for (int yy = sudY0; yy <= sudY0 + N * p; yy += 2) display.drawPixel(x, yy, WHITE);
    if (i % sudBR == 0) display.drawFastHLine(sudX0, y, N * p + 1, WHITE);
    else for (int xx = sudX0; xx <= sudX0 + N * p; xx += 2) display.drawPixel(xx, y, WHITE);
  }

  // numeros
  for (int i = 0; i < N * N; i++) {
    int cx = sudX0 + (i % N) * p + 1;
    int cy = sudY0 + (i / N) * p + 1;
    int v = (sudEditando && i == sudCursor) ? sudValorEdit : sudGrade[i];
    if (v <= 0) continue;
    bool inv = !sudFixo[i];                          // o que voce escreveu fica "branquinho"
    bool pisca = sudErro[i] && sudErroT[i] != 0 && (t - sudErroT[i] > SUD_ATRASO_ERRO) && ((t / 300) % 2 == 0);
    if (pisca) inv = !inv;
    if (inv) {
      display.fillRect(cx, cy, p - 1, p - 1, WHITE);
      sudDesenhaNum(cx, cy, v, BLACK);
    } else {
      sudDesenhaNum(cx, cy, v, WHITE);
    }
  }

  // cursor piscando (mais rapido quando esta editando)
  if (sudEstado == SUD_E_JOGO) {
    int cx = sudX0 + (sudCursor % N) * p + 1;
    int cy = sudY0 + (sudCursor / N) * p + 1;
    unsigned long per = sudEditando ? 140 : 380;
    if ((t / per) % 2 == 0) display.fillRect(cx, cy, p - 1, p - 1, INVERSE);
  }

  // painel
  char buf[24];
  display.setCursor(72, 1);
  display.print("SUDOKU");
  snprintf(buf, sizeof(buf), "%dx%d %s", N, N, SUD_NOME_DIF[sudDif]);
  display.setCursor(72, 11);
  display.print(buf);

  unsigned long fim = t;
  if (sudEstado == SUD_E_VIT) fim = sudTFim;
  else if (sudEstado == SUD_E_SAIR) fim = sudTPausaIni;
  int segs = (int)((fim - sudTInicio) / 1000);
  char tb[8];
  sudFormataTempo(tb, sizeof(tb), segs);
  display.setCursor(72, 21);
  display.print(tb);

  snprintf(buf, sizeof(buf), "vagas %d", sudVagas);
  display.setCursor(72, 31);
  display.print(buf);

  display.setCursor(72, 44);
  display.print(sudEditando ? "UP/DN:+/-" : "UP/DN:ir");
  display.setCursor(72, 54);
  display.print(sudEditando ? "OK:salvar" : "OK:editar");

  bool piscando = (t % 3000) < 130;
  desenhaPurinMini(108, 18, 0, piscando);
}

// ---------------- telas de escolha ----------------
// mini tabuleiro enfeite (centro em cx,cy)
void sudMiniGrade(int cx, int cy, int tam, uint16_t cor) {
  int N  = (tam == 0) ? 4 : (tam == 1) ? 6 : 9;
  int br = (tam == 2) ? 3 : 2;
  int bc = (tam == 0) ? 2 : 3;
  int p  = (tam == 0) ? 5 : (tam == 1) ? 3 : 2;
  int lado = N * p;
  int x0 = cx - lado / 2, y0 = cy - lado / 2;
  for (int i = 0; i <= N; i++) {
    int x = x0 + i * p, y = y0 + i * p;
    if (i % bc == 0) display.drawFastVLine(x, y0, lado + 1, cor);
    else if (p >= 3) for (int yy = y0; yy <= y0 + lado; yy += 2) display.drawPixel(x, yy, cor);
    if (i % br == 0) display.drawFastHLine(x0, y, lado + 1, cor);
    else if (p >= 3) for (int xx = x0; xx <= x0 + lado; xx += 2) display.drawPixel(xx, y, cor);
  }
  for (int r = 0; r < N; r++) {                      // "numerinhos"
    for (int c = 0; c < N; c++) {
      if ((r * 3 + c * 5) % 4 != 0) continue;
      int px = x0 + c * p + p / 2, py = y0 + r * p + p / 2;
      if (p >= 5) display.fillRect(px - 1, py - 1, 2, 2, cor);
      else        display.drawPixel(px, py, cor);
    }
  }
}

void sudDesenhaEscolhaTam(unsigned long t) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  ceuEstrelado(0, 10, 127, 12, 5, 31);
  ceuEstrelado(0, 61, 127, 63, 6, 32);

  display.fillRect(0, 0, SCREEN_W, 10, WHITE);
  display.setTextColor(BLACK);
  display.setCursor(46, 1);
  display.print("SUDOKU");
  display.setTextColor(WHITE);

  const char* nomes[3] = { "4x4", "6x6", "9x9" };
  for (int i = 0; i < 3; i++) {
    int x = 3 + i * 42, y = 13, w = 38, h = 36;
    bool sel = (i == sudTamSel);
    uint16_t cor = sel ? BLACK : WHITE;
    if (sel) display.fillRoundRect(x, y, w, h, 4, WHITE);
    else     display.drawRoundRect(x, y, w, h, 4, WHITE);
    sudMiniGrade(x + w / 2, y + 14, i, cor);
    display.setTextColor(cor);
    display.setCursor(x + (w - 18) / 2, y + 27);
    display.print(nomes[i]);
    display.setTextColor(WHITE);
    if (sel) {                                        // brilhinhos nos cantinhos
      if ((t / 300) % 2 == 0) brilhinho(x - 2, y + 18, 1);
      else                    brilhinho(x + w + 1, y + 18, 1);
    }
  }

  char buf[24];
  int melhor = sudMelhorTam(sudTamSel);
  if (melhor > 0) {
    char tb[8];
    sudFormataTempo(tb, sizeof(tb), melhor);
    snprintf(buf, sizeof(buf), "melhor %s", tb);
  } else {
    snprintf(buf, sizeof(buf), "sem recorde ainda");
  }
  textoCentralizado(buf, 53);
  display.display();
}

void sudDesenhaEscolhaDif(unsigned long t) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  ceuEstrelado(0, 11, 4, 51, 3, 34);
  ceuEstrelado(123, 11, 127, 51, 3, 35);
  ceuEstrelado(0, 62, 100, 63, 4, 37);

  char buf[24];
  int N = (sudTamSel == 0) ? 4 : (sudTamSel == 1) ? 6 : 9;
  display.fillRect(0, 0, SCREEN_W, 10, WHITE);
  display.setTextColor(BLACK);
  snprintf(buf, sizeof(buf), "NIVEL DO %dx%d", N, N);
  display.setCursor((SCREEN_W - (int)strlen(buf) * 6) / 2, 1);
  display.print(buf);
  display.setTextColor(WHITE);

  for (int i = 0; i < 3; i++) {
    int y = 13 + i * 13;
    bool sel = (i == sudDifSel);
    uint16_t cor = sel ? BLACK : WHITE;
    if (sel) display.fillRoundRect(6, y, 116, 12, 3, WHITE);
    else     display.drawRoundRect(6, y, 116, 12, 3, WHITE);

    display.setTextColor(cor);
    display.setCursor(12, y + 2);
    display.print(SUD_ROT_DIF[i]);

    for (int k = 0; k < 3; k++) {                      // 1, 2 ou 3 brilhinhos
      int bx = 64 + k * 9, by = y + 6;
      if (k <= i) {
        int r = (sel && ((t / 300) + k) % 2 == 0) ? 3 : 2;
        brilhinhoCor(bx, by, r, cor);
      } else {
        display.drawPixel(bx, by, cor);
      }
    }

    int m = sudMelhor[sudTamSel * 3 + i];
    if (m > 0) sudFormataTempo(buf, sizeof(buf), m);
    else       snprintf(buf, sizeof(buf), "--:--");
    display.setCursor(90, y + 2);
    display.print(buf);
    display.setTextColor(WHITE);
  }

  display.setCursor(2, 56);
  display.print("BACK: voltar");
  desenhaPurinMini(112, 52, 0, (t % 3000) < 130);
  display.display();
}

// ---------------- tela final ----------------
void sudDesenhaFim(unsigned long t) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  ceuEstrelado(0, 0, 46, 63, 8, 4);

  if (sudNovoRec) {
    desenhaConfete(t);
    desenhaBmp7(8, 22 - (int)((t / 120) % 8), BMP_CORACAO, 6, 1);
    desenhaBmp7(41, 28 - (int)((t / 150) % 8), BMP_CORACAO, 6, 1);
    float fase = (t % 700) / 700.0f;
    int pulo = (int)(4 * fase * (1 - fase) * 8);           // Purin pulando de alegria
    desenhaPurin(24, 58, pulo, 0);
  } else {
    desenhaPurin(24, 58, 0, 0);
  }

  char buf[24], tb[8];
  display.setCursor(52, 6);
  display.print("RESOLVIDO!");
  int N = sudN;
  snprintf(buf, sizeof(buf), "%dx%d %s", N, N, SUD_NOME_DIF[sudDif]);
  display.setCursor(52, 16);
  display.print(buf);

  sudFormataTempo(tb, sizeof(tb), sudSegs);
  snprintf(buf, sizeof(buf), "tempo %s", tb);
  display.setCursor(52, 28);
  display.print(buf);

  if (sudNovoRec) {
    if ((t / 250) % 2 == 0) {
      display.setCursor(52, 37);
      display.print("* RECORDE *");
    }
  } else {
    sudFormataTempo(tb, sizeof(tb), sudMelhor[sudTam * 3 + sudDif]);
    snprintf(buf, sizeof(buf), "melhor %s", tb);
    display.setCursor(52, 37);
    display.print(buf);
  }
  display.setCursor(52, 46); display.print("OK: de novo");
  display.setCursor(52, 55); display.print("BACK: sair");
  display.display();
}

// ---------------- icone no Game Center ----------------
void sudMiniDigito(int x, int y, int d) {
  for (int r = 0; r < 5; r++)
    for (int c = 0; c < 3; c++)
      if (FONTE3X5[d][r] & (4 >> c)) display.drawPixel(x + c, y + r, WHITE);
}

void sudIconeCard() {
  unsigned long t = millis();
  const int x0 = 54, y0 = 26;                         // grade 3x3 de 21x21
  display.drawRoundRect(x0 - 2, y0 - 2, 25, 25, 3, WHITE);
  display.drawFastVLine(x0 + 7, y0, 21, WHITE);
  display.drawFastVLine(x0 + 14, y0, 21, WHITE);
  display.drawFastHLine(x0, y0 + 7, 21, WHITE);
  display.drawFastHLine(x0, y0 + 14, 21, WHITE);

  const int digitos[9] = { 3, 0, 1, 0, 0, 0, 2, 0, 3 };
  for (int i = 0; i < 9; i++) {
    int cx = x0 + (i % 3) * 7 + 2;
    int cy = y0 + (i / 3) * 7 + 1;
    int d = digitos[i];
    if (i == 4) d = 1 + (int)((t / 900) % 3);          // a casa do meio troca de numero
    if (d > 0) sudMiniDigito(cx, cy, d);
  }
  if ((t / 350) % 2 == 0) display.fillRect(x0 + 8, y0 + 8, 6, 6, INVERSE);   // cursorzinho

  if ((t / 400) % 2 == 0) { brilhinho(45, 29, 1); brilhinho(83, 45, 2); }
  else                    { brilhinho(46, 45, 2); brilhinho(82, 29, 1); }
}

// ---------------- tela principal do Sudoku ----------------
void sudTela() {
  ultimaAtividade = millis();
  unsigned long t = millis();
  bool bu = clicou(bUp, true);
  bool bd = clicou(bDown, true);
  bool bo = clicou(bOk, false);
  bool bb = clicou(bBack, false);

  // ----- escolher tamanho -----
  if (sudEstado == SUD_E_TAM) {
    if (bu) { sudTamSel = (sudTamSel + 2) % 3; somTique(); }
    if (bd) { sudTamSel = (sudTamSel + 1) % 3; somTique(); }
    if (bo) { somConfirma(); sudEstado = SUD_E_DIF; return; }
    if (bb) { somVoltar(); esperaSoltar(); telaAtual = TELA_JOGOS_MENU; return; }
    sudDesenhaEscolhaTam(t);
    return;
  }

  // ----- escolher nivel -----
  if (sudEstado == SUD_E_DIF) {
    if (bu) { sudDifSel = (sudDifSel + 2) % 3; somTique(); }
    if (bd) { sudDifSel = (sudDifSel + 1) % 3; somTique(); }
    if (bo) {
      somConfirma();
      sudConfigura(sudTamSel, sudDifSel);
      sudNovoJogo();
      return;
    }
    if (bb) { somVoltar(); sudEstado = SUD_E_TAM; return; }
    sudDesenhaEscolhaDif(t);
    return;
  }

  // ----- tela final -----
  if (sudEstado == SUD_E_FIM) {
    if (t - sudTFim > 700) {                           // evita apertar sem querer
      if (bo) { somConfirma(); sudNovoJogo(); return; }
      if (bb) { somVoltar(); sudEstado = SUD_E_TAM; return; }
    }
    sudDesenhaFim(t - sudTFim);
    return;
  }

  // ----- comemorando -----
  if (sudEstado == SUD_E_VIT) {
    if (t - sudTFim > 1400) {
      sudEstado = SUD_E_FIM;
      sudTFim = t;
      if (sudNovoRec) somRecorde();
      return;
    }
    sudDesenha(t);
    desenhaConfete(t - sudTFim);
    display.display();
    return;
  }

  // ----- "quer sair?" -----
  if (sudEstado == SUD_E_SAIR) {
    if (bo) { somVoltar(); sudEstado = SUD_E_TAM; return; }
    if (bb) {                                          // continua: desconta o tempo parado
      sudTInicio += (t - sudTPausaIni);
      sudEstado = SUD_E_JOGO;
      somConfirma();
      return;
    }
    sudDesenha(t);
    display.fillRoundRect(10, 18, 108, 28, 4, BLACK);
    display.drawRoundRect(10, 18, 108, 28, 4, WHITE);
    textoCentralizado("SAIR DO JOGO?", 23);
    textoCentralizado("OK:sim BACK:nao", 34);
    display.display();
    return;
  }

  // ----- jogando -----
  if (!sudEditando) {
    if (bb) { somVoltar(); sudTPausaIni = t; sudEstado = SUD_E_SAIR; return; }
    if (bu) { sudMoveCursor(-1); somTique(); }
    if (bd) { sudMoveCursor(+1); somTique(); }
    if (bo) { somConfirma(); sudEditando = true; sudValorEdit = sudGrade[sudCursor]; }
  } else {
    if (bb) { somVoltar(); sudEditando = false; }
    if (bu) { sudValorEdit = (sudValorEdit + 1) % (sudN + 1); somTique(); }
    if (bd) { sudValorEdit = (sudValorEdit + sudN) % (sudN + 1); somTique(); }
    if (bo) {
      sudGrade[sudCursor] = sudValorEdit;
      sudEditando = false;
      somConfirma();
      sudAtualizaErros();
      sudContaVagas();
      if (sudVagas == 0 && !sudTemErro()) {            // resolvido!
        sudTFim = t;
        sudSegs = (int)((t - sudTInicio) / 1000);
        if (sudSegs < 1) sudSegs = 1;
        int idx = sudTam * 3 + sudDif;
        sudNovoRec = (sudMelhor[idx] == 0 || sudSegs < sudMelhor[idx]);
        if (sudNovoRec) {
          sudMelhor[idx] = sudSegs;
          char k[8];
          snprintf(k, sizeof(k), "sud%d", idx);
          preferencias.putInt(k, sudSegs);
        }
        sudEstado = SUD_E_VIT;
      }
    }
  }
  sudDesenha(t);
  display.display();
}


// ============================================================
//  JOGO DA MEMORIA  (dois jogos em um: PARES e GENIUS)
//
//  PARES : CIMA/BAIXO = escolher carta | OK = virar | BACK = sair
//          3 niveis: 6, 8 e 12 pares (as cartas aparecem um instante
//          no comeco: decore!). Pontos = pares + bonus de jogadas
//          + bonus de tempo.
//
//  GENIUS: os 4 botoes viram as 4 teclas (CIMA, BAIXO, OK, VOLTA).
//          Repita a sequencia que o Purin mostrar. Toque curto em
//          VOLTA = tecla | SEGURAR VOLTA = sair.
//          3 niveis: 8, 12 e 16 rodadas (mais rapido a cada nivel).
//
//  Recorde: melhor pontuacao de cada jogo e nivel (6 recordes).
//
//  Cole este bloco no sketch, antes de "SETUP / LOOP".
//  Depende de: ceuEstrelado, brilhinho, brilhinhoCor, desenhaBmp7,
//  BMP_CORACAO (do Sudoku), desenhaConfete, somRecorde,
//  registrarPontuacao, textoCentralizado, desenhaPurin e
//  desenhaPurinMini (que voce ja tem).
// ============================================================
#define MEM_E_MODO    0     // escolher PARES ou GENIUS
#define MEM_E_NIVEL   1     // escolher facil / medio / dificil
#define MEM_E_PARES   2     // jogando PARES
#define MEM_E_GENIUS  3     // jogando GENIUS
#define MEM_E_SAIR    4     // "quer sair?"
#define MEM_E_VIT     5     // comemorando
#define MEM_E_FIM     6     // tela final

#define MEM_G_MOSTRA  0     // Purin mostrando a sequencia
#define MEM_G_JOGA    1     // sua vez
#define MEM_G_ACERTO  2     // rodada completa
#define MEM_G_ERRO    3     // errou

const uint8_t  MEM_N_PARES[3]   = { 6, 8, 12 };
const uint16_t MEM_ESPIA_MS[3]  = { 3000, 2500, 2000 };   // tempo "olhando" as cartas no comeco
const uint16_t MEM_ERRO_MS[3]   = { 1100, 900, 700 };     // tempo vendo o par errado
const uint8_t  MEM_G_RODADAS[3] = { 8, 12, 16 };
const uint16_t MEM_G_DUR[3]     = { 600, 450, 330 };      // tempo aceso de cada passo
const uint16_t MEM_G_MIN[3]     = { 380, 280, 200 };      // limite da aceleracao
const uint16_t MEM_NOTAS[4]     = { 523, 659, 784, 1047 };
const char* MEM_NOME_NIVEL[3]   = { "Facil", "Medio", "Dificil" };
const char* MEM_ABREV[3]        = { "FAC", "MED", "DIF" };
const char* MEM_INFO_PARES[3]   = { "6 pares, 4x3", "8 pares, 4x4", "12 pares, 6x4" };
const char* MEM_INFO_GENIUS[3]  = { "8 rodadas lento", "12 rodadas medio", "16 rodadas veloz" };

// 12 desenhinhos 7x7 (cada linha = 7 bits, o bit mais a esquerda e a coluna 0)
const uint8_t MEM_SPR[12][7] = {
  { 0b0110110, 0b1111111, 0b1111111, 0b1111111, 0b0111110, 0b0011100, 0b0001000 },  // coracao
  { 0b0001000, 0b0001000, 0b1111111, 0b0111110, 0b0011100, 0b0110110, 0b0100010 },  // estrela
  { 0b0011100, 0b0111000, 0b1110000, 0b1110000, 0b1110000, 0b0111000, 0b0011100 },  // lua
  { 0b0001000, 0b1011101, 0b0111110, 0b0111110, 0b0111110, 0b1011101, 0b0001000 },  // sol
  { 0b1000001, 0b1100011, 0b1111111, 0b1011101, 0b1111111, 0b0111110, 0b0011100 },  // gatinho
  { 0b0011100, 0b0111110, 0b1101011, 0b1111111, 0b0001000, 0b0011100, 0b0011100 },  // cogumelo
  { 0b0001110, 0b0001010, 0b0001010, 0b0001010, 0b0111010, 0b1111000, 0b0110000 },  // nota musical
  { 0b0000000, 0b0111110, 0b1111111, 0b0111110, 0b0011100, 0b0001000, 0b0000000 },  // diamante
  { 0b0011100, 0b0111110, 0b1101011, 0b1111111, 0b1011101, 0b0100010, 0b0011100 },  // carinha
  { 0b0011100, 0b0110110, 0b0110110, 0b0011100, 0b0001000, 0b0001100, 0b0001100 },  // chavinha
  { 0b0000000, 0b0011000, 0b0111100, 0b1111111, 0b1111111, 0b0111110, 0b0000000 },  // nuvem
  { 0b0001000, 0b0001000, 0b0011100, 0b0111110, 0b0111110, 0b0111110, 0b0011100 }   // gotinha
};

// ---------------- estado geral ----------------
int memEstado = MEM_E_MODO;
int memModoSel = 0, memNivelSel = 0;      // escolhas nos menus
int memModo = 0, memNivel = 0;            // partida atual (0 = PARES, 1 = GENIUS)
int memRec[6] = { 0, 0, 0, 0, 0, 0 };     // recorde [modo*3 + nivel]
int memPontos = 0;
bool memNovoRec = false, memVenceu = false;
unsigned long memTFim = 0, memTPausaIni = 0;

// ---------------- estado do PARES ----------------
int memCols = 4, memLinhas = 3, memCW = 26, memCH = 15, memGapX = 4, memGapY = 3, memGX = 6, memGY = 12;
int memNCartas = 12;
int8_t memBaralho[24];
bool memVirada[24], memAchada[24];
unsigned long memTMuda[24];
int memCursor = 0, memAbertas = 0, memPrim = -1, memSeg = -1;
int memJogadas = 0, memAchados = 0, memSegs = 0;
bool memEspiando = false, memResolvendo = false, memAcerto = false;
unsigned long memTInicio = 0, memTEspiaFim = 0, memTResolve = 0, memTBrilho = 0;
int memBrilhoA = -1, memBrilhoB = -1;

// ---------------- estado do GENIUS ----------------
uint8_t memGSeq[17];
int memGLen = 1, memGIdx = 0, memGFase = MEM_G_MOSTRA;
int memGPad = -1, memGEsperado = 0, memGFeitas = 0, memGGanho = 0;
unsigned long memGT = 0, memGPadAte = 0, memGBackT = 0;
bool memGBackPend = false;

// ============================================================
//  utilidades
// ============================================================
void memTempoTxt(char* buf, int tam, int segs) {
  if (segs > 5999) segs = 5999;
  if (segs < 0) segs = 0;
  snprintf(buf, tam, "%02d:%02d", segs / 60, segs % 60);
}

void memDesenhaSprite(int id, int cx, int cy, uint16_t cor) {
  int ox = cx - 3, oy = cy - 3;
  for (int r = 0; r < 7; r++)
    for (int c = 0; c < 7; c++)
      if (MEM_SPR[id][r] & (0x40 >> c)) display.drawPixel(ox + c, oy + r, cor);
}

// som sem travar (o buzzer fica ligado ate chamarem memTomDesliga)
void memTomLiga(int freq) {
  if (cfgVolume == 0) return;
  ledcChangeFrequency(PIN_BUZZER, freq, 8);
  ledcWrite(PIN_BUZZER, DUTY_VOLUME[cfgVolume]);
}

void memTomDesliga() {
  ledcWrite(PIN_BUZZER, 0);
}

int memMelhorModo(int modo) {
  int m = 0;
  for (int n = 0; n < 3; n++) if (memRec[modo * 3 + n] > m) m = memRec[modo * 3 + n];
  return m;
}

void memSalvaRecorde() {
  int idx = memModo * 3 + memNivel;
  memNovoRec = (memPontos > memRec[idx]);
  if (memNovoRec) {
    memRec[idx] = memPontos;
    char k[8];
    snprintf(k, sizeof(k), "mem%d", idx);
    preferencias.putInt(k, memPontos);
    registrarPontuacao(3, memPontos);        // aparece como "R:" no Game Center
  }
}

void memEntraFim(unsigned long t) {
  memTomDesliga();
  memGPad = -1;
  memEstado = MEM_E_FIM;
  memTFim = t;
  if (memNovoRec) somRecorde();
  else if (memVenceu) {
    tocaTom(1319, 80, false);
    tocaTom(1568, 80, false);
    tocaTom(2093, 160, false);
  }
}

void memAbrir() {                            // chamado pelo menu dos jogos
  char k[8];
  for (int i = 0; i < 6; i++) {
    snprintf(k, sizeof(k), "mem%d", i);
    memRec[i] = preferencias.getInt(k, 0);
  }
  memEstado = MEM_E_MODO;
  telaAtual = TELA_MEMORIA;
}

// ============================================================
//  PARES
// ============================================================
void memConfigPares() {
  if (memNivel == 0)      { memCols = 4; memLinhas = 3; memCW = 26; memCH = 15; memGapX = 4; memGapY = 3; memGX = 6; memGY = 12; }
  else if (memNivel == 1) { memCols = 4; memLinhas = 4; memCW = 26; memCH = 11; memGapX = 4; memGapY = 2; memGX = 6; memGY = 13; }
  else                    { memCols = 6; memLinhas = 4; memCW = 17; memCH = 11; memGapX = 3; memGapY = 2; memGX = 5; memGY = 13; }
  memNCartas = memCols * memLinhas;
}

void memEmbaralha(int8_t* a, int n) {
  for (int i = n - 1; i > 0; i--) {
    int j = random(i + 1);
    int8_t tmp = a[i]; a[i] = a[j]; a[j] = tmp;
  }
}

void memNovoPares() {
  memConfigPares();
  int P = MEM_N_PARES[memNivel];
  int8_t simb[12];
  for (int i = 0; i < 12; i++) simb[i] = i;
  memEmbaralha(simb, 12);                    // escolhe P desenhos diferentes
  for (int k = 0; k < P; k++) { memBaralho[2 * k] = simb[k]; memBaralho[2 * k + 1] = simb[k]; }
  memEmbaralha(memBaralho, memNCartas);

  unsigned long t = millis();
  for (int i = 0; i < memNCartas; i++) { memVirada[i] = true; memAchada[i] = false; memTMuda[i] = t; }
  memCursor = 0; memAbertas = 0; memPrim = -1; memSeg = -1;
  memJogadas = 0; memAchados = 0; memSegs = 0; memPontos = 0;
  memResolvendo = false; memNovoRec = false; memVenceu = false;
  memBrilhoA = -1; memBrilhoB = -1; memTBrilho = 0;
  memEspiando = true;                        // comeca mostrando todas as cartas
  memTEspiaFim = t + MEM_ESPIA_MS[memNivel] + 300;
  memTInicio = t;
  memEstado = MEM_E_PARES;
}

void memAjustaCursor() {                     // pula cartas viradas ou ja achadas
  if (!memAchada[memCursor] && !memVirada[memCursor]) return;
  for (int p = 1; p <= memNCartas; p++) {
    int idx = (memCursor + p) % memNCartas;
    if (!memAchada[idx] && !memVirada[idx]) { memCursor = idx; return; }
  }
}

void memMoveCursor(int dir) {
  for (int p = 1; p <= memNCartas; p++) {
    int idx = (((memCursor + dir * p) % memNCartas) + memNCartas) % memNCartas;
    if (!memAchada[idx] && !memVirada[idx]) { memCursor = idx; return; }
  }
}

void memVence(unsigned long t) {
  memSegs = (int)((t - memTInicio) / 1000);
  if (memSegs < 1) memSegs = 1;
  int P = MEM_N_PARES[memNivel];
  int bJog = P * 3 - memJogadas;             // quanto menos jogadas, melhor
  if (bJog < 0) bJog = 0;
  int bTemp = P * 6 - memSegs;               // quanto mais rapido, melhor
  if (bTemp < 0) bTemp = 0;
  memPontos = P * 100 + bJog * 20 + bTemp * 5;
  memVenceu = true;
  memSalvaRecorde();
  memTFim = t;
  memEstado = MEM_E_VIT;
}

void memViraCarta(int i, unsigned long t) {
  memVirada[i] = true;
  memTMuda[i] = t;
  tocaTom(1200 + (i % 4) * 90, 14, false);
  if (memAbertas == 0) {
    memPrim = i;
    memAbertas = 1;
    memAjustaCursor();
  } else {
    memSeg = i;
    memAbertas = 2;
    memJogadas++;
    memResolvendo = true;
    memAcerto = (memBaralho[memPrim] == memBaralho[memSeg]);
    memTResolve = t + (memAcerto ? 450 : MEM_ERRO_MS[memNivel]);
  }
}

void memResolve(unsigned long t) {
  memResolvendo = false;
  if (memAcerto) {
    memAchada[memPrim] = true;
    memAchada[memSeg] = true;
    memBrilhoA = memPrim; memBrilhoB = memSeg; memTBrilho = t;
    memAchados++;
    tocaTom(1319, 60, false);
    tocaTom(1760, 90, false);
  } else {
    memVirada[memPrim] = false; memTMuda[memPrim] = t;
    memVirada[memSeg]  = false; memTMuda[memSeg]  = t;
    tocaTom(330, 70, false);
  }
  memAbertas = 0; memPrim = -1; memSeg = -1;
  if (memAcerto && memAchados >= MEM_N_PARES[memNivel]) { memVence(t); return; }
  memAjustaCursor();
}

void memParesAtualiza(unsigned long t, bool bu, bool bd, bool bo) {
  if (memEspiando) {                         // fim do "olhe bem!": vira tudo de volta
    if (t >= memTEspiaFim) {
      for (int i = 0; i < memNCartas; i++) { memVirada[i] = false; memTMuda[i] = t; }
      memEspiando = false;
      memTInicio = t;
      memCursor = 0;
      tocaTom(1047, 40, false);
    }
    return;
  }
  if (memResolvendo) {
    bool pular = (!memAcerto && bo && t + MEM_ERRO_MS[memNivel] >= memTResolve + 500);
    if (t >= memTResolve || pular) memResolve(t);
    return;
  }
  if (bu) { memMoveCursor(-1); somTique(); }
  if (bd) { memMoveCursor(+1); somTique(); }
  if (bo && !memVirada[memCursor] && !memAchada[memCursor]) memViraCarta(memCursor, t);
}

void memDesenhaCarta(int i, unsigned long t) {
  int x = memGX + (i % memCols) * (memCW + memGapX);
  int y = memGY + (i / memCols) * (memCH + memGapY);
  unsigned long dt = t - memTMuda[i];
  bool face = memVirada[i];
  int w = memCW;
  if (dt < 180) {                            // animacao: estreita, troca de lado e abre
    float u = dt / 180.0f;
    if (u < 0.5f) { face = !face; w = (int)(memCW * (1.0f - 2.0f * u)); }
    else          { w = (int)(memCW * (2.0f * u - 1.0f)); }
    if (w < 3) w = 3;
  }
  int xx = x + (memCW - w) / 2;
  int cx = x + memCW / 2, cy = y + memCH / 2;
  int r = (memCH >= 14) ? 3 : 2;

  if (face && memAchada[i]) {                // par achado: cartinha "branquinha"
    display.fillRoundRect(xx, y, w, memCH, r, WHITE);
    if (w >= 9) memDesenhaSprite(memBaralho[i], cx, cy, BLACK);
  } else if (face) {
    display.drawRoundRect(xx, y, w, memCH, r, WHITE);
    if (w >= 9) memDesenhaSprite(memBaralho[i], cx, cy, WHITE);
  } else {                                   // costas da carta: brilhinho no meio
    display.drawRoundRect(xx, y, w, memCH, r, WHITE);
    if (w >= 7) brilhinho(cx, cy, (memCH >= 14) ? 2 : 1);
  }

  // cursor piscando
  bool sel = (i == memCursor) && memEstado == MEM_E_PARES && !memEspiando &&
             !memResolvendo && !memAchada[i] && !memVirada[i];
  if (sel && ((t / 300) % 2 == 0)) display.fillRect(x + 1, y + 1, memCW - 2, memCH - 2, INVERSE);

  // brilhinhos quando acha um par
  if (t - memTBrilho < 700 && (i == memBrilhoA || i == memBrilhoB)) {
    int rr = ((t / 120) % 2) ? 2 : 1;
    brilhinho(x + memCW - 1, y, rr);
    brilhinho(x, y + memCH - 1, 1);
  }
}

void memDesenhaPares(unsigned long t) {
  char buf[16], tb[8];
  display.clearDisplay();
  display.setTextSize(1);

  display.fillRect(0, 0, SCREEN_W, 10, WHITE);
  display.setTextColor(BLACK);
  snprintf(buf, sizeof(buf), "J:%d", memJogadas);
  display.setCursor(3, 1);
  display.print(buf);
  if (memEspiando && (t / 300) % 2 == 0) textoCentralizado("OLHE BEM!", 1);
  else                                   textoCentralizado("PARES", 1);

  unsigned long fim = t;
  if (memEstado == MEM_E_VIT)       fim = memTFim;
  else if (memEstado == MEM_E_SAIR) fim = memTPausaIni;
  int segs = memEspiando ? 0 : (int)((fim - memTInicio) / 1000);
  memTempoTxt(tb, sizeof(tb), segs);
  display.setCursor(95, 1);
  display.print(tb);
  display.setTextColor(WHITE);

  for (int i = 0; i < memNCartas; i++) memDesenhaCarta(i, t);
}

// ============================================================
//  GENIUS
// ============================================================
void memNovoGenius() {
  unsigned long t = millis();
  memGLen = 1;
  memGSeq[0] = random(4);
  memGIdx = 0;
  memGFase = MEM_G_MOSTRA;
  memGT = t + 800;
  memGPad = -1;
  memGFeitas = 0; memGGanho = 0; memPontos = 0;
  memGBackPend = false;
  memNovoRec = false; memVenceu = false;
  memEstado = MEM_E_GENIUS;
}

int memGDur() {
  int d = (int)MEM_G_DUR[memNivel] - memGLen * 12;
  if (d < (int)MEM_G_MIN[memNivel]) d = MEM_G_MIN[memNivel];
  return d;
}

void memGAcende(int pad, int ms, unsigned long t) {
  memGPad = pad;
  memGPadAte = t + ms;
  memTomLiga(MEM_NOTAS[pad]);
}

void memGJogada(int pad, unsigned long t) {
  if (pad != 3) memGAcende(pad, 250, t);
  if (pad == memGSeq[memGIdx]) {
    memGIdx++;
    if (memGIdx >= memGLen) {                // rodada completa
      int ganho = memGLen * 10 * (memNivel + 1);
      memPontos += ganho;
      memGGanho = ganho;
      memGFeitas = memGLen;
      if (memGLen >= MEM_G_RODADAS[memNivel]) {          // zerou o nivel!
        memPontos += 100 * (memNivel + 1);
        memVenceu = true;
        memTomDesliga();
        memGPad = -1;
        memSalvaRecorde();
        memTFim = t;
        memEstado = MEM_E_VIT;
        return;
      }
      memGFase = MEM_G_ACERTO;
      memGT = t + 900;
    }
  } else {                                   // errou
    memGEsperado = memGSeq[memGIdx];
    memGFase = MEM_G_ERRO;
    memGT = t;
    memTomDesliga();
    memGPad = -1;
    somErro();
  }
}

void memGeniusAtualiza(unsigned long t, bool bu, bool bd, bool bo, bool bb) {
  // apaga o pad (e o som) quando o tempo dele acabar
  if (memGPad >= 0 && t >= memGPadAte) { memGPad = -1; memTomDesliga(); }

  // VOLTA: toque curto = tecla | segurar = sair
  bool backBaixo = !digitalRead(PIN_BTN_BACK);
  if (bb) {
    memGBackT = t;
    memGBackPend = true;
    if (memGFase == MEM_G_JOGA) memGAcende(3, 250, t);
  }
  if (memGBackPend) {
    unsigned long seg = t - memGBackT;
    if (!backBaixo) {                        // soltou
      memGBackPend = false;
      if (seg < 600 && memGFase == MEM_G_JOGA) { memGJogada(3, t); return; }
    } else if (seg >= 1300) {                // segurou: pergunta se quer sair
      memGBackPend = false;
      memTomDesliga();
      memGPad = -1;
      memTPausaIni = t;
      memEstado = MEM_E_SAIR;
      esperaSoltar();
      return;
    }
  }

  if (memGFase == MEM_G_MOSTRA) {
    if (t >= memGT) {
      if (memGIdx >= memGLen) { memGFase = MEM_G_JOGA; memGIdx = 0; }
      else {
        int dur = memGDur();
        memGAcende(memGSeq[memGIdx], dur, t);
        memGT = t + dur + dur / 3 + 60;
        memGIdx++;
      }
    }
  }
  else if (memGFase == MEM_G_JOGA) {
    if (bu) { memGJogada(0, t); return; }
    if (bd) { memGJogada(1, t); return; }
    if (bo) { memGJogada(2, t); return; }
  }
  else if (memGFase == MEM_G_ACERTO) {
    if (t >= memGT) {                        // acrescenta mais um passo
      memGSeq[memGLen] = random(4);
      memGLen++;
      memGIdx = 0;
      memGFase = MEM_G_MOSTRA;
      memGT = t + 400;
    }
  }
  else if (memGFase == MEM_G_ERRO) {
    if (t - memGT > 1300) {                  // acabou: tela final
      memVenceu = false;
      memSalvaRecorde();
      memEntraFim(t);
    }
  }
}

void memDesenhaPad(int i, bool aceso, unsigned long t) {
  const int x = 3 + i * 31, y = 16, w = 28, h = 32;
  int cx = x + w / 2, cy = y + h / 2;
  uint16_t cor = aceso ? BLACK : WHITE;
  if (aceso) display.fillRoundRect(x, y, w, h, 5, WHITE);
  else       display.drawRoundRect(x, y, w, h, 5, WHITE);

  switch (i) {
    case 0:   // CIMA
      if (aceso) display.fillTriangle(cx, cy - 8, cx - 9, cy + 7, cx + 9, cy + 7, cor);
      else       display.drawTriangle(cx, cy - 8, cx - 9, cy + 7, cx + 9, cy + 7, cor);
      break;
    case 1:   // BAIXO
      if (aceso) display.fillTriangle(cx, cy + 8, cx - 9, cy - 7, cx + 9, cy - 7, cor);
      else       display.drawTriangle(cx, cy + 8, cx - 9, cy - 7, cx + 9, cy - 7, cor);
      break;
    case 2:   // OK (estrelinha)
      brilhinhoCor(cx, cy, 8, cor);
      display.drawLine(cx - 5, cy - 5, cx + 5, cy + 5, cor);
      display.drawLine(cx - 5, cy + 5, cx + 5, cy - 5, cor);
      break;
    case 3:   // VOLTA
      if (aceso) display.fillTriangle(cx - 9, cy, cx + 7, cy - 8, cx + 7, cy + 8, cor);
      else       display.drawTriangle(cx - 9, cy, cx + 7, cy - 8, cx + 7, cy + 8, cor);
      break;
  }

  if (aceso) {                               // brilhinhos em cima do pad aceso
    int rr = ((t / 110) % 2) ? 2 : 1;
    brilhinho(x + 4, 12, rr);
    brilhinho(x + w - 5, 12, 3 - rr);
  }
}

void memDesenhaGenius(unsigned long t) {
  char buf[20];
  display.clearDisplay();
  display.setTextSize(1);

  display.fillRect(0, 0, SCREEN_W, 10, WHITE);
  display.setTextColor(BLACK);
  display.setCursor(3, 1);
  display.print("GENIUS");
  snprintf(buf, sizeof(buf), "R%d/%d", memGLen, (int)MEM_G_RODADAS[memNivel]);
  display.setCursor(50, 1);
  display.print(buf);
  snprintf(buf, sizeof(buf), "%d", memPontos);
  display.setCursor(125 - (int)strlen(buf) * 6, 1);
  display.print(buf);
  display.setTextColor(WHITE);

  ceuEstrelado(2, 12, 125, 13, 6, 41);       // estrelinhas acima dos pads

  int aceso = memGPad;
  if (memGFase == MEM_G_ERRO && ((t / 180) % 2 == 0)) aceso = memGEsperado;
  for (int i = 0; i < 4; i++) memDesenhaPad(i, i == aceso, t);

  // mensagem de baixo
  bool segurando = memGBackPend && (t - memGBackT) > 600;
  if (segurando) {
    int p = (int)(((t - memGBackT) - 600) * 100 / 700);
    if (p > 100) p = 100;
    display.setCursor(66, 54);
    display.print("sair...");
    display.drawRect(3, 55, 58, 6, WHITE);
    display.fillRect(5, 57, (54 * p) / 100, 2, WHITE);
  } else {
    display.setCursor(3, 54);
    if (memGFase == MEM_G_MOSTRA) {
      if (memGLen == 1 && (t / 1500) % 2 == 1) display.print("VOLTA longa: sair");
      else                                      display.print("OBSERVE...");
    } else if (memGFase == MEM_G_JOGA) {
      snprintf(buf, sizeof(buf), "SUA VEZ %d/%d", memGIdx + 1, memGLen);
      display.print(buf);
    } else if (memGFase == MEM_G_ACERTO) {
      snprintf(buf, sizeof(buf), "BOA! +%d", memGGanho);
      display.print(buf);
    } else {
      display.print("OPS! era este");
    }
  }
  desenhaPurinMini(112, 51, 0, (t % 3000) < 130);
}

// ============================================================
//  TELAS DE ESCOLHA
// ============================================================
void memDesenhaEscolhaModo(unsigned long t) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  ceuEstrelado(0, 10, 127, 12, 5, 61);
  ceuEstrelado(0, 61, 127, 63, 6, 62);

  display.fillRect(0, 0, SCREEN_W, 10, WHITE);
  display.setTextColor(BLACK);
  textoCentralizado("MEMORIA", 1);
  display.setTextColor(WHITE);

  const char* nomes[2] = { "PARES", "GENIUS" };
  for (int i = 0; i < 2; i++) {
    int x = 4 + i * 62, y = 13, w = 58, h = 37;
    bool sel = (i == memModoSel);
    uint16_t cor = sel ? BLACK : WHITE;
    uint16_t inv = sel ? WHITE : BLACK;
    if (sel) display.fillRoundRect(x, y, w, h, 4, WHITE);
    else     display.drawRoundRect(x, y, w, h, 4, WHITE);

    if (i == 0) {                            // mini cartinhas 3x2
      int cyc = (int)((t / 600) % 3);
      int x0 = x + w / 2 - 19, y0 = y + 4;
      for (int r = 0; r < 2; r++) {
        for (int c = 0; c < 3; c++) {
          int cx = x0 + c * 14, cy = y0 + r * 12;
          if (c == cyc) {
            display.fillRoundRect(cx, cy, 11, 9, 2, cor);
            memDesenhaSprite(c, cx + 5, cy + 4, inv);
          } else {
            display.drawRoundRect(cx, cy, 11, 9, 2, cor);
            display.drawPixel(cx + 5, cy + 4, cor);
          }
        }
      }
    } else {                                 // mini pads
      int lit = (int)((t / 450) % 4);
      int x0 = x + w / 2 - 26, y0 = y + 5;
      for (int k = 0; k < 4; k++) {
        int px = x0 + k * 14;
        if (k == lit) display.fillRoundRect(px, y0, 11, 16, 3, cor);
        else          display.drawRoundRect(px, y0, 11, 16, 3, cor);
      }
    }

    display.setTextColor(cor);
    display.setCursor(x + (w - (int)strlen(nomes[i]) * 6) / 2, y + 26);
    display.print(nomes[i]);
    display.setTextColor(WHITE);

    if (sel) {
      if ((t / 300) % 2 == 0) brilhinho(x - 2, y + 18, 1);
      else                    brilhinho(x + w + 1, y + 18, 1);
    }
  }

  char buf[24];
  int melhor = memMelhorModo(memModoSel);
  if (melhor > 0) snprintf(buf, sizeof(buf), "melhor %d", melhor);
  else            snprintf(buf, sizeof(buf), "sem recorde ainda");
  textoCentralizado(buf, 53);
}

void memDesenhaEscolhaNivel(unsigned long t) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  ceuEstrelado(0, 11, 4, 51, 3, 64);
  ceuEstrelado(123, 11, 127, 51, 3, 65);
  ceuEstrelado(0, 62, 100, 63, 4, 66);

  const char* titulo = (memModoSel == 0) ? "PARES: NIVEL" : "GENIUS: NIVEL";
  display.fillRect(0, 0, SCREEN_W, 10, WHITE);
  display.setTextColor(BLACK);
  display.setCursor((SCREEN_W - (int)strlen(titulo) * 6) / 2, 1);
  display.print(titulo);
  display.setTextColor(WHITE);

  char buf[16];
  for (int i = 0; i < 3; i++) {
    int y = 13 + i * 13;
    bool sel = (i == memNivelSel);
    uint16_t cor = sel ? BLACK : WHITE;
    if (sel) display.fillRoundRect(6, y, 116, 12, 3, WHITE);
    else     display.drawRoundRect(6, y, 116, 12, 3, WHITE);

    display.setTextColor(cor);
    display.setCursor(12, y + 2);
    display.print(MEM_NOME_NIVEL[i]);

    for (int k = 0; k < 3; k++) {            // 1, 2 ou 3 brilhinhos
      int bx = 64 + k * 9, by = y + 6;
      if (k <= i) {
        int r = (sel && ((t / 300) + k) % 2 == 0) ? 3 : 2;
        brilhinhoCor(bx, by, r, cor);
      } else {
        display.drawPixel(bx, by, cor);
      }
    }

    int m = memRec[memModoSel * 3 + i];
    if (m > 0) snprintf(buf, sizeof(buf), "%d", m);
    else       snprintf(buf, sizeof(buf), "--");
    display.setCursor(94, y + 2);
    display.print(buf);
    display.setTextColor(WHITE);
  }

  display.setCursor(2, 55);
  display.print(memModoSel == 0 ? MEM_INFO_PARES[memNivelSel] : MEM_INFO_GENIUS[memNivelSel]);
  desenhaPurinMini(112, 51, 0, (t % 3000) < 130);
}

// ============================================================
//  TELA FINAL
// ============================================================
void memDesenhaFim(unsigned long t) {
  char buf[24], tb[8];
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  ceuEstrelado(0, 0, 46, 63, 8, 6);

  if (memNovoRec) {
    desenhaConfete(t);
    desenhaBmp7(8, 22 - (int)((t / 120) % 8), BMP_CORACAO, 6, 1);
    desenhaBmp7(41, 28 - (int)((t / 150) % 8), BMP_CORACAO, 6, 1);
  }
  if (memNovoRec || memVenceu) {
    float fase = (t % 700) / 700.0f;
    int pulo = (int)(4 * fase * (1 - fase) * 8);          // Purin pulando de alegria
    desenhaPurin(24, 58, pulo, 0);
  } else {
    desenhaPurin(24, 58, 0, 0);
  }

  display.setCursor(52, 5);
  display.print(memVenceu ? "PARABENS!" : "FIM DE JOGO");

  if (memModo == 0) {
    memTempoTxt(tb, sizeof(tb), memSegs);
    snprintf(buf, sizeof(buf), "%dj %s", memJogadas, tb);
  } else {
    snprintf(buf, sizeof(buf), "rodada %d/%d", memGFeitas, (int)MEM_G_RODADAS[memNivel]);
  }
  display.setCursor(52, 15);
  display.print(buf);

  snprintf(buf, sizeof(buf), "pts %d", memPontos);
  display.setCursor(52, 25);
  display.print(buf);

  if (memNovoRec) {
    if ((t / 250) % 2 == 0) {
      display.setCursor(52, 35);
      display.print("* RECORDE *");
    }
  } else {
    snprintf(buf, sizeof(buf), "melhor %d", memRec[memModo * 3 + memNivel]);
    display.setCursor(52, 35);
    display.print(buf);
  }
  display.setCursor(52, 46); display.print("OK: de novo");
  display.setCursor(52, 55); display.print("BACK: sair");
}

// ============================================================
//  ICONE NO GAME CENTER
// ============================================================
void memIconeCard() {
  unsigned long t = millis();
  const int x0 = 45, y0 = 25;                // 3x2 cartinhas de 11x10
  int lit = (int)((t / 650) % 3);
  for (int r = 0; r < 2; r++) {
    for (int c = 0; c < 3; c++) {
      int x = x0 + c * 14, y = y0 + r * 13;
      if (c == lit) {
        display.fillRoundRect(x, y, 11, 10, 2, WHITE);
        memDesenhaSprite(c, x + 5, y + 5, BLACK);
      } else {
        display.drawRoundRect(x, y, 11, 10, 2, WHITE);
        display.drawPixel(x + 5, y + 5, WHITE);
      }
    }
  }
  if ((t / 400) % 2 == 0) { brilhinho(38, 30, 1); brilhinho(88, 44, 2); }
  else                    { brilhinho(39, 44, 2); brilhinho(87, 30, 1); }
}

// ============================================================
//  TELA PRINCIPAL DA MEMORIA
// ============================================================
void memTela() {
  ultimaAtividade = millis();
  unsigned long t = millis();
  bool rep = (memEstado != MEM_E_GENIUS);    // no Genius os botoes nao repetem
  bool bu = clicou(bUp, rep);
  bool bd = clicou(bDown, rep);
  bool bo = clicou(bOk, false);
  bool bb = clicou(bBack, false);

  // ----- escolher jogo -----
  if (memEstado == MEM_E_MODO) {
    if (bu || bd) { memModoSel = 1 - memModoSel; somTique(); }
    if (bo) { somConfirma(); memEstado = MEM_E_NIVEL; return; }
    if (bb) { somVoltar(); esperaSoltar(); telaAtual = TELA_JOGOS_MENU; return; }
    memDesenhaEscolhaModo(t);
    display.display();
    return;
  }

  // ----- escolher nivel -----
  if (memEstado == MEM_E_NIVEL) {
    if (bu) { memNivelSel = (memNivelSel + 2) % 3; somTique(); }
    if (bd) { memNivelSel = (memNivelSel + 1) % 3; somTique(); }
    if (bo) {
      somConfirma();
      esperaSoltar();
      memModo = memModoSel;
      memNivel = memNivelSel;
      if (memModo == 0) memNovoPares(); else memNovoGenius();
      return;
    }
    if (bb) { somVoltar(); memEstado = MEM_E_MODO; return; }
    memDesenhaEscolhaNivel(t);
    display.display();
    return;
  }

  // ----- jogando PARES -----
  if (memEstado == MEM_E_PARES) {
    if (bb) { somVoltar(); memTPausaIni = t; memEstado = MEM_E_SAIR; return; }
    memParesAtualiza(t, bu, bd, bo);
    if (memEstado != MEM_E_PARES) return;
    memDesenhaPares(t);
    display.display();
    return;
  }

  // ----- jogando GENIUS -----
  if (memEstado == MEM_E_GENIUS) {
    memGeniusAtualiza(t, bu, bd, bo, bb);
    if (memEstado != MEM_E_GENIUS) return;
    memDesenhaGenius(t);
    display.display();
    return;
  }

  // ----- "quer sair?" -----
  if (memEstado == MEM_E_SAIR) {
    if (bo) { somVoltar(); memTomDesliga(); memEstado = MEM_E_MODO; return; }
    if (bb) {                                // continua: desconta o tempo parado
      unsigned long pausa = t - memTPausaIni;
      somConfirma();
      if (memModo == 0) {
        memTInicio += pausa;
        memTEspiaFim += pausa;
        memTResolve += pausa;
        memEstado = MEM_E_PARES;
      } else {
        if (memGFase == MEM_G_ACERTO) { memGT = t + 400; }
        else { memGFase = MEM_G_MOSTRA; memGIdx = 0; memGT = t + 700; }
        memGBackPend = false;
        memEstado = MEM_E_GENIUS;
      }
      return;
    }
    if (memModo == 0) memDesenhaPares(t); else memDesenhaGenius(t);
    display.fillRoundRect(10, 18, 108, 28, 4, BLACK);
    display.drawRoundRect(10, 18, 108, 28, 4, WHITE);
    textoCentralizado("SAIR DO JOGO?", 23);
    textoCentralizado("OK:sim BACK:nao", 34);
    display.display();
    return;
  }

  // ----- comemorando -----
  if (memEstado == MEM_E_VIT) {
    if (t - memTFim > 1400) { memEntraFim(t); return; }
    if (memModo == 0) memDesenhaPares(t); else memDesenhaGenius(t);
    desenhaConfete(t - memTFim);
    display.display();
    return;
  }

  // ----- tela final -----
  if (t - memTFim > 700) {                   // evita apertar sem querer
    if (bo) {
      somConfirma();
      esperaSoltar();
      if (memModo == 0) memNovoPares(); else memNovoGenius();
      return;
    }
    if (bb) { somVoltar(); memEstado = MEM_E_MODO; return; }
  }
  memDesenhaFim(t - memTFim);
  display.display();
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

  carregarConfiguracoes();
  iniciaBuzzer();
  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(cfgContraste);

  // hora: fuso + plano B + Wi-Fi em segundo plano
  setenv("TZ", FUSO, 1);
  tzset();
  iniciaHoraCompilacao();
  wifiIniciar();

  sequenciaBoot();
  ultimaAtividade = millis();
}

void loop() {
  wifiAtualiza();
  atualizaPet();
  atualizaHorarioPet();

    // ---- protecao de tela (descanso) ----
  if (algumBotao()) {
    if (descansando) {            // o primeiro toque so acorda
      sairDescanso();
      esperaSoltar();
      return;
    }
    ultimaAtividade = millis();
  }
  if (!descansando && cfgTempoDescanso > 0 &&
      millis() - ultimaAtividade > (unsigned long)cfgTempoDescanso * 1000UL) {
    entrarDescanso();
  }
  if (descansando) {
    telaDescanso();
    delay(5);
    return;
  }

  switch (telaAtual) {
    case TELA_BOAS_VINDAS: telaBoasVindas(); break;
    case TELA_MENU:        telaMenu();       break;
    case TELA_RELOGIO:     telaRelogio();    break;
    case TELA_TAROT:
      controlarTarot();
      telaTarot();
      break;
    case TELA_PET_QUARTO:
      controlarPetQuarto();
      telaPetQuarto();
      break;
    case TELA_PET_STATUS:
      controlarPetStatus();
      telaPetStatus();
      break;  
    case TELA_PET_ACOES:
      controlarPetAcoes();
      telaPetAcoes();
      break;
    case TELA_CONFIGURACOES:
      controlarConfiguracoes();
      telaConfiguracoes();
      break;
    case TELA_CFG_TELA:
      controlarConfigTela();
      telaConfigTela();
      break;
    case TELA_CFG_SOM:
      controlarConfigSom();
      telaConfigSom();
      break;
    case TELA_JOGOS_MENU: telaJogosMenu(); break;
    case TELA_DINO:       telaDino();      break;
    case TELA_PONG:       telaPong();      break;
    case TELA_VELHA_MENU: vhTelaMenu(); break;
    case TELA_VELHA_JOGO: vhTelaJogo(); break;
    case TELA_SUDOKU: sudTela(); break;
    case TELA_MEMORIA: memTela(); break;

  }
  delay(5);   // pequeno para a animacao do menu ficar fluida
}
