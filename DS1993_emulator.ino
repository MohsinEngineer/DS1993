#include "Arduino.h"
#include "LowLevel.h"
#include "OneWireSlave.h"
#include <OneWire.h>

OneWire  ds(2);

// This is the pin that will be used for one-wire data (depending on your arduino model, you are limited to a few choices, because some pins don't have complete interrupt support)
// On Arduino Uno, you can use pin 2 or pin 3
Pin oneWireData(2);

Pin led(13);

/* TODO: Replace this ROM with a correct sample of ROM code */
// This is the unique 64-bit Lasered ROM code of the ibutton
const byte owROM[8] = { 0xA1, 0x00, 0x00, 0x00, 0xFB, 0xC5, 0x2B, 0x32};

// This sample emulates a ibutton device, so we start by defining the available commands
const byte WRITE_SCRATCHPAD = 0x0F;
const byte READ_SCRATCHPAD = 0xAA;
const byte COPY_SCRATCHPAD = 0x55;
const byte READ_MEMORY = 0xF0;

// The memory page addresses
const word PAGE0 = 0x0000;
const word PAGE1 = 0x0020;
const word PAGE2 = 0x0040;
const word PAGE3 = 0x0060;
const word PAGE4 = 0x0080;
const word PAGE5 = 0x00A0;
const word PAGE6 = 0x00C0;
const word PAGE7 = 0x00E0;
const word PAGE8 = 0x0100;
const word PAGE9 = 0x0120;
const word PAGE10 = 0x0140;
const word PAGE11 = 0x0160;
const word PAGE12 = 0x0180;
const word PAGE13 = 0x01A0;
const word PAGE14 = 0x01C0;
const word PAGE15 = 0x01E0;

const uint8_t START_MASK = 0x1F;

enum DeviceState
{
  WaitingData,
  WaitingReset,
  WaitingCommand,
  WaitingAddr1,
  WaitingAddr2,
  WaitingTransmission
};

uint8_t data_recieved;

uint8_t Cursor = 0;
uint8_t Memory_Cursor = 0;
uint8_t Ending_offset = 0;

// Command Bytes
uint8_t TA1;
uint8_t TA2;
uint8_t ES_register = 0x40;

int OF = 0; // Overflow Flag
int PF = 0; // Partial Byte Flag
int AA = 0; // Authorization Flag

boolean partialWrite = false;

// Memory Map
byte scratchpad[32];
byte memory[16][32];


enum Command
{
  WritingScratchpad,
  ReadingScratchpad,
  CopyingScratchpad,
  ReadingMemory
};

volatile Command currentCommand = NULL;
volatile DeviceState state = WaitingReset;

// This function will be called each time the OneWire library has an event to notify (reset, error, byte received)
void owReceive(OneWireSlave::ReceiveEvent evt, byte data);

void setup() {
  Serial.begin(9600);
  led.outputMode();
  led.writeLow();

  Serial.println("Starting Slave");
  Serial.println();

  // Setup the OneWire library
  OWSlave.setReceiveCallback(&owReceive);
  OWSlave.begin(owROM, oneWireData.getPinNumber());

  sei();
}

void loop() {
  //delay(10);

  //cli();//disable interrupts
  // Be sure to not block interrupts for too long, OneWire timing is very tight for some operations. 1 or 2 microseconds (yes, microseconds, not milliseconds) can be too much depending on your master controller, but then it's equally unlikely that you block exactly at the moment where it matters.
  // This can be mitigated by using error checking and retry in your high-level communication protocol. A good thing to do anyway.
  //DeviceState localState = state;
  //sei();//enable interrupts

  if (state == WaitingTransmission) {
    switch (currentCommand) {
      case ReadingScratchpad:
        OWSlave.end();
        ds.write(TA1,0);
        ds.write(TA2,0);
        ds.write(ES_register,0);
        for (Cursor = TA1 & START_MASK; Cursor < 32; Cursor++) {
          ds.write(scratchpad[Cursor],0);
        }
        break;
      case ReadingMemory:
        OWSlave.end();
        for (int i = 0; i < 32; i++)
        {
          ds.write(memory[Cursor][i],0);
        }
        for (int i = 0; i < 32; i++)
        {
          Serial.print("Data" + String(i) + ": ");
          Serial.println(memory[Cursor][i], HEX);
        }
        break;
    }
    cli();
    OWSlave.begin(owROM, oneWireData.getPinNumber());
    OWSlave.beginWriteBit(1, true); // send ones as long as master reset does not occur
    state = WaitingReset;
    currentCommand = NULL;
    sei();
  }
}

