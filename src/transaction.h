void saveTrigger(uint16_t _targetStep, uint16_t _newValue)
{
  //if it is not a false trigger , prepare and point to next undo
  if (_newValue < 2)
  {
    lastUndoPtr++;
    //if undo table is full
    if (lastUndoPtr >= MAXUNDOS)
    {
      //point last undo back to 0
      lastUndoPtr = 0;
      //block circular rollbacks
      rollbackBlock=0; 
    }
    //if rollbackBlock was activated , increase it together with normal lastUndoPtr
    if (rollbackBlock >= 0) {
      rollbackBlock = lastUndoPtr +1;
     }
    //save [table][ channel,step, last value,commit flag]
    undo[0][lastUndoPtr] = actualChannel;
    undo[1][lastUndoPtr] = _targetStep;
    undo[2][lastUndoPtr] = triggerTable[bankChoice[actualChannel]][actualChannel][_targetStep];
    undo[3][lastUndoPtr] = 0;
    undo[4][lastUndoPtr] = bankChoice[actualChannel];
  }
  //saves the new trigger , even if it was a ghost trigger
  triggerTable[bankChoice[actualChannel]][actualChannel][_targetStep] = _newValue;
}

void commitTrans()
{
  undo[3][lastUndoPtr] = 1;
}

void rollbackTrans()
{
  if (lastUndoPtr >= 0)
  { //if there was undos available
    undo[3][lastUndoPtr] = 0; //force the last one undo to rollback
    while (undo[3][lastUndoPtr] != 1)
    { //rollback command
      //[table][ channel, step, last value,commit flag]
      uint16_t previousReg0 = undo[0][lastUndoPtr];
      uint16_t previousReg1 = undo[1][lastUndoPtr];
      uint16_t previousReg2 = undo[2][lastUndoPtr];
      uint16_t previousReg4 = undo[4][lastUndoPtr];
      triggerTable[previousReg4][previousReg0][previousReg1] = previousReg2;
      undo[0][lastUndoPtr] = 0;
      undo[1][lastUndoPtr] = 0;
      undo[2][lastUndoPtr] = 0;
      undo[3][lastUndoPtr] = 0;
      undo[4][lastUndoPtr] = 0;
      //point to previous command
      //search for the previous commit
      lastUndoPtr--;
      //if circular rollback reaches the minimum value
      if(lastUndoPtr == rollbackBlock){
        //if no more undos allowed , return
        return;
      }
      //if more than 400 undos, fifo problem
      if ((lastUndoPtr < 0) && (rollbackBlock > -1))
      {
        lastUndoPtr = MAXUNDOS - 1;
      } else if (lastUndoPtr < 0) {
        //if no more undos , return
        return;
      }
    }
  }
}

