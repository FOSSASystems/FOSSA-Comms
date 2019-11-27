// include the libraries
#include <FOSSA-Comms.h>
#include <RadioLib.h>

// modulation properties to be used in transmission
#define BANDWIDTH               7   // 125 kHz - see array below
#define SPREADING_FACTOR        12  // SF 12
#define CODING_RATE             7   // CR 4/7
#define PREAMBLE_LENGTH         32  // symbols
#define CRC_ENABLED             1
#define OUTPUT_POWER            10  // dBm

// array of allowed bandwidth values in kHz
float bws[] = {7.8, 10.4, 15.6, 20.8, 31.25, 41.7, 62.5, 125.0};

// SX1262 has the following connections:
// NSS pin:   10
// DIO1 pin:  2
// BUSY pin:  9
SX1262 radio = new Module(10, 2, 9);

// satellite callsign
char callsign[] = "FOSSASAT-1";

// last transmission timestamp
uint32_t lastTransmit = 0;

// transmission period in ms
const uint32_t transmitPeriod = 6000;

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
  int state = radio.begin(436.7, 125.0, 11, 8, 0x0F0F, 21, 120, 16);
  if (state == ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true);
  }

  // set interrupt service routine
  radio.setDio1Action(setFlag);

  // start listening
  radio.startReceive();
}

void loop() {
  // check if it's time to transmit
  if(millis() - lastTransmit >= transmitPeriod) {
    // disable reception interrupt
    enableInterrupt = false;
    detachInterrupt(digitalPinToInterrupt(2));

    // save timestamp
    lastTransmit = millis();

    Serial.println(F("Transmitting packet ... "));

    // data to transmit
    uint8_t functionId = CMD_RETRANSMIT_CUSTOM;
    char message[] = "I'm a message!";
    uint8_t optDataLen = 7 + strlen(message);
    uint8_t* optData = new uint8_t[optDataLen];
    optData[0] = BANDWIDTH;
    optData[1] = SPREADING_FACTOR;
    optData[2] = CODING_RATE;
    optData[3] = (uint8_t)(PREAMBLE_LENGTH & 0xFF);
    optData[4] = (uint8_t)((PREAMBLE_LENGTH >> 8) & 0xFF);
    optData[5] = CRC_ENABLED;
    optData[6] = (uint8_t)OUTPUT_POWER;
    memcpy(optData + 7, message, strlen(message));

    // build frame
    uint8_t len = FCP_Get_Frame_Length(callsign, optDataLen);
    uint8_t* frame = new uint8_t[len];
    FCP_Encode(frame, callsign, functionId, optDataLen, optData);
    delete[] optData;
    PRINT_BUFF(frame, len);

    // send data with default configuration
    radio.begin(436.7, 125.0, 11, 8, 0x0F0F, 21, 120, 16);
    radio.setCRC(true);
    int state = radio.transmit(frame, len);
    delete[] frame;

    // check transmission success
    if (state == ERR_NONE) {
      Serial.println(F("Success!"));
    }

    // set radio mode to reception with custom configuration
    Serial.println(F("Waiting for response ... "));
    radio.begin(436.7, bws[BANDWIDTH], SPREADING_FACTOR, CODING_RATE, 0x0F0F, OUTPUT_POWER, 120, PREAMBLE_LENGTH);
    radio.setCRC(CRC_ENABLED);
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

      if(functionId == RESP_REPEATED_MESSAGE_CUSTOM) {
        Serial.println(F("Got response with custom configuration!"));

        // check optional data
        uint8_t respOptDataLen = FCP_Get_OptData_Length(callsign, respFrame, respLen);
        if(respOptDataLen > 0) {
          // frame contains optional data
          uint8_t* respOptData = new uint8_t[respOptDataLen];
          FCP_Get_OptData(callsign, respFrame, respLen, respOptData);

          // print optional data
          Serial.print(F("Optional data ("));
          Serial.print(respOptDataLen);
          Serial.println(F(" bytes):"));
          PRINT_BUFF(respOptData, respOptDataLen);
          delete[] respOptData;
        }
      }

    } else {
      Serial.println(F("Reception failed, code "));
      Serial.println(state);

    }

    // enable reception interrupt
    delete[] respFrame;
    enableInterrupt = true;
  }
}
