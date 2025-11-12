#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

// ====== CREDENCIALES WIFI ======
const char* ssid = "Informatica2043";
const char* password = "Info20432025+";

// ====== FIREBASE (REST) ======
const char* host = "sahurparkingfirebase-default-rtdb.firebaseio.com";
const int httpsPort = 443;
const char* auth = "";  // vacío si la DB es pública

// ====== RUTAS FIREBASE ======
const char* recursosPlaza[4] = {
  "/Parking_copy_copy_checkpoint18_checkpoint3/plaza1.json",
  "/Parking_copy_copy_checkpoint18_checkpoint3/plaza2.json",
  "/Parking_copy_copy_checkpoint18_checkpoint3/plaza3.json",
  "/Parking_copy_copy_checkpoint18_checkpoint3/plaza4.json"
};

const char* recursosContador[4] = {
  "/Parking_copy_copy_checkpoint18_checkpoint3/Contador1.json",
  "/Parking_copy_copy_checkpoint18_checkpoint3/Contador2.json",
  "/Parking_copy_copy_checkpoint18_checkpoint3/Contador3.json",
  "/Parking_copy_copy_checkpoint18_checkpoint3/Contador4.json"
};

// 🔢 Códigos de acceso por dígito (1–4)
String recursosCodigo[4][4] = {
  {
    "/Parking_copy_copy_checkpoint18_checkpoint3/codigoAcceso1-1.json",
    "/Parking_copy_copy_checkpoint18_checkpoint3/codigoAcceso1-2.json",
    "/Parking_copy_copy_checkpoint18_checkpoint3/codigoAcceso1-3.json",
    "/Parking_copy_copy_checkpoint18_checkpoint3/codigoAcceso1-4.json"
  },
  {
    "/Parking_copy_copy_checkpoint18_checkpoint3/codigoAcceso2-1.json",
    "/Parking_copy_copy_checkpoint18_checkpoint3/codigoAcceso2-2.json",
    "/Parking_copy_copy_checkpoint18_checkpoint3/codigoAcceso2-3.json",
    "/Parking_copy_copy_checkpoint18_checkpoint3/codigoAcceso2-4.json"
  },
  {
    "/Parking_copy_copy_checkpoint18_checkpoint3/codigoAcceso3-1.json",
    "/Parking_copy_copy_checkpoint18_checkpoint3/codigoAcceso3-2.json",
    "/Parking_copy_copy_checkpoint18_checkpoint3/codigoAcceso3-3.json",
    "/Parking_copy_copy_checkpoint18_checkpoint3/codigoAcceso3-4.json"
  },
  {
    "/Parking_copy_copy_checkpoint18_checkpoint3/codigoAcceso4-1.json",
    "/Parking_copy_copy_checkpoint18_checkpoint3/codigoAcceso4-2.json",
    "/Parking_copy_copy_checkpoint18_checkpoint3/codigoAcceso4-3.json",
    "/Parking_copy_copy_checkpoint18_checkpoint3/codigoAcceso4-4.json"
  }
};

WiFiClientSecure client;

// ====== VARIABLES ======
String estadoFisico[4] = {"", "", "", ""};
String ultimoPublicado[4] = {"", "", "", ""};
int contadorActualSeg[4] = {0, 0, 0, 0};

bool reservaActiva[4] = {false, false, false, false};
unsigned long reservaFinMs[4] = {0, 0, 0, 0};

String codigoAcceso[4] = {"", "", "", ""};
String ultimoCodigoEnviado[4] = {"", "", "", ""};

unsigned long lastLecturaFirebase = 0;
const unsigned long intervaloLectura = 1000;
unsigned long lastDataReceived = 0;
const unsigned long avisoIntervalo = 5000;

// ====== WIFI ======
void setupWifi() {
  Serial.println("Conectando a WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi conectado");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  client.setInsecure();
  client.setTimeout(3000);
}

// ====== LECTURA FIREBASE ======
String leerDeFirebase(const char* recurso) {
  if (!client.connect(host, httpsPort)) return "";
  String url = String(recurso);
  if (strlen(auth) > 0) url += "?auth=" + String(auth);

  String request =
    String("GET ") + url + " HTTP/1.1\r\n" +
    "Host: " + host + "\r\n" +
    "Connection: close\r\n\r\n";

  client.print(request);
  String statusLine = client.readStringUntil('\n');
  if (!statusLine.startsWith("HTTP/1.1 200")) {
    client.stop();
    return "";
  }

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") break;
  }

  String cuerpo = client.readString();
  cuerpo.trim();
  client.stop();

  if (cuerpo.startsWith("\"") && cuerpo.endsWith("\""))
    cuerpo = cuerpo.substring(1, cuerpo.length() - 1);

  return cuerpo;
}

// ====== ESCRITURA FIREBASE ======
bool enviarAFirebase(const char* recurso, const String& valor) {
  if (!client.connect(host, httpsPort)) return false;
  String url = String(recurso);
  if (strlen(auth) > 0) url += "?auth=" + String(auth);
  String cuerpo = "\"" + valor + "\"";
  String request =
    String("PUT ") + url + " HTTP/1.1\r\n" +
    "Host: " + host + "\r\n" +
    "Connection: close\r\n" +
    "Content-Type: application/json\r\n" +
    "Content-Length: " + String(cuerpo.length()) + "\r\n\r\n" +
    cuerpo;
  client.print(request);
  client.readStringUntil('\n');
  client.stop();
  return true;
}

// ====== SETUP ======
void setup() {
  Serial.begin(9600);
  delay(1000);
  setupWifi();
  Serial.println("🚀 Wemos listo (4 plazas + códigos de acceso condicionales)");
  lastDataReceived = millis();
}

// ====== LOOP ======
void loop() {
  // 1️⃣ Leer datos físicos del Mega
  if (Serial.available()) {
    String msg = Serial.readStringUntil('\n');
    msg.trim();
    for (int i = 0; i < 4; i++) {
      if (msg.startsWith("estado" + String(i + 1) + "="))
        estadoFisico[i] = msg.substring(8);
    }
    lastDataReceived = millis();
  }

  // 2️⃣ Lectura periódica Firebase
  if (millis() - lastLecturaFirebase > intervaloLectura) {
    for (int i = 0; i < 4; i++) {

      // ----- Leer contador -----
      String contadorStr = leerDeFirebase(recursosContador[i]);
      if (contadorStr != "") {
        int nuevoContadorSeg = contadorStr.toInt();
        if (nuevoContadorSeg > 0) {
          contadorActualSeg[i] = nuevoContadorSeg;
          reservaFinMs[i] = millis() + (unsigned long)nuevoContadorSeg * 1000UL;
          if (!reservaActiva[i]) {
            reservaActiva[i] = true;
            Serial.println("reserva:" + String(i + 1));
          }
        } else if (reservaActiva[i] && millis() >= reservaFinMs[i]) {
          reservaActiva[i] = false;
          reservaFinMs[i] = 0;
          contadorActualSeg[i] = 0;
          Serial.println("reserva:" + String(i + 1) + "_fin");
        }
      }

      // ----- Leer código solo si hay reserva -----
      if (reservaActiva[i]) {
        String nuevoCodigo = "";
        for (int d = 0; d < 4; d++) {
          String digito = leerDeFirebase(recursosCodigo[i][d].c_str());
          nuevoCodigo += digito;
        }

        if (nuevoCodigo != "" && nuevoCodigo != ultimoCodigoEnviado[i]) {
          codigoAcceso[i] = nuevoCodigo;
          Serial.println("codigo" + String(i + 1) + "=" + nuevoCodigo);
          ultimoCodigoEnviado[i] = nuevoCodigo;
        }
      }

      // ----- Prioridad física -----
      if (estadoFisico[i] == "ocupado") {
        enviarAFirebase(recursosContador[i], "0");
        reservaActiva[i] = false;
        reservaFinMs[i] = 0;
        contadorActualSeg[i] = 0;
      }

      // ----- Subir estado -----
      String estadoParaSubir;
      if (estadoFisico[i] == "ocupado") estadoParaSubir = "ocupado";
      else if (reservaActiva[i]) estadoParaSubir = "reservado";
      else estadoParaSubir = estadoFisico[i];

      if (estadoParaSubir != ultimoPublicado[i] && estadoParaSubir != "") {
        enviarAFirebase(recursosPlaza[i], estadoParaSubir);
        ultimoPublicado[i] = estadoParaSubir;
      }
    }

    lastLecturaFirebase = millis();
  }

  // 3️⃣ Aviso si no llegan datos del Mega
  if (millis() - lastDataReceived > avisoIntervalo) {
    Serial.println("⏳ Esperando datos del Mega...");
    lastDataReceived = millis();
  }

  delay(80);
}
