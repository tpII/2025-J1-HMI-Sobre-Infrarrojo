#define SEND_NEC
#include <IRremote.hpp>
#include <WiFi.h>
#include <WebServer.h>

// ==== CONFIGURACIÓN WiFi ====
const char* ssid = "Fibertel WiFi773 2.4GHz";
const char* password = "0042375774";

WebServer server(80);

// ==== PINES IR ====
const uint8_t SEND_PIN = 17;
const uint8_t RECV_PIN = 16;

// ==== ADDRESSES ====
const uint16_t CONTROLLER_COMMAND_ADDRESS = 0x10;
const uint16_t CONTROLLER_DATA_ADDRESS = 0x20;
const uint16_t ACTUADOR_ADDRESS = 0x30;
const uint16_t SENSOR_ADDRESS = 0x40;

// ==== COMMANDS REQ TO SENSOR ====
const uint16_t CMD_TEMPERATURE = 0x20;
const uint16_t CMD_HUMIDITY = 0x21;
const uint16_t CMD_LUMINOSITY = 0x22;

// ==== COMMANDS REQ TO ACTUADOR ====
const uint16_t CMD_TEMPERATURE_ACT = 0x30;
const uint16_t CMD_HUMIDITY_ACT = 0x31;
const uint16_t CMD_LUMINOSITY_ACT = 0x32;
const uint16_t CMD_STATUS_ACT = 0x33;

// ==== COMMANDS RESP ====
const uint16_t CMD_ACK = 0xFF;
const uint16_t CMD_STATUS_ON = 0x80;
const uint16_t CMD_STATUS_OFF = 0x90;

// ==== TIMEOUT ====
const unsigned long TIMEOUT = 5000;

// ==== MEMORIA DE DATOS ====
uint8_t temperatureData = 128;  // Default: 25°C
uint8_t humidityData = 128;     // Default: 50%
uint8_t luminosityData = 128;   // Default: mitad rango
uint8_t relayStatus = 0;

// ==== FLAGS DE DATOS VÁLIDOS ====
bool hasTemperature = false;
bool hasHumidity = false;
bool hasLuminosity = false;

// ==== ESTADO DEL SISTEMA ====
enum SystemState {
  STATE_IDLE,
  STATE_REQUESTING_SENSOR_T,
  STATE_REQUESTING_SENSOR_H,
  STATE_REQUESTING_SENSOR_L,
  STATE_SENDING_ACT_T,
  STATE_SENDING_ACT_H,
  STATE_SENDING_ACT_L,
  STATE_REQUESTING_STATUS
};

SystemState currentState = STATE_IDLE;

// ==== HISTORIAL ====
struct LogEntry {
  String hora;
  String accion;
  String variable;
  String valor;
  String resultado;
};
#define MAX_LOGS 20
LogEntry logs[MAX_LOGS];
int logIndex = 0;

// ==== FUNCIONES AUXILIARES ====
String formatTime(unsigned long ms) {
  unsigned long totalSeconds = ms / 1000;
  int hours = (totalSeconds / 3600) % 24;
  int minutes = (totalSeconds / 60) % 60;
  int seconds = totalSeconds % 60;
  char buffer[9];
  sprintf(buffer, "%02d:%02d:%02d", hours, minutes, seconds);
  return String(buffer);
}

void addLog(String accion, String variable, String valor, String resultado) {
  if (logIndex >= MAX_LOGS) logIndex = 0;
  logs[logIndex].hora = formatTime(millis());
  logs[logIndex].accion = accion;
  logs[logIndex].variable = variable;
  logs[logIndex].valor = valor;
  logs[logIndex].resultado = resultado;
  logIndex++;
}

String buildLogHTML() {
  String html = "<h3>Historial de Transacciones</h3><table border='1' style='margin:auto;border-collapse:collapse;'>";
  html += "<tr><th>Hora</th><th>Acción</th><th>Variable</th><th>Valor</th><th>Resultado</th></tr>";
  for (int i = 0; i < logIndex; i++) {
    html += "<tr><td>" + logs[i].hora + "</td><td>" + logs[i].accion +
            "</td><td>" + logs[i].variable + "</td><td>" + logs[i].valor +
            "</td><td>" + logs[i].resultado + "</td></tr>";
  }
  html += "</table>";
  return html;
}

