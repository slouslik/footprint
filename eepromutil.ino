
#include <EEPROM.h>

byte COUNTEREEPROMSLOTS = 10;
unsigned long COUNTERADDRBASE = 8; // address in EEPROM that points to the first possible slot for a counter
unsigned long COUNTERADDR = 0;     // address in EEPROM that points to the latest Counter in EEPROM

unsigned long EEPROM_Read_Counter()
{
    return EEPROM_Read_ULong(EEPROM_Read_ULong(COUNTERADDR));
}

void EEPROM_Write_Counter(unsigned long counterNow)
{
    if (counterNow == EEPROM_Read_Counter()) {
        Serial.println("{EEPROM-SKIP(no changes)}");
        return; //skip if nothing changed
    }
    
    Serial.print("{EEPROM-SAVE(");
    Serial.print(EEPROM_Read_ULong(COUNTERADDR));
    Serial.print(")=");
    // DEBUG(PulseCounter);
    Serial.println("}");
      
    unsigned long CounterAddr = EEPROM_Read_ULong(COUNTERADDR);
    if (CounterAddr == COUNTERADDRBASE+8*(COUNTEREEPROMSLOTS-1))
        CounterAddr = COUNTERADDRBASE;
    else CounterAddr += 8;
    
    EEPROM_Write_ULong(CounterAddr, counterNow);
    EEPROM_Write_ULong(COUNTERADDR, CounterAddr);
}

unsigned long EEPROM_Read_ULong(int address)
{
    unsigned long temp;
    for (byte i=0; i<8; i++)
        temp = (temp << 8) + EEPROM.read(address++);
    return temp;
}

void EEPROM_Write_ULong(int address, unsigned long data)
{
    for (byte i=0; i<8; i++) {
        EEPROM.write(address+7-i, data);
        data = data >> 8;
    }
}

