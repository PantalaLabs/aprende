boolean debug = false;

/*Aprende is an 8 track , hand tap step sequencer..
  The module provides:

  -two banks of 8 channels x 32 steps grid
  -long life micro switch button to tapping
  -grid forward shift rotation
  -sequence fill
  -400 undos
  -load/save grid
  -erase sequence
  -ghost tap
  -8 selectable trigger / gate outs with led indicator and mute switch
  -internal / external clock
  -bpm selector
  -channel selector

  This project is ispired on acproject called BIG BUTTON by LOOK MUM NO COMPUTER
  https://www.lookmumnocomputer.com/big-button/

  Creative Commons License CC-BY-SA
  Aprende by Gibran Curtiss Salomão/Pantala Labs is licensed
  under a Creative Commons Attribution-ShareAlike 4.0 International License.
*/

#include <Counter.h>
#include <DigitalInput.h>
#include <AnalogInput.h>
#include <EventDebounce.h>
#include <MicroDebounce.h>
#include <Trigger.h>

//limits
#define MAXSTEPS 32      //max grid steps
#define MAXCHANNELS 8    //max  channels - 8 in this board
#define DEFCLOCKLENGHT 5 //default clock lenght

//analogic pins
#define CHANNELSELPIN 0 //channel selector pot
#define BPMPIN 1        //bpm pot

//digital pins
#define CLOCKINPIN 2   //clock in for external triggers
#define RESETPIN 3     //step counter reset
#define SHIFTPIN 4     //shift / rotate especific grid
#define LOADSAVEPIN 5  //load / save all grid
#define ERASEPIN 6     //erase one especific channel
#define FILLPIN 7      //fill last channel sequence
#define GHOSTPIN 8     //ghost trigger - doesnt save the trigger in the grid
#define UNDOPIN 9      //yeahhhhhhhh undo grid modifications
#define LEARNPIN 10    //APRENDE
#define BANKPIN 11     //changes between two main grids
#define CLOCKSRCPIN 50 //select external or internal clock source

Trigger CLKOUT1(34);
Trigger CLKOUT2(44);
Trigger CLKOUT3(32);
Trigger CLKOUT4(40);
Trigger CLKOUT5(38);
Trigger CLKOUT6(46);
Trigger CLKOUT7(42);
Trigger CLKOUT8(36);
Trigger *triggerArray[8] = {&CLKOUT1, &CLKOUT2, &CLKOUT3, &CLKOUT4, &CLKOUT5, &CLKOUT6, &CLKOUT7, &CLKOUT8};

#define EOGPIN 48 //end of grid indicator
Trigger eog(EOGPIN);

#define DEBOUNCETIME 200 //interface debounve for buttons

#define MAXGIVEUPTIME 5000

uint32_t lastClockTick;      //last time step changed
boolean pleaseCycle = false; //flags to request a new cycle

EventDebounce triggerInInterval(30);
MicroDebounce internalTickTack(500000); //main internal tick timer

Counter thisStep(MAXSTEPS - 1); //step counter
boolean internalClock = true;   //flags int/ext clock
#define INIBPMVALUE 120         //initial bpm
uint16_t bpm = INIBPMVALUE;     //time interval in ms between each interruption
#define MINBPMVALUE 10          //min bpm

uint16_t maxTriggerChannels = 6; //default max tiggers channels

//triggers related
#define CLOSETRIGGER 0
#define OPENTRIGGER 1
#define GHOSTTRIGGER 2
uint16_t triggerTable[2][MAXCHANNELS][MAXSTEPS]; //triggers flags positions
uint16_t bankChoice[MAXCHANNELS];                //chose between trigger bank A or B (possible values 0 or 1)
uint32_t calculatedTriggerInterval = 0;          //last time when trigger was lit

//undos
#define MAXUNDOS 400
uint16_t undo[5][MAXUNDOS]; //[channel, grid step, last value, commit flag, bank]
//channel - 0 to 7 channel pointer
//grid step - 0 to 31 = step pointer
//last value - 0-not trigger / 1=trigger
//commit flag - 0=middle transation / 1=EOT
//bank - 0=bankA / 1=bankB
int16_t lastUndoPtr = -1;   //points to last saved undo
int16_t rollbackBlock = -2; //flags that undo array if full and starts from position 0 again
//uint32_t eventLearned;         //flags that user trigger something
uint16_t actualChannel;        //compute pot and points to the chosen channel
uint16_t actualPattern;        //compute pot and points to the pre saved pattern
boolean fillingEuclidianState; //flags that I m in the filling euclidean pattern mode