String floatToValue(uint8_t rawValue, const char* type) {
  char buffer[20];
  if (strcmp(type, "T") == 0) {
    float temp = (rawValue / 255.0) * 50.0;
    sprintf(buffer, "%.1f °C", temp);
  } else if (strcmp(type, "H") == 0) {
    float hum = (rawValue / 255.0) * 100.0;
    sprintf(buffer, "%.1f %%", hum);
  } else if (strcmp(type, "L") == 0) {
    float lux = (rawValue / 255.0) * 1000.0;
    sprintf(buffer, "%.0f lx", lux);
  } else {
    sprintf(buffer, "0x%02X", rawValue);
  }
  return String(buffer);
}

// ==== FUNCIONES DE COMUNICACIÓN IR ====
bool requestSensorData(uint16_t command, uint8_t* data) {
  IrSender.sendNEC(CONTROLLER_COMMAND_ADDRESS, command, 0);
  
  unsigned long startTime = millis();
  bool receivedResponse = false;
  
  while (millis() - startTime < TIMEOUT) {
    if (IrReceiver.decode()) {
      uint32_t rawData = IrReceiver.decodedIRData.decodedRawData;
      uint16_t receivedAddress = IrReceiver.decodedIRData.address;
      uint16_t receivedCommand = IrReceiver.decodedIRData.command;
      
      if (rawData != 0 && receivedAddress == SENSOR_ADDRESS) {
        *data = (uint8_t)receivedCommand;
        receivedResponse = true;
        IrReceiver.resume();
        break;
      }
      IrReceiver.resume();
    }
  }
  
  return receivedResponse;
}

bool sendDataToActuator(uint16_t requestCommand, uint8_t dataToSend) {
  IrSender.sendNEC(CONTROLLER_COMMAND_ADDRESS, requestCommand, 0);
  
  unsigned long startTime = millis();
  bool receivedAck = false;
  
  while (millis() - startTime < TIMEOUT) {
    if (IrReceiver.decode()) {
      uint32_t rawData = IrReceiver.decodedIRData.decodedRawData;
      uint16_t receivedAddress = IrReceiver.decodedIRData.address;
      uint16_t receivedCommand = IrReceiver.decodedIRData.command;
      
      if (rawData != 0 && receivedAddress == ACTUADOR_ADDRESS && 
          receivedCommand == CMD_ACK) {
        receivedAck = true;
        IrReceiver.resume();
        break;
      }
      IrReceiver.resume();
    }
  }
  
  if (!receivedAck) {
    return false;
  }
  
  delay(100);
  IrSender.sendNEC(CONTROLLER_DATA_ADDRESS, dataToSend, 0);
  return true;
}

bool requestActuatorStatus() {
  IrSender.sendNEC(CONTROLLER_COMMAND_ADDRESS, CMD_STATUS_ACT, 0);
  
  unsigned long startTime = millis();
  bool receivedStatus = false;
  
  while (millis() - startTime < TIMEOUT) {
    if (IrReceiver.decode()) {
      uint32_t rawData = IrReceiver.decodedIRData.decodedRawData;
      uint16_t receivedAddress = IrReceiver.decodedIRData.address;
      uint16_t receivedCommand = IrReceiver.decodedIRData.command;
      
      if (rawData != 0 && receivedAddress == ACTUADOR_ADDRESS) {
        relayStatus = (uint8_t)receivedCommand;
        receivedStatus = true;
        IrReceiver.resume();
        break;
      }
      IrReceiver.resume();
    }
  }
  
  return receivedStatus;
}

