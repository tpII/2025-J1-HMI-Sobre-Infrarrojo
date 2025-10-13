#define SEND_NEC
#include <IRremote.hpp>

const uint8_t SEND_PIN = 3;      // Pin del LED IR para enviar
const uint8_t RECV_PIN = 2;      // Pin para recibir IR
const uint16_t NEC_ADDRESS = 0x10;
const unsigned long TIMEOUT = 5000; // 5 segundos

void setup() {
  Serial.begin(115200);
  IrSender.begin(SEND_PIN, DISABLE_LED_FEEDBACK);
  IrReceiver.begin(RECV_PIN, ENABLE_LED_FEEDBACK);
  Serial.println("Emisor IR listo (NEC) con recepción de respuesta");
  Serial.println("Ingrese: T=Temperatura, H=Humedad, L=Luminosidad");
}

void loop() {
  if (Serial.available() > 0) {
    char c = Serial.read();
    uint16_t command = 0;

    switch (c) {
      case 'T':{
        command = 0x20; Serial.println("Enviando comando TEMPERATURA...");
        break;
      } 
      case 'H':{
        command = 0x21; Serial.println("Enviando comando HUMEDAD...");
        break;
      }
      case 'L':{
          command = 0x22; Serial.println("Enviando comando LUMINOSIDAD...");
          break;
      }
      default: 
        // Serial.println("Comando inválido. Use T, H o L."); 
        return;
    }

    IrSender.sendNEC(NEC_ADDRESS, command, 0);

    unsigned long startTime = millis();
    bool receivedResponse = false;

    while (millis() - startTime < TIMEOUT) {
      if (IrReceiver.decode()) {
        uint32_t rawData = IrReceiver.decodedIRData.decodedRawData;
        uint16_t receivedAddress = IrReceiver.decodedIRData.address;
        uint16_t receivedCommand = IrReceiver.decodedIRData.command;

        if (rawData != 0 && receivedAddress == 0x02) {
          Serial.print("Se recibió IR válido: ");
          Serial.print("Protocolo: "); Serial.print(IrReceiver.decodedIRData.protocol);
          Serial.print(", Dirección: 0x"); Serial.print(receivedAddress, HEX);
          Serial.print(", Comando: 0x"); Serial.println(receivedCommand, HEX);
          receivedResponse = true;
          IrReceiver.resume();
          break;
        }

        IrReceiver.resume();
      }
    }

    if (!receivedResponse) {
      Serial.println("ERROR: No se recibió respuesta IR en 5 segundos");
    }
  }
}
