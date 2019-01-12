/*
    Avalon Star GO Focuser
    Copyright (C) 2018 Christopher Contaxis (chrconta@gmail.com) and
    Wolfgang Reissenberger (sterne-jaeger@t-online.de)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*/

#include "stargofocuser.h"

#include <cstring>

#define AVALON_FOCUSER_POSITION_OFFSET                  500000

/**
 * @brief Constructor
 * @param defaultDevice the telescope
 * @param name device name
 */
StarGoFocuser::StarGoFocuser() : FI(this)
{
//    baseDevice = defaultDevice;
//    deviceName = name;
}

/**
 * @brief Initialize the focuser UI controls
 * @param groupName tab where the UI controls are grouped
 */
bool StarGoFocuser::initProperties()
{
    StarGoTelescope::initProperties();

    FI::initProperties(FOCUS_TAB);

//    IUFillNumber(&FocusSyncPosN[0], "FOCUS_SYNC_POSITION_VALUE", "Ticks", "%4.0f", 0.0, 100000.0, 1000.0, 0);
//    IUFillNumberVector(&FocusSyncPosNP, FocusSyncPosN, 1, getDeviceName(), "FOCUS_SYNC_POSITION", "Sync", deviceName, IP_WO, 0, IPS_OK);

    FI::SetCapability(
        FOCUSER_CAN_ABS_MOVE        | /*!< Can the focuser move by absolute position? */
        FOCUSER_CAN_REL_MOVE        | /*!< Can the focuser move by relative position? */
        FOCUSER_CAN_ABORT           | /*!< Is it possible to abort focuser motion? */
        FOCUSER_CAN_REVERSE         | /*!< Is it possible to reverse focuser motion? */
        FOCUSER_CAN_SYNC            | /*!< Can the focuser sync to a custom position */
        FOCUSER_HAS_VARIABLE_SPEED    /*!< Can the focuser move in different configurable speeds? */
    );
    FocusSpeedN[0].min   = 0;
    FocusSpeedN[0].max   = 10;
    FocusSpeedN[0].step  = 1;
    FocusSpeedN[0].value = 1;

    return true;
}

/**
 * @brief Fill the UI controls with current values
 * @return true iff everything went fine
 */

bool StarGoFocuser::updateProperties()
{
    if (isConnected())
    {
        StarGoTelescope::updateProperties();
        FI::updateProperties();
    }
    else
    {
        StarGoTelescope::updateProperties();
        FI::updateProperties();
    }
    return true;
}

bool StarGoFocuser::ReadScopeStatus()
{
    return StarGoTelescope::ReadScopeStatus();
}

/***************************************************************************
 * Reaction to UI commands
 ***************************************************************************/

bool StarGoFocuser::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    INDI_UNUSED(states);
    INDI_UNUSED(names);
    INDI_UNUSED(n);

    //  first check if it's for our device
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strstr(name, "FOCUS"))
        {
            return FI::processSwitch(dev, name, states, names, n);
        }
    }
    return StarGoTelescope::ISNewSwitch(dev, name, states, names, n);
}


bool StarGoFocuser::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    INDI_UNUSED(values);
    INDI_UNUSED(names);
    INDI_UNUSED(n);

    //  first check if it's for our device
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strstr(name, "FOCUS"))
        {
            return FI::processNumber(dev, name, values, names, n);
        }
    }
    return StarGoTelescope::ISNewNumber(dev, name, values, names, n);
}

/***************************************************************************
 *
 ***************************************************************************/

bool StarGoFocuser::SetFocuserSpeed(int speed)
{
    // Command  - :X1Caaaa*bb#
    // Response - 0#
    bool valid = false;
    unsigned int param[10][2] = 
    {
        {9000,1},
        {6000,1},
        {4000,1},
        {2500,1},
        {1000,5},
        {750,10},
        {500,20},
        {250,30},
        {100,40},
        {60,50}
    };
    char cmd[AVALON_COMMAND_BUFFER_LENGTH];
    char response[AVALON_RESPONSE_BUFFER_LENGTH];

    sprintf(cmd, ":X1C%4d*%2d#", param[speed-1][0],param[speed-1][1]);

//    switch(speed) {
//    case 1: valid = transmit(":X1C9000*01#"); break;
//    case 2: valid = transmit(":X1C6000*01#"); break;
//    case 3: valid = transmit(":X1C4000*01#"); break;
//    case 4: valid = transmit(":X1C2500*01#"); break;
//    case 5: valid = transmit(":X1C1000*05#"); break;
//    case 6: valid = transmit(":X1C0750*10#"); break;
//    case 7: valid = transmit(":X1C0500*20#"); break;
//    case 8: valid = transmit(":X1C0250*30#"); break;
//    case 9: valid = transmit(":X1C0100*40#"); break;
//    case 10: valid = transmit(":X1C0060*50#"); break;
//    default: DEBUGF(INDI::Logger::DBG_ERROR, "%s: Invalid focuser speed %d specified.", getDeviceName(), speed);
//    }
    if (!sendQuery(cmd, response))
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "%s: Failed to send new focuser speed command.", getDeviceName());
        return false;
    }
    return valid;
}

