// include the libraries
#include <AESLib.h>
#include <FOSSA-Comms.h>
#include <RadioLib.h>

// SX1262 has the following connections:
// NSS pin:   10
// DIO1 pin:  2
// DIO2 pin:  3
// BUSY pin:  9
SX1262 radio = new Module(10, 2, 3, 9);

// satellite callsign
char callsign[] = "FOSSASAT-1";

// transmission password
const char* password = "password";

// encryption key
const uint8_t encryptionKey[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};

// last transmission timestamp
uint32_t lastTransmit = 0;

// transmission period in ms
const uint32_t transmitPeriod = 4000;

// interrupt flags
volatile bool receivedFlag = false;
volatile bool enableInterrupt = true;

// interrupt service routine for data reception
void setFlag(void) {
  if(!enableInterrupt) {
    return;
  }

  receivedFlag = true;
}

void setup() {
  Serial.begin(9600);

  // initialize SX1262
  Serial.print(F("Initializing ... "));
  int state = radio.begin(434.0, 125.0, 11, 8, 0x0F0F);
  if (state == ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true);
  }

  /*
    // set TCXO reference voltage
    Serial.print(F("Setting TCXO reference ... "));
    state = radio.setTCXO(1.6);
    if (state == ERR_NONE) {
      Serial.println(F("success!"));
    } else {
      Serial.print(F("failed, code "));
      Serial.println(state);
      while (true);
    }
  */

  // set interrupt service routine
  radio.setDio1Action(setFlag);

  // start listening
  radio.startReceive();

  // provide seed for PRNG
  randomSeed(analogRead(A6));
}

void loop() {
  if(millis() - lastTransmit >= transmitPeriod) {
    // disable reception interrupt
    enableInterrupt = false;
    detachInterrupt(digitalPinToInterrupt(2));

    // save timestamp
    lastTransmit = millis();

    Serial.println(F("Transmitting packet ... "));

    // data to transmit
    uint8_t functionId = CMD_SET_CALLSIGN;
    char optData[] = "FOSSASAT-1";
    uint8_t optDataLen = strlen(optData);

    // build frame
    uint8_t len = FCP_Get_Frame_Length(callsign, optDataLen, password);
    uint8_t* frame = new uint8_t[len];
    FCP_Encode(frame, callsign, functionId, optDataLen, (uint8_t*)optData, encryptionKey, password);
    PRINT_BUFF(frame, len);

    // send data
    int state = radio.transmit(frame, len);
    delete[] frame;

    // check transmission success
    if (state == ERR_NONE) {
      Serial.println(F("Success!"));
    }

    // set radio mode to reception
    Serial.println(F("Waiting for response ... "));
    radio.setDio1Action(setFlag);
    radio.startReceive();
    enableInterrupt = true;
  }

  // check if new data were received
  if(receivedFlag) {
    // disable reception interrupt
    enableInterrupt = false;
    receivedFlag = false;

    // read received data
    size_t respLen = radio.getPacketLength();
    uint8_t* respFrame = new uint8_t[respLen];
    int state = radio.readData(respFrame, respLen);

    // check reception success
    if (state == ERR_NONE) {
      // print raw data
      Serial.print(F("Received "));
      Serial.print(respLen);
      Serial.println(F(" bytes:"));
      PRINT_BUFF(respFrame, respLen);

      // get function ID
      uint8_t functionId = FCP_Get_FunctionID(callsign, respFrame, respLen);
      Serial.print(F("Function ID: 0x"));
      Serial.println(functionId, HEX);

      // check optional data
      uint8_t respOptDataLen = FCP_Get_OptData_Length(callsign, respFrame, respLen, encryptionKey, password);
      if(respOptDataLen > 0) {
        // frame contains optional data
        uint8_t* respOptData = new uint8_t[respOptDataLen];
        FCP_Get_OptData(callsign, respFrame, respLen, respOptData, encryptionKey, password);

        // print optional data
        Serial.print(F("Optional data ("));
        Serial.print(respOptDataLen);
        Serial.println(F(" bytes):"));
        PRINT_BUFF(respOptData, respOptDataLen);
        delete[] respOptData;
      }
   
    } else {
      Serial.print(F("Reception failed, code "));
      Serial.println(state);

    }

    // enable reception interrupt
    delete[] respFrame;
    enableInterrupt = true;
  }
}
