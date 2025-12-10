#define SEND_NEC
#include <IRremote.hpp>

const uint8_t RECV_PIN = 2;      // Pin para recibir IR
const uint8_t SEND_PIN = 3;      // Pin para enviar IR de respuesta
const uint8_t RELAY_PIN = 7;     // Pin del relé

/************ 
* ADDRESSES
************/
const uint16_t CONTROLLER_COMMAND_ADDRESS = 0x10;
const uint16_t CONTROLLER_DATA_ADDRESS = 0x20;
const uint16_t ACTUADOR_ADDRESS = 0x30;

/********************
* COMMANDS REQUESTS
********************/
const uint16_t CMD_TEMPERATURE = 0x30;
const uint16_t CMD_HUMIDITY = 0x31;
const uint16_t CMD_LUMINOSITY = 0x32;
const uint16_t CMD_STATUS = 0x33;

/*********************
* COMMANDS RESPONSES
*********************/
const uint16_t CMD_ACK = 0xFF;
const uint16_t CMD_STATUS_ON = 0x80;
const uint16_t CMD_STATUS_OFF = 0x90;

/*************
* THRESHOLDS
*************/
const uint8_t TEMP_THRESHOLD = 128;   // 128 = 25°C
const uint8_t HUM_THRESHOLD = 128;    // 128 = 50%
const uint8_t LUM_THRESHOLD = 128;    // 128 = mitad de luminosidad

// Estado del relé (actualizado cada vez que cambia)
bool relayState = false;

/************* 
* TIMEOUT
*************/
const unsigned long TIMEOUT = 5000; // 5 segundos

void setup() {
  Serial.begin(115200);
  IrReceiver.begin(RECV_PIN, ENABLE_LED_FEEDBACK);
  IrSender.begin(SEND_PIN, DISABLE_LED_FEEDBACK);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  Serial.println("Actuador IR listo - Esperando comandos del controller");
}

void loop() {
  if (IrReceiver.decode()) {
    uint32_t rawData = IrReceiver.decodedIRData.decodedRawData;
    uint16_t receivedAddress = IrReceiver.decodedIRData.address;
    uint16_t receivedCommand = IrReceiver.decodedIRData.command;

    if (rawData != 0) {
      Serial.print("Protocol: "); Serial.println(IrReceiver.decodedIRData.protocol);
      Serial.print("Address: 0x"); Serial.print(receivedAddress, HEX);
      Serial.print(", Command: 0x"); Serial.println(receivedCommand, HEX);
    }

    // Procesar comandos (dirección 0x00)
    if (rawData != 0 && receivedAddress == CONTROLLER_COMMAND_ADDRESS) {
      bool expectingData = false;
      uint8_t dataType = 0;

      switch (receivedCommand) {
        case CMD_TEMPERATURE: { // SEND_T REQ
          Serial.println("Recibido SEND_T REQ - Esperando dato de temperatura");
          expectingData = true;
          dataType = 'T';
          break;
        }
        case CMD_HUMIDITY: { // SEND_H REQ
          Serial.println("Recibido SEND_H REQ - Esperando dato de humedad");
          expectingData = true;
          dataType = 'H';
          break;
        }
        case CMD_LUMINOSITY: { // SEND_L REQ
          Serial.println("Recibido SEND_L REQ - Esperando dato de luminosidad");
          expectingData = true;
          dataType = 'L';
          break;
        }
        case CMD_STATUS: { // STATUS REQ
          Serial.println("Recibido STATUS REQ - Enviando estado del relé");
          handleStatusRequest();
          // IrReceiver.resume();
          break;
        }
        default:
          Serial.println("Comando desconocido");
      }

      if (expectingData) {
        // Enviar ACK
        Serial.println("Enviando SEND_ACK RES (CMD_ACK)");
        IrSender.sendNEC(ACTUADOR_ADDRESS, CMD_ACK, 0);

        // Esperar el dato con dirección CONTROLLER_DATA_ADDRESS (0x20)
        unsigned long startTime = millis();
        bool receivedData = false;

        while (millis() - startTime < TIMEOUT ) {
          if (IrReceiver.decode()) {
            uint32_t dataRawData = IrReceiver.decodedIRData.decodedRawData;
            uint16_t dataAddress = IrReceiver.decodedIRData.address;
            uint8_t dataValue = (uint8_t)IrReceiver.decodedIRData.command;

            if (dataRawData != 0 && dataAddress == CONTROLLER_DATA_ADDRESS) {
              Serial.print("Dato recibido: 0x"); Serial.println(dataValue, HEX);
              receivedData = true;
              controlRelay(dataType, dataValue);
              // IrReceiver.resume();
              break;
            }
            IrReceiver.resume();
          }
        }

        if (!receivedData) {
          Serial.println("ERROR: No se recibió el dato en 5 segundos");
        }
      }
    }

    IrReceiver.resume();
  }
}

void controlRelay(uint8_t dataType, uint8_t value) {
  bool shouldActivate = false;

  switch (dataType) {
    case 'T': // Temperatura
      shouldActivate = (value >= TEMP_THRESHOLD);
      Serial.print("Controlando relé con TEMPERATURA: ");
      Serial.print(value);
      Serial.print(" (threshold: ");
      Serial.print(TEMP_THRESHOLD);
      Serial.println(")");
      break;

    case 'H': // Humedad
      shouldActivate = (value >= HUM_THRESHOLD);
      Serial.print("Controlando relé con HUMEDAD: ");
      Serial.print(value);
      Serial.print(" (threshold: ");
      Serial.print(HUM_THRESHOLD);
      Serial.println(")");
      break;

    case 'L': // Luminosidad
      shouldActivate = (value >= LUM_THRESHOLD);
      Serial.print("Controlando relé con LUMINOSIDAD: ");
      Serial.print(value);
      Serial.print(" (threshold: ");
      Serial.print(LUM_THRESHOLD);
      Serial.println(")");
      break;

    default:
      Serial.println("Tipo de dato desconocido");
      return;
  }

  if (shouldActivate) {
    digitalWrite(RELAY_PIN, HIGH);
    relayState = true;
    Serial.println("Relé ACTIVADO");
  } else {
    digitalWrite(RELAY_PIN, LOW);
    relayState = false;
    Serial.println("Relé DESACTIVADO");
  }
}

void handleStatusRequest() {
  // Enviar estado del relé (0x00 = apagado, 0xFF = encendido)
  uint8_t statusValue = relayState ? CMD_STATUS_ON : CMD_STATUS_OFF;
  Serial.print("Enviando estado del relé: ");
  Serial.println(relayState ? "ENCENDIDO" : "APAGADO");
  IrSender.sendNEC(ACTUADOR_ADDRESS, statusValue, 0);
}
