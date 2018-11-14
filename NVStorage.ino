#include <EEPROM.h>
void ReadLineInfo(LINEINFO line, int id)
{
  EEPROM.get(id*sizeof(LINEINFO) , line);
  
}


void UpdateLineInfo(LINEINFO line, int id)
{
  EEPROM.put(id * sizeof(LINEINFO),line);
  
}
