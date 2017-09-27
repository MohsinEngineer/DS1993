#include <LiquidCrystal.h>
#include <OneWire.h>

OneWire  ds(2);
uint8_t addr[8];
String keyStatus = "";

// This sample emulates a ibutton device, so we start by defining the available commands
const uint8_t WRITE_SCRATCHPAD = 0x0F;
const uint8_t READ_SCRATCHPAD = 0xAA;
const uint8_t COPY_SCRATCHPAD = 0x55;
const uint8_t READ_MEMORY = 0xF0;

uint8_t data[32];

const uint8_t ADDR1 = 0x26;
const uint8_t ADDR2 = 0x12;
const uint8_t DUMMY_DATA[2] = {0x3E, 0x1D};

void setup(void) {
  Serial.begin(9600);

  Serial.println("Starting Master");
  Serial.println();
  delay(1000);

  if ( !ds.search(addr)) {
    Serial.println("No more addresses.");
    Serial.println();
    ds.reset_search();
    delay(250);
    return;
  }

  if ( OneWire::crc8( addr, 7) != addr[7]) {
    Serial.print("CRC is not valid!\n");
    return;
  }

  Serial.print("ROM =");
  for ( int i = 0; i < 8; i++) {
    Serial.write(' ');
    Serial.print(addr[i], HEX);
  }
  Serial.println();

  ds.reset();
  ds.skip();


  Serial.println();
  
  // Writing the dummy data 1 & 2 at index 6 of scratchpad

  ds.write(WRITE_SCRATCHPAD, 1);
  ds.write(ADDR1);
  Serial.print("Write TA1: ");
  Serial.println(ADDR1, HEX);
  ds.write(ADDR2);
  Serial.print("Write TA2: ");
  Serial.println(ADDR2, HEX);
  ds.write(DUMMY_DATA[0]);
  Serial.print("Write DATA: ");
  Serial.println(DUMMY_DATA[0], HEX);
  ds.write(DUMMY_DATA[1]);
  Serial.print("Write DATA: ");
  Serial.println(DUMMY_DATA[1], HEX);
  ds.depower();

  ds.reset();
  Serial.println();
  Serial.println("Scratchpad Written");
  Serial.println();

  /*
    ds.skip();
    
    // Reading the scratchpad
    
    ds.write(READ_SCRATCHPAD,1);

    for (int i = 0; i < 10; i++) {
      data[i] = ds.read();
      Serial.print("Read: ");
      Serial.println(data[i], HEX);
    }
    ds.depower();
    ds.reset();

    Serial.println();
    Serial.println("Scratchpad Read\n");
  */

  ds.skip();
  
  // Copying the scratchpad to memory
  
  ds.write(COPY_SCRATCHPAD, 1);
  ds.write(ADDR1);
  ds.write(ADDR2);
  ds.write(0x40);
  delay(10);

  ds.depower();
  ds.reset();

  ds.skip();
  
  // Reading Memory
  
  ds.write(READ_MEMORY, 1);

  ds.write(0x00);  // Transmitting a dummy TA1
  ds.write(0x00);  // Transmitting a dummy TA2
  
  // reading the required data
  for (int i = 0; i < 32; i++) {
    data[i] = ds.read();
  }
  
  delay(1000);

  // printing the read data to serial monitor
  for (int i = 0; i < 32; i++) {
    Serial.print("data" + String(i) + ": ");
    Serial.println(data[i], HEX);
  }

  ds.depower();
  ds.reset();

  Serial.println();
  Serial.println("Memory Read\n");
}


void loop(void) {
}
