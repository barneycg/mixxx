// ****************************************************************************
// * Mixxx mapping script file for the Behringer CMD Studio 2a.
// * Author: Barney Garrett
// * Version 0.1 (Sep 2017)
// * Forum: http://www.mixxx.org/forums/viewtopic.php?f=7&amp;t=7868
// * Wiki: http://www.mixxx.org/wiki/doku.php/behringer_cmd_studio_4a
// ****************************************************************************

////////////////////////////////////////////////////////////////////////
// JSHint configuration                                               //
////////////////////////////////////////////////////////////////////////
/* global engine                                                      */
/* global script                                                      */
/* global print                                                       */
/* global midi                                                        */
//////////////////////////////////////////////////////////////////////// 

// Master function definition.
function BehringerCMDStudio2a() {}


// ***************************** Global Vars **********************************

// Shift/mode state variables.
BehringerCMDStudio2a.delButtonState = [false,false,false,false];
BehringerCMDStudio2a.scratchButtonState = [true,true,true,true];

// Button push/release state variables.
BehringerCMDStudio2a.pitchPushed = [[false,false,false,false], [false,false,false,false]];
BehringerCMDStudio2a.delPushed = false;
BehringerCMDStudio2a.delShiftUsed = false;
BehringerCMDStudio2a.fxAssignPushed = false;
BehringerCMDStudio2a.fxAssignShiftUsed = false;
BehringerCMDStudio2a.fxAssignLastGroup = "";

// ************************ Initialisation stuff. *****************************

BehringerCMDStudio2a.initLEDs = function () {
    // (re)Initialise any LEDs that are direcctly controlled by this script.
    
    // Temporary turn everything blue :)
    
    midi.sendShortMsg(0x90, 0x08, 0x00); // Assign A A
    midi.sendShortMsg(0x90, 0x09, 0x00); // Assign A B
    midi.sendShortMsg(0x90, 0x38, 0x00); // Assign B A
    midi.sendShortMsg(0x90, 0x39, 0x00); // Assign B B
    midi.sendShortMsg(0x90, 0x01, 0x00); // Cue A
    midi.sendShortMsg(0x90, 0x31, 0x00); // Cue B
    midi.sendShortMsg(0x90, 0x02, 0x00); // Play A
    midi.sendShortMsg(0x90, 0x32, 0x00); // Play B
    midi.sendShortMsg(0x90, 0x16, 0x00); // Cue A A
    midi.sendShortMsg(0x90, 0x46, 0x00); // Cue B B
    midi.sendShortMsg(0x90, 0x22, 0x00); // vinyl
    midi.sendShortMsg(0x90, 0x23, 0x00); // mode
    midi.sendShortMsg(0x90, 0x25, 0x00); // Folder
    midi.sendShortMsg(0x90, 0x26, 0x00); // File
}

BehringerCMDStudio2a.init = function () {
    // Initialise anything that might not be in the correct state.
    BehringerCMDStudio2a.initLEDs();
}
 
BehringerCMDStudio2a.shutdown = function() {
    // Leave the deck in a properly initialised state.
    BehringerCMDStudio2a.initLEDs();
}

// Functions to deal with the wheel (i.e. scratcing and jog).
// Why is there no (XML) support in Mixxx for this most basic of functions?
// I suspect the vast majority of controller mappings use the same code
// (provided in the Wiki).
BehringerCMDStudio2a.wheelTouch = function (channel, control, value, status, group) {
    var deck = script.deckFromGroup(group);
    //channel = channel+1;
    if (value > 0) {
        // We're touching the wheel.
        var alpha = 1.0/8;
        var beta = alpha/32;
        engine.scratchEnable(deck, 128, 33+1/3, alpha, beta);
    } else {
        // We've released the wheel.
        engine.scratchDisable(deck);
    }
}

BehringerCMDStudio2a.wheelTurn = function (channel, control, value, status, group) {
    //var deck = channel+1;
    var deck = script.deckFromGroup(group);
    var deck_array = deck-1;
    var newValue = value-64;
    if (true){
        if (engine.isScratching(deck)){
            engine.scratchTick(deck,newValue);  // Scratch!
        } else {
            engine.setValue(group, "jog", newValue); // Jog.
        }
    } else {
            engine.setValue(group, "jog", newValue); // Jog.
    }
}
