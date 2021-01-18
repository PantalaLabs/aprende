#include <EEPROM.h>

//load trigger/gate quantity
void loadEEpromTriggerQuantity()
{
  int addr = 0; //must be an INT data type
  maxTriggerChannels = EEPROM.read(addr);
}

//load trigger/gate quantity and all bank patterns
void loadEEpromBanks()
{
  int addr = 1; //must be an INT data type
  for (uint8_t k = 0; k < 2; k++)
  {
    for (uint8_t i = 0; i < MAXCHANNELS; i++)
    {
      for (uint8_t j = 0; j < MAXSTEPS; j++)
      {
        triggerTable[k][i][j] = EEPROM.read(addr);
        addr++;
      }
    }
  }
}

//clear all eeprom
void clearEEprom()
{
  for (int i = 0; i < EEPROM.length(); i++)
  {
    EEPROM.write(i, 0);
  }
}

//save trigger/gate quantity
void saveEEpromTriggerQuantity()
{
  int addr = 0; //must be an INT data type
  EEPROM.write(addr, maxTriggerChannels);
}

//save trigger/gate quantity and all bank patterns
void saveEEpromBanks()
{
  int addr = 1; //must be an INT data type
  for (uint8_t k = 0; k < 2; k++)
  {
    for (uint8_t i = 0; i < MAXCHANNELS; i++)
    {
      for (uint8_t j = 0; j < MAXSTEPS; j++)
      {
        EEPROM.write(addr, triggerTable[k][i][j]);
        addr++;
      }
    }
  }
}
