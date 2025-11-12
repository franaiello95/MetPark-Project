#include <Servo.h>
#include <Adafruit_NeoPixel.h>
#include <Keypad.h>

// ===== Configuración de hardware =====
#define LED_PIN 52
#define NUMPIXELS 4

#define SENSOR_ENTRADA 47
#define SERVO_ENTRADA 45
#define SENSOR_SALIDA 12
#define SERVO_SALIDA 13

const int lugarPin[4] = {23, 25, 27, 29};

Adafruit_NeoPixel pixels(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

Servo barreraEntrada;
Servo barreraSalida;

const int ANGULO_ABIERTO = 90;
const int ANGULO_CERRADO = 0;
unsigned long tiempoEspera = 1500;

// ===== Teclado 4x4 =====
const byte FILAS = 4;
const byte COLUMNAS = 4;
char teclas[FILAS][COLUMNAS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte pinesFilas[FILAS] = {37, 39, 41, 43};
byte pinesColumnas[COLUMNAS] = {36, 38, 40, 42};
Keypad teclado = Keypad(makeKeymap(teclas), pinesFilas, pinesColumnas, FILAS, COLUMNAS);

// ===== Variables =====
bool reservaActiva[4] = {false, false, false, false};
String estadoAnterior[4] = {"", "", "", ""};
unsigned long lastEnvioEstado[4] = {0,0,0,0};
const unsigned long intervaloEnvio = 10000; // 10 s entre envíos repetidos

String codigoAcceso[4] = {"0000", "0000", "0000", "0000"};
String codigoIngresado = "";

bool barreraEntradaAbierta = false;
bool barreraSalidaAbierta = false;
unsigned long tiempoCierreProgramadoEntrada = 0;
unsigned long tiempoCierreProgramadoSalida = 0;

// ===== Funciones LED =====
void setColor(int plaza, uint8_t r, uint8_t g, uint8_t b) {
  pixels.setPixelColor(plaza, pixels.Color(r, g, b));
}

void actualizarLeds() {
  for (int i = 0; i < 4; i++) {
    bool libre = digitalRead(lugarPin[i]) == HIGH;
    if (reservaActiva[i]) setColor(i, 255, 255, 0);       // 🟡 Reservado
    else if (libre) setColor(i, 0, 255, 0);               // 🟢 Libre
    else setColor(i, 255, 0, 0);                          // 🔴 Ocupado
  }
  pixels.show();
}

// ===== Envío de estados al Wemos =====
void enviarEstado(int plaza, const String& estado) {
  Serial1.println("estado" + String(plaza+1) + "=" + estado);
  Serial.print("→ Enviado plaza ");
  Serial.print(plaza+1);
  Serial.print(": ");
  Serial.println(estado);
}

// ===== Setup =====
void setup() {
  Serial.begin(115200);  // Para monitor serie
  Serial1.begin(9600);   // Comunicación con Wemos (TX1/RX1)
  
  for (int i = 0; i < 4; i++) pinMode(lugarPin[i], INPUT);
  pinMode(SENSOR_ENTRADA, INPUT);
  pinMode(SENSOR_SALIDA, INPUT);

  barreraEntrada.attach(SERVO_ENTRADA);
  barreraSalida.attach(SERVO_SALIDA);
  barreraEntrada.write(ANGULO_CERRADO);
  barreraSalida.write(ANGULO_CERRADO);

  pixels.begin();
  actualizarLeds();

  Serial.println("🟢 Mega listo (4 plazas + teclado + servos + LED Neopixel + Serial1)");
}

// ===== Loop =====
void loop() {
  // === Lectura sensores ===
  int entradaDetectada = digitalRead(SENSOR_ENTRADA);
  int salidaDetectada = digitalRead(SENSOR_SALIDA);

  // === Actualizar estados y enviar al Wemos ===
  for (int i = 0; i < 4; i++) {
    bool libre = digitalRead(lugarPin[i]) == HIGH;
    String estadoActual;

    if (!libre) estadoActual = "ocupado";
    else if (reservaActiva[i]) estadoActual = "reservado";
    else estadoActual = "libre";

    if (estadoActual != estadoAnterior[i] || millis() - lastEnvioEstado[i] > intervaloEnvio) {
      enviarEstado(i, estadoActual);
      estadoAnterior[i] = estadoActual;
      lastEnvioEstado[i] = millis();
    }
  }

  // === Teclado ===
  char tecla = teclado.getKey();
  if (tecla) {
    if (tecla == '#') {  // Confirmar
      for (int i = 0; i < 4; i++) {
        if (codigoIngresado == codigoAcceso[i]) {
          Serial.print("🔓 Código correcto para plaza ");
          Serial.println(i+1);
          barreraSalida.write(ANGULO_ABIERTO);
          barreraSalidaAbierta = true;
          tiempoCierreProgramadoSalida = millis() + tiempoEspera;
          break;
        }
      }
      codigoIngresado = "";
    }
    else if (tecla == '*') {
      codigoIngresado = "";
    }
    else {
      codigoIngresado += tecla;
      if (codigoIngresado.length() > 4)
        codigoIngresado.remove(0, codigoIngresado.length() - 4);
    }
  }

  // === Leer comandos del Wemos ===
  if (Serial1.available()) {
    String comando = Serial1.readStringUntil('\n');
    comando.trim();

    if (comando.startsWith("reserva:") && comando.endsWith("_fin")) {
      int plaza = comando.substring(8, comando.indexOf("_fin")).toInt() - 1;
      if (plaza >= 0 && plaza < 4) {
        reservaActiva[plaza] = false;
        Serial.print("⌛ Reserva finalizada plaza ");
        Serial.println(plaza+1);
      }
    }
    else if (comando.startsWith("codigo")) {
      int plaza = comando.substring(6, 7).toInt() - 1;
      String codigo = comando.substring(8);
      if (plaza >= 0 && plaza < 4) {
        codigoAcceso[plaza] = codigo;
        Serial.print("🔢 Código acceso plaza ");
        Serial.print(plaza+1);
        Serial.print(": ");
        Serial.println(codigo);
      }
    }
  }

  actualizarLeds();
  delay(100);
}