// ==== MANEJADORES DE ESTADOS ====
void handleUpdateTemperature() {
  currentState = STATE_REQUESTING_SENSOR_T;
  
  delay(100);
  bool success = requestSensorData(CMD_TEMPERATURE, &temperatureData);
  
  if (success) {
    hasTemperature = true;
    String valor = floatToValue(temperatureData, "T");
    addLog("Actualizar", "Temperatura", valor, "Éxito");
    Serial.print("Temperatura recibida: 0x");
    Serial.println(temperatureData, HEX);
  } else {
    addLog("Actualizar", "Temperatura", "N/A", "Error timeout");
    Serial.println("ERROR: No se recibió temperatura del sensor");
  }
  
  currentState = STATE_IDLE;
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

void handleUpdateHumidity() {
  currentState = STATE_REQUESTING_SENSOR_H;
  //server.send(200, "text/html", buildMainHTML());
  
  delay(100);
  bool success = requestSensorData(CMD_HUMIDITY, &humidityData);
  
  if (success) {
    hasHumidity = true;
    String valor = floatToValue(humidityData, "H");
    addLog("Actualizar", "Humedad", valor, "Éxito");
    Serial.print("Humedad recibida: 0x");
    Serial.println(humidityData, HEX);
  } else {
    addLog("Actualizar", "Humedad", "N/A", "Error timeout");
    Serial.println("ERROR: No se recibió humedad del sensor");
  }
  
  currentState = STATE_IDLE;
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

void handleUpdateLuminosity() {
  currentState = STATE_REQUESTING_SENSOR_L;
  
  delay(100);
  bool success = requestSensorData(CMD_LUMINOSITY, &luminosityData);
  
  if (success) {
    hasLuminosity = true;
    String valor = floatToValue(luminosityData, "L");
    addLog("Actualizar", "Luminosidad", valor, "Éxito");
    Serial.print("Luminosidad recibida: 0x");
    Serial.println(luminosityData, HEX);
  } else {
    addLog("Actualizar", "Luminosidad", "N/A", "Error timeout");
    Serial.println("ERROR: No se recibió luminosidad del sensor");
  }
  
  currentState = STATE_IDLE;
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

void handleSendTemperature() {
  currentState = STATE_SENDING_ACT_T;
  
  // Si no hay datos, pedir primero
  if (!hasTemperature) {
    Serial.println("No hay temperatura disponible, solicitando...");
    delay(100);
    bool success = requestSensorData(CMD_TEMPERATURE, &temperatureData);
    if (success) hasTemperature = true;
  }
  
  delay(100);
  bool success = sendDataToActuator(CMD_TEMPERATURE_ACT, temperatureData);
  
  if (success) {
    String valor = floatToValue(temperatureData, "T");
    addLog("Retransmitir", "Temperatura", valor, "Éxito");
    Serial.print("Temperatura enviada al actuador: 0x");
    Serial.println(temperatureData, HEX);
  } else {
    addLog("Retransmitir", "Temperatura", "N/A", "Error timeout");
    Serial.println("ERROR: No se recibió ACK del actuador");
  }
  
  currentState = STATE_IDLE;
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

void handleSendHumidity() {
  currentState = STATE_SENDING_ACT_H;
  
  // Si no hay datos, pedir primero
  if (!hasHumidity) {
    Serial.println("No hay humedad disponible, solicitando...");
    delay(100);
    bool success = requestSensorData(CMD_HUMIDITY, &humidityData);
    if (success) hasHumidity = true;
  }
  
  delay(100);
  bool success = sendDataToActuator(CMD_HUMIDITY_ACT, humidityData);
  
  if (success) {
    String valor = floatToValue(humidityData, "H");
    addLog("Retransmitir", "Humedad", valor, "Éxito");
    Serial.print("Humedad enviada al actuador: 0x");
    Serial.println(humidityData, HEX);
  } else {
    addLog("Retransmitir", "Humedad", "N/A", "Error timeout");
    Serial.println("ERROR: No se recibió ACK del actuador");
  }
  
  currentState = STATE_IDLE;
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

void handleSendLuminosity() {
  currentState = STATE_SENDING_ACT_L;
  
  // Si no hay datos, pedir primero
  if (!hasLuminosity) {
    Serial.println("No hay luminosidad disponible, solicitando...");
    delay(100);
    bool success = requestSensorData(CMD_LUMINOSITY, &luminosityData);
    if (success) hasLuminosity = true;
  }
  
  delay(100);
  bool success = sendDataToActuator(CMD_LUMINOSITY_ACT, luminosityData);
  
  if (success) {
    String valor = floatToValue(luminosityData, "L");
    addLog("Retransmitir", "Luminosidad", valor, "Éxito");
    Serial.print("Luminosidad enviada al actuador: 0x");
    Serial.println(luminosityData, HEX);
  } else {
    addLog("Retransmitir", "Luminosidad", "N/A", "Error timeout");
    Serial.println("ERROR: No se recibió ACK del actuador");
  }
  
  currentState = STATE_IDLE;
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

void handleRequestStatus() {
  currentState = STATE_REQUESTING_STATUS;
  
  delay(100);
  bool success = requestActuatorStatus();
  
  if (success) {
    String estado = (relayStatus == CMD_STATUS_ON) ? "Encendido" : 
                    (relayStatus == CMD_STATUS_OFF) ? "Apagado" : "Desconocido";
    addLog("Actualizar", "Estado Actuador", estado, "Éxito");
    Serial.print("Estado del relé: ");
    Serial.println(estado);
  } else {
    addLog("Actualizar", "Estado Actuador", "N/A", "Error timeout");
    Serial.println("ERROR: No se recibió estado del actuador");
  }
  
  currentState = STATE_IDLE;
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

// ==== INTERFAZ WEB ====
String buildMainHTML() {
  String html = R"(
  <html>
    <head>
      <meta charset='utf-8'>
      <title>Controller Web - Sistema IR</title>
      <style>
        body { font-family: Arial; background: #f2f2f2; text-align: center; }
        h1 { color: #004aad; }
        button { padding: 10px 20px; margin: 5px; font-size: 16px; cursor: pointer; }
        .section { background: white; border-radius: 10px; padding: 20px; margin: 20px auto; width: 500px; box-shadow: 0px 2px 5px rgba(0,0,0,0.1); }
        .status { font-weight: bold; }
        table { width: 100%; margin-top: 15px; border-collapse: collapse; }
        th, td { padding: 6px; border: 1px solid #999; }
        th { background: #004aad; color: white; }
        td { text-align: left; }
        .success { color: green; }
        .error { color: red; }
      </style>
    </head>
    <body>
      <h1>Sistema Controller Web - ESP32</h1>
  )";

  html += "<div class='section'><h2>Estado Actual</h2>";
  
  html += "<p>Temperatura: <span class='status'>";
  if (hasTemperature) {
    html += floatToValue(temperatureData, "T");
  } else {
    html += floatToValue(temperatureData, "T") + " (default)";
  }
  html += "</span></p>";
  
  html += "<p>Humedad: <span class='status'>";
  if (hasHumidity) {
    html += floatToValue(humidityData, "H");
  } else {
    html += floatToValue(humidityData, "H") + " (default)";
  }
  html += "</span></p>";
  
  html += "<p>Luminosidad: <span class='status'>";
  if (hasLuminosity) {
    html += floatToValue(luminosityData, "L");
  } else {
    html += floatToValue(luminosityData, "L") + " (default)";
  }
  html += "</span></p>";
  
  html += "<p>Estado Actuador: <span class='status'>";
  if (relayStatus == CMD_STATUS_ON) html += "Encendido";
  else if (relayStatus == CMD_STATUS_OFF) html += "Apagado";
  else html += "Desconocido";
  html += "</span></p></div>";

  html += "<div class='section'><h2>Consultar Sensor</h2>";
  html += "<form action='/update_temp'><button>Actualizar Temperatura</button></form>";
  html += "<form action='/update_hum'><button>Actualizar Humedad</button></form>";
  html += "<form action='/update_lux'><button>Actualizar Luminosidad</button></form></div>";

  html += "<div class='section'><h2>Enviar al Actuador</h2>";
  html += "<form action='/send_temp'><button>Enviar Temperatura</button></form>";
  html += "<form action='/send_hum'><button>Enviar Humedad</button></form>";
  html += "<form action='/send_lux'><button>Enviar Luminosidad</button></form></div>";

  html += "<div class='section'><h2>Control Actuador</h2>";
  html += "<form action='/status'><button>Consultar Estado</button></form></div>";

  html += "<div class='section'>" + buildLogHTML() + "</div>";

  html += "<p><small>TdP2 - Sistema IR con Web Server</small></p></body></html>";
  
  return html;
}

void handleRoot() {
  server.send(200, "text/html", buildMainHTML());
}

// ==== SETUP ====
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("Iniciando ControllerWeb...");
  
  // Inicializar IR
  IrSender.begin(SEND_PIN, DISABLE_LED_FEEDBACK);
  IrReceiver.begin(RECV_PIN, ENABLE_LED_FEEDBACK);
  Serial.println("IR inicializado");
  
  // Inicializar WiFi
  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  
  // Configurar rutas
  server.on("/", handleRoot);
  server.on("/update_temp", handleUpdateTemperature);
  server.on("/update_hum", handleUpdateHumidity);
  server.on("/update_lux", handleUpdateLuminosity);
  server.on("/send_temp", handleSendTemperature);
  server.on("/send_hum", handleSendHumidity);
  server.on("/send_lux", handleSendLuminosity);
  server.on("/status", handleRequestStatus);
  
  server.begin();
  Serial.println("Servidor web iniciado");
  
  // Log inicial
  addLog("Sistema", "Inicio", WiFi.localIP().toString(), "Éxito");
}

// ==== LOOP ====
void loop() {
  server.handleClient();
}