uint32_t waitForInfoEnds = 0; //end time that info led still lit

uint16_t selector = 0;

Counter potSelector(250);
Counter taskSelector(4);

AnalogInput channelSelect(CHANNELSELPIN, 0, 1023);
int16_t channelMenuVoltages[MAXCHANNELS] = {118, 265, 420, 563, 690, 830, 974, 1024};

DigitalInput bankButton(BANKPIN);
DigitalInput shiftButton(SHIFTPIN);
DigitalInput fillButton(FILLPIN);
DigitalInput undoButton(UNDOPIN);
DigitalInput eraseButton(ERASEPIN);
DigitalInput clksrcButton(CLOCKSRCPIN);
DigitalInput loadSaveButton(LOADSAVEPIN);

DigitalInput resetButton(RESETPIN);

EventDebounce interfaceEvent;

EventDebounce exitEuclidianFillState(MAXGIVEUPTIME);

boolean patterns[16][16] = {
    {1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0},
    {0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0},
    {1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0},
    {1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0},
    {1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0},
    {1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0},
    {1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0},
    {1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1},
    {1, 0, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0},
    {1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1, 1},
    {1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0}};

uint32_t _start;
uint32_t _lenght;

boolean bkpPattern[MAXSTEPS];

//these two includes must be in here , after all variables definition
#include "eprom.h"
#include "transaction.h"

void setup()
{
    if (debug)
        Serial.begin(9600);

    channelSelect.setMenu(channelMenuVoltages, MAXCHANNELS);
    pinMode(LEARNPIN, INPUT);
    pinMode(GHOSTPIN, INPUT);

    for (uint16_t k = 0; k < 2; k++)
        for (uint16_t i = 0; i < MAXCHANNELS; i++)
            for (uint16_t j = 0; j < MAXSTEPS; j++)
                triggerTable[k][i][j] = 0;

    clearUndos();

    // //!!!!!!!!!!!!!!!!!first eeprom burn!!!!!!!!!!!!!!!!!
    // maxTriggerChannels = 6;
    // clearEEprom();
    // //!!!!!!!!!!!!!!!!!first eeprom burn!!!!!!!!!!!!!!!!!

    //if LOAD/SAVE button pressed , clear ALLLLLLL EEprom
    if (digitalRead(LOADSAVEPIN))
    {
        clearEEprom();
        saveEEpromTriggerQuantity();
    }

    //if BANK button pressed , change only trigger quantity
    if (digitalRead(BANKPIN))
    {
        //compute new trigger quantity and save it
        uint16_t val;
        //read 100 samples
        for (byte i = 0; i < 100; i++)
            val = channelSelect.readSmoothed();

        if (val < 30)
            //if the knob is fully counter clockwise , means no trigger channel
            maxTriggerChannels = 0;
        else
            //if not , compute the menu index + 1
            maxTriggerChannels = channelSelect.getSmoothedMenuIndex() + 1;
        saveEEpromTriggerQuantity();
    }
    else
    {
        //BY DEFAULT load previous saved configuration
        loadEEpromTriggerQuantity();
    }
    loadEEpromBanks();

    //if FILL button pressed , load cool start patterns
    if (digitalRead(FILLPIN))
        loadMyCoolGrid();
}

void loop()
{
    computeClockSource();
    computeBpm();
    readActualChannel();
    stepReset();
    stepForward(pleaseCycle);
    computeLoadSave();
    if (interfaceEvent.debounced())
    {
        computeLearn(digitalRead(LEARNPIN) == LOW, true);
        computeLearn(digitalRead(GHOSTPIN) == HIGH, false);
        taskSelector.advance();
        switch (taskSelector.getValue())
        {
        case 0:
            computeBank();
            break;
        case 1:
            computeShift();
            break;
        case 2:
            computeFill();
            break;
        case 3:
            computeUndo();
            break;
        case 4:
            computeErase();
            break;
        }
    }
    closeTriggers();
    eog.compute();
}

void stepReset()
{
    resetButton.readPin();
    if (resetButton.thisWasSwitchedOn())
        thisStep.reset();
}

void stepForward(boolean condition)
{
    if (condition)
    {
        pleaseCycle = false;
        //calculated time between triggers , this is usefull to quantize the step when user trigger aprende
        calculatedTriggerInterval = millis() - lastClockTick;
        lastClockTick = millis();
        thisStep.advance();
        openTriggers();
        openGates();
        closeGates();
        // Serial.print(" - ");
        // Serial.println(thisStep.getValue());
    }
}

void openGates()
{
    //turn on GATES
    for (uint16_t channel = maxTriggerChannels; channel < MAXCHANNELS; channel++)
    {

        if ((triggerTable[bankChoice[channel]][channel][thisStep.getValue()] != CLOSETRIGGER) && //if new step is good to open gate
            !triggerArray[channel]->pinState())                                                  //and it is closed
        {
            triggerArray[channel]->start();
            triggerArray[channel]->setNoexp();
        }
        //if it was a ghost trigger , untrigger it
        if (triggerTable[bankChoice[channel]][channel][thisStep.getValue()] == GHOSTTRIGGER)
            triggerTable[bankChoice[channel]][channel][thisStep.getValue()] = CLOSETRIGGER;
    }
}

void closeGates()
{
    //turn off GATES
    for (uint16_t channel = maxTriggerChannels; channel < MAXCHANNELS; channel++)
    {
        if (
            (triggerTable[bankChoice[channel]][channel][thisStep.getValue()] == CLOSETRIGGER) //if this step is good to close
            && (triggerArray[channel]->pinState())                                            //and it was opened
            && digitalRead(LEARNPIN)                                                          //and user not pressing TAP switch
            && !digitalRead(GHOSTPIN)                                                         //and user not pressing GHOST switch
        )
            triggerArray[channel]->end(); //turn off led
        //if it was a ghost trigger , untrigger it
        if (triggerTable[bankChoice[channel]][channel][thisStep.getValue()] == GHOSTTRIGGER)
            triggerTable[bankChoice[channel]][channel][thisStep.getValue()] = CLOSETRIGGER;
    }
}

void openTriggers()
{
    //time to turn on TRIGGERS < maxTriggerChannels
    for (uint16_t channel = 0; channel < maxTriggerChannels; channel++)
    {
        if (triggerTable[bankChoice[channel]][channel][thisStep.getValue()] != CLOSETRIGGER)
        {
            //light on led, save state
            triggerArray[channel]->start();
            //if it was a ghost trigger , untrigger it
            if (triggerTable[bankChoice[channel]][channel][thisStep.getValue()] == GHOSTTRIGGER)
                triggerTable[bankChoice[channel]][channel][thisStep.getValue()] = CLOSETRIGGER;
        }
    }
    if ((thisStep.getValue() == 24) || (thisStep.getValue() == 28) || (thisStep.getValue() == 0))
        eog.start();
}

//close all opened triggers
void closeTriggers()
{
    for (uint16_t channel = 0; channel < maxTriggerChannels; channel++)
        triggerArray[channel]->compute();
}

//compute learn pin and calculate if it should lit this step or in the next
void computeLearn(boolean condition, boolean learnMe)
{
    if (condition) //if tap or ghost button pressed
    {
        //quantize TARGETSTEP to this or to the next step
        uint32_t now = millis();
        int32_t targetStep;

        if (now < (uint32_t)(lastClockTick + (calculatedTriggerInterval / 2))) //same step
        {
            targetStep = thisStep.getValue();
            //play it if it is blank (if it wasnt blank , it was already played)
            if (triggerTable[bankChoice[actualChannel]][actualChannel][targetStep] == 0)
                triggerArray[actualChannel]->start();
        }
        else //next step
        {
            targetStep = thisStep.getValue() + 1;
            if (targetStep >= MAXSTEPS) //if next step is greater tham max steps , points to 0
                targetStep = 0;
        }

        if (learnMe) //if it is a real trigger, in thge same step or next step, save it
        {
            saveTrigger(targetStep, OPENTRIGGER);
            commitTrans();
        }
        else //or it is a ghost trigger
        {
            //only saves a ghost trigger if it will be next step
            if (targetStep != thisStep.getValue())
                saveTrigger(targetStep, GHOSTTRIGGER);
        }
        interfaceEvent.debounce(lastClockTick + calculatedTriggerInterval - now);
    }
}

//compute load save pin and do it
void computeLoadSave()
{
    loadSaveButton.readPin();
    if (loadSaveButton.thisWasDoubleGate())
    {
        loadEEpromTriggerQuantity();
        loadEEpromBanks();
        clearUndos();
    }
    else if (loadSaveButton.thisWasLargeGate())
    {
        saveEEpromTriggerQuantity();
        saveEEpromBanks();
    }
}

//defines wich one bank we are using for a especific actualChannel
void computeBank()
{
    if (bankButton.readRawPin())
    {
        if (bankChoice[actualChannel] == 0)
            bankChoice[actualChannel] = 1;
        else
            bankChoice[actualChannel] = 0;
        interfaceEvent.debounce(DEBOUNCETIME);
    }
}

//compute shift pin and rotate an esspecific channel grid
void computeShift()
{
    if (shiftButton.readRawPin())
    {
        uint16_t spare;
        //rotate always forward
        spare = triggerTable[bankChoice[actualChannel]][actualChannel][MAXSTEPS - 1];
        for (uint16_t col = MAXSTEPS - 1; col > 0; col--)
            saveTrigger(col, triggerTable[bankChoice[actualChannel]][actualChannel][col - 1]);
        saveTrigger(0, spare);
        interfaceEvent.debounce(DEBOUNCETIME);
    }
}

//identify the last step lit and copy all previous sequence until the end of the grid
void computeFill()
{
    fillButton.readPin();
    //if I want to enter into fill euclidian state and if possible, enters the "filling pattern mode"
    if (fillButton.thisWasLargeGate() && !fillingEuclidianState)
    {
        fillingEuclidianState = true;
        bkpThisPattern();
        eog.start(MAXGIVEUPTIME);
        exitEuclidianFillState.debounce();
    }
    else if (fillButton.thisWasSmallGate())
    {
        if (fillingEuclidianState) //if filling euclidean state
        {
            //exit
            eog.end();
            fillingEuclidianState = false;
        }
        else
        {
            //if ordinary fill / repeat steps
            for (int16_t lastTrigger = (MAXSTEPS - 1); lastTrigger >= 0; lastTrigger--)
            { //find last trigger on sequence
                if (triggerTable[bankChoice[actualChannel]][actualChannel][lastTrigger] == 1)
                {
                    int16_t sourceTrigger = 0; //point to zero position to copy
                    for (int16_t stepPtr = lastTrigger; stepPtr < MAXSTEPS; stepPtr++)
                    { //for last trigger until the end of sequence
                        saveTrigger(stepPtr, triggerTable[bankChoice[actualChannel]][actualChannel][sourceTrigger]);
                        sourceTrigger++; //update source position
                        if (sourceTrigger >= lastTrigger)
                            sourceTrigger = 0; //bring it back to 0
                    }
                    commitTrans();
                    interfaceEvent.debounce(DEBOUNCETIME);
                    return;
                }
            }
        }
    }
}

//compute undo pin and rollback trans
void computeUndo()
{
    if ((undoButton.readRawPin()) && (lastUndoPtr >= 0))
    {
        rollbackTrans();
        interfaceEvent.debounce(DEBOUNCETIME);
    }
}

void clearUndos()
{
    for (uint16_t i = 0; i < 5; i++)
        for (uint16_t j = 0; j < MAXSTEPS; j++)
            undo[i][j] = 0;
}

//erase a full channel
void computeErase()
{
    if (eraseButton.readRawPin())
    {
        for (int16_t stepPtr = 0; stepPtr < MAXSTEPS; stepPtr++)
            saveTrigger(stepPtr, CLOSETRIGGER); //for last trigger until the end of sequence
        commitTrans();
        interfaceEvent.debounce(DEBOUNCETIME);
    }
}

//compute pin and decides if ext or int clock source
void computeClockSource()
{
    //change timer source internal or external
    if (interfaceEvent.debounced() && clksrcButton.readPin())
    {
        if (internalClock) //becomes EXTERNAL clock
            internalClock = false;
        else //becomes INTERNAL clock
            internalClock = true;
        interfaceEvent.debounce();
    }
    if (internalClock)
    {
        if (internalTickTack.debounced())
        {
            pleaseCycle = true;
            internalTickTack.debounce(bpm2micros(bpm));
        }
    }
    else
    {
        //external trigger in
        if ((digitalRead(CLOCKINPIN) == HIGH) && triggerInInterval.debounced())
        {
            pleaseCycle = true;
            triggerInInterval.debounce();
        }
    }
}

