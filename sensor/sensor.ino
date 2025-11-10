#define SEND_NEC
#include <IRremote.hpp>
#include <DHT.h>

const uint8_t RECV_PIN = 2;      // Pin para recibir IR
const uint8_t SEND_PIN = 3;      // Pin para enviar IR de respuesta
const uint16_t CONTROLLER_ADDRESS = 0x10;
const uint16_t SENSOR_ADDRESS = 0x40;

/********************
* COMMANDS REQUESTS
********************/
const uint16_t CMD_TEMPERATURE = 0x20;
const uint16_t CMD_HUMIDITY    = 0x21;
const uint16_t CMD_LUMINOSITY  = 0x22;

#define DHTPIN 7
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(115200);
  IrReceiver.begin(RECV_PIN, ENABLE_LED_FEEDBACK);
  IrSender.begin(SEND_PIN, DISABLE_LED_FEEDBACK);
  dht.begin();
  Serial.println("Receptor IR listo (responde a NEC 0x10/0x20-0x22)");
}

void loop() {
  if (IrReceiver.decode()) {
    uint32_t rawData = IrReceiver.decodedIRData.decodedRawData;
    uint16_t receivedCommand = IrReceiver.decodedIRData.command;

    if (rawData != 0) {
      Serial.print("Protocol: "); Serial.println(IrReceiver.decodedIRData.protocol);
      Serial.print("Address: 0x"); Serial.println(IrReceiver.decodedIRData.address, HEX);
      Serial.print("Command: 0x"); Serial.println(receivedCommand, HEX);
    }

    if (rawData != 0 && IrReceiver.decodedIRData.address == CONTROLLER_ADDRESS) {
      uint16_t responseCommand = 0;

      switch (receivedCommand) {
        case CMD_TEMPERATURE: { // TEMPERATURA
          float temp = dht.readTemperature(); // °C
          if (isnan(temp)) { Serial.println("Error leyendo temperatura"); break; }
          responseCommand = (uint8_t)((temp / 50.0) * 255.0); // mapear 0-50°C a 0-255
          Serial.print("Recibido TEMPERATURA. Temp: "); Serial.print(temp);
          Serial.print("°C -> Enviando 8bit: "); Serial.println(responseCommand, HEX);
          break;
        }

        case CMD_HUMIDITY: { // HUMEDAD
          float hum = dht.readHumidity(); // %
          if (isnan(hum)) { Serial.println("Error leyendo humedad"); break; }
          responseCommand = (uint8_t)((hum / 100.0) * 255.0); // mapear 0-100% a 0-255
          Serial.print("Recibido HUMEDAD. Hum: "); Serial.print(hum);
          Serial.print("% -> Enviando 8bit: "); Serial.println(responseCommand, HEX);
          break;
        }

        case CMD_LUMINOSITY: { // LUMINOSIDAD
          int analogValue = analogRead(A0); // 0-1023
          responseCommand = map(analogValue, 0, 1023, 0, 255); // convertir a 8 bits
          Serial.print("Recibido LUMINOSIDAD. Analog A0: "); Serial.print(analogValue);
          Serial.print(" -> Enviando 8bit: "); Serial.println(responseCommand, HEX);
          break;
        }

        default: 
          Serial.println("Comando desconocido, ignorando"); 
          responseCommand = 0;
      }
      delay(100);
      if (responseCommand != 0) {
        IrSender.sendNEC(SENSOR_ADDRESS, responseCommand, 0);
      }
    }

    IrReceiver.resume(); // Listo para próxima señal
  }
}