IPState StarGoFocuser::MoveFocuser(FocusDirection dir, int speed, uint16_t duration)
{
    INDI_UNUSED(speed);
    if (duration == 0) 
    {
        return IPS_OK;
    }
    int position = FocusAbsPosN[0].min;
    if (dir == FOCUS_INWARD) 
    {
        position = FocusAbsPosN[0].max;
    }
    moveFocuserDurationRemaining = duration;
    return MoveAbsFocuser(position);
}

IPState StarGoFocuser::MoveAbsFocuser(uint32_t position)
{
    // Command  - :X16pppppp#
    // Response - Nothing
    bool result = true;
    targetFocuserPosition = position;
    char command[AVALON_COMMAND_BUFFER_LENGTH] = {0};
    char response[AVALON_RESPONSE_BUFFER_LENGTH];
    sprintf(command, ":X16%06d#", AVALON_FOCUSER_POSITION_OFFSET + targetFocuserPosition);
    if ((result = sendQuery(command, response,0)) )
    {
        LOGF_ERROR("%s: Failed to send AUX1 goto command.", getDeviceName());
    }
    return result? IPS_BUSY: IPS_ALERT;
}

IPState StarGoFocuser::MoveRelFocuser(FocusDirection dir, uint32_t relativePosition)
//IPState StarGoFocuser::moveFocuserRelative(int relativePosition)
{
    int absolutePosition = relativePosition * (dir==FOCUS_INWARD?-1:+1);
    return MoveAbsFocuser(absolutePosition);
}


bool StarGoFocuser::AbortFocuser()
{
    // Command  - :X0AAUX1ST#
    // Response - Nothing
    char response[AVALON_RESPONSE_BUFFER_LENGTH];
    if (!sendQuery(":X0AAUX1ST#", response, 0)) 
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "%s: Failed to send AUX1 stop command.", getDeviceName());
        return false;
    }
    return true;
}

bool StarGoFocuser::SyncFocuser(uint32_t  position)
{
    // Command  - :X0Cpppppp#
    // Response - Nothing
    char response[AVALON_RESPONSE_BUFFER_LENGTH];
    char command[AVALON_COMMAND_BUFFER_LENGTH] = {0};
    sprintf(command, ":X0C%06d#", AVALON_FOCUSER_POSITION_OFFSET + position);
    if (!sendQuery(command, response,0)) 
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "%s: Failed to send AUX1 sync command.", getDeviceName());
        return false;
    }
    return true;
}

/***************************************************************************
 * LX200 queries, sent to baseDevice
 ***************************************************************************/

bool StarGoFocuser::getFocuserPosition(int* position) {
    // Command  - :X0BAUX1AS#
    // Response - AX1=ppppppp #
    char response[AVALON_RESPONSE_BUFFER_LENGTH];
    if(!sendQuery(":X0BAUX1AS#", response)) 
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "%s: Failed to get AUX1 position request.", getDeviceName());
        return false;
    }
//    if (!receive(response, &bytesReceived)) 
//    {
//        DEBUGF(INDI::Logger::DBG_ERROR, "%s: Failed to receive AUX1 position response.", getDeviceName());
//        return false;
//    }
    int tempPosition = 0;
    int returnCode = sscanf(response, "%*c%*c%*c%*c%07d", &tempPosition);
    if (returnCode <= 0) 
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "%s: Failed to parse AUX1 position response '%s'.", getDeviceName(), response);
        return false;
    }
    (*position) = (tempPosition - AVALON_FOCUSER_POSITION_OFFSET);
    return true;
}