//pots ******************************************************************************************************************
// void debounce(uint32_t newValue)
// {
//     _lenght = newValue;
//     _start = millis();
// }

// boolean debounced()
// {
//     return (millis() > (_start + _lenght));
// }

uint32_t bpm2micros(uint16_t bpm)
{
    return ((60000UL / (uint32_t)bpm) / 4UL) * 1000UL;
}

//compute bpm pot and update internal clock
void computeBpm()
{
    potSelector.advance();
    if ((potSelector.getValue() == 0) && internalClock)
    {
        uint16_t bpmRead = MINBPMVALUE + (analogRead(BPMPIN) >> 3);
        if (bpmRead != bpm)
            bpm = bpmRead;
    }
}

//bkp the actual channel pattern
void bkpThisPattern()
{
    for (uint16_t i = 0; i < MAXSTEPS; i++)
        bkpPattern[i] = triggerTable[bankChoice[actualChannel]][actualChannel][i];
}
//restore actual channel pattern
void restoreThisPattern()
{
    for (uint16_t i = 0; i < MAXSTEPS; i++)
        triggerTable[bankChoice[actualChannel]][actualChannel][i] = bkpPattern[i];
}

//defines actual channel
void readActualChannel()
{
    channelSelect.readSmoothed();
    //if not in filling pattern mode , update actual channel
    if (!fillingEuclidianState)
        actualChannel = channelSelect.getSmoothedMenuIndex();
    else
    {
        //if filling pattern mode , update the channel pattern with saved patterns
        uint16_t patternRead = analogRead(CHANNELSELPIN) >> 6;
        if (actualPattern != patternRead)
        {
            exitEuclidianFillState.debounce(); //start the 4 seconds timer
            eog.start(MAXGIVEUPTIME);          //start eog led
            actualPattern = patternRead;       //read new channel position WITHOUT change the master ACTUALCHANNEL
            for (uint16_t j = 0; j < 2; j++)
                for (uint16_t i = 0; i < 16; i++)
                    triggerTable[bankChoice[actualChannel]][actualChannel][(j * 16) + i] = patterns[actualPattern][i];
        }
        //if no changes made on choosing any euclidian rythm into 3 seconds
        if (exitEuclidianFillState.debounced())
        {
            //restores the original pattern and exit mode
            restoreThisPattern();
            fillingEuclidianState = false;
            eog.end();
        }
    }
}

void loadMyCoolGrid()
{
    //data test
    //kick A
    triggerTable[0][0][0] = OPENTRIGGER;
    triggerTable[0][0][4] = OPENTRIGGER;
    triggerTable[0][0][8] = OPENTRIGGER;
    triggerTable[0][0][12] = OPENTRIGGER;
    triggerTable[0][0][16] = OPENTRIGGER;
    triggerTable[0][0][20] = OPENTRIGGER;
    triggerTable[0][0][24] = OPENTRIGGER;
    triggerTable[0][0][28] = OPENTRIGGER;

    //kick B
    for (uint16_t j = 0; j < MAXSTEPS; j++)
        if ((j % 2) == 0)
            triggerTable[1][0][j] = OPENTRIGGER;

    //hats A
    triggerTable[0][1][2] = OPENTRIGGER;
    triggerTable[0][1][6] = OPENTRIGGER;
    triggerTable[0][1][10] = OPENTRIGGER;
    triggerTable[0][1][14] = OPENTRIGGER;
    triggerTable[0][1][18] = OPENTRIGGER;
    triggerTable[0][1][22] = OPENTRIGGER;
    triggerTable[0][1][26] = OPENTRIGGER;
    triggerTable[0][1][30] = OPENTRIGGER;

    //clap B
    triggerTable[0][2][4] = OPENTRIGGER;
    triggerTable[0][2][12] = OPENTRIGGER;
    triggerTable[0][2][20] = OPENTRIGGER;
    triggerTable[0][2][28] = OPENTRIGGER;

    //gates A - 8
    for (uint16_t i = 0; i < 4; i++)
        for (uint16_t j = 0; j < 4; j++)
            triggerTable[0][7][i * 8 + j] = OPENTRIGGER;
}