void owReceive(OneWireSlave::ReceiveEvent evt, byte data)
{
  switch (evt)
  {
    case OneWireSlave::RE_Byte:
      switch (state)
      {
        case WaitingCommand:
          switch (data)
          {
            case WRITE_SCRATCHPAD:
              state = WaitingAddr1;
              currentCommand = WritingScratchpad;
              break;

            case READ_SCRATCHPAD:
              state = WaitingData;
              currentCommand = ReadingScratchpad;
              break;

            case COPY_SCRATCHPAD:
              state = WaitingAddr1;
              currentCommand = CopyingScratchpad;
              break;
            case READ_MEMORY:
              state = WaitingAddr1;
              currentCommand = ReadingMemory;
              break;
          }
          break;

        case WaitingAddr1:
          TA1 = (uint8_t) data;
          state = WaitingAddr2;
          break;

        case WaitingAddr2:
          state = WaitingData;
          TA2 = (uint8_t) data;
          Cursor = TA1 & START_MASK;
          AA = 0;
          PF = 0;
          OF = 0;
          break;

        case WaitingData:
          switch (currentCommand) {
            case WritingScratchpad:
              if (Cursor == (uint8_t) 32)
              {
                OF = 1;
              }
              else
              {
                partialWrite = true;
                scratchpad[Cursor] = data;
                Ending_offset = Cursor;
                Cursor++;
              }
              break;

            case ReadingScratchpad:
              ES_register = (uint8_t) (AA * 256) + (OF * 128) + (PF * 64) + (0 * 32) + (0 * 16) + (0 * 8) + (0 * 4) + (0 * 2) + (0 * 1);
              state = WaitingTransmission;
              break;

            case CopyingScratchpad:
              if ( data == ES_register )
              {
                AA = 1;
                for (int i = 0; i < 32; i++) {
                  memory[Memory_Cursor][i] = scratchpad[i];
                }
                Memory_Cursor++;
                if (Memory_Cursor == 16)
                  Memory_Cursor = 0;
                OWSlave.beginWriteBit(0, true); // send zeros as long as master reset does not occur
              }
              state = WaitingReset;
              currentCommand = NULL;
              break;

            case ReadingMemory:
              word ADDRESS = word(TA2, TA1);

              //Serial.print("Address: ");
              //Serial.println(ADDRESS, HEX);

              switch (ADDRESS)
              {
                case PAGE0: Cursor = 0;
                  break;
                case PAGE1: Cursor = 1;
                  break;
                case PAGE2: Cursor = 2;
                  break;
                case PAGE3: Cursor = 3;
                  break;
                case PAGE4: Cursor = 4;
                  break;
                case PAGE5: Cursor = 5;
                  break;
                case PAGE6: Cursor = 6;
                  break;
                case PAGE7: Cursor = 7;
                  break;
                case PAGE8: Cursor = 8;
                  break;
                case PAGE9: Cursor = 9;
                  break;
                case PAGE10: Cursor = 10;
                  break;
                case PAGE11: Cursor = 11;
                  break;
                case PAGE12: Cursor = 12;
                  break;
                case PAGE13: Cursor = 13;
                  break;
                case PAGE14: Cursor = 14;
                  break;
                case PAGE15: Cursor = 15;
                  break;
              }

              //Serial.print("Cursor: ");
              //Serial.print(Cursor, DEC);

              state = WaitingTransmission;

              break;
          }
          break;
      }
      break;

    case OneWireSlave::RE_Reset:
      if ( currentCommand == WritingScratchpad && partialWrite == true) {
        PF = 1;
        partialWrite = false;
      }
      state = WaitingCommand;
      currentCommand = NULL;
      break;

    case OneWireSlave::RE_Error:
      state = WaitingReset;
      break;
  }
}

