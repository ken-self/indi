/*
    Microtouch Focuser
    Copyright (C) 2016 Marco Peters (mpeters@rzpeters.de)
    Copyright (C) 2013 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "microtouch.h"

#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <cmath>
#include <memory>
#include <cstring>
#include <termios.h>
#include <unistd.h>

#define MICROTOUCH_TIMEOUT 3

static std::unique_ptr<Microtouch> microTouch(new Microtouch());

Microtouch::Microtouch()
{
    // Can move in Absolute & Relative motions, can AbortFocuser motion, and has variable speed.
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE |
                      FOCUSER_CAN_ABORT | FOCUSER_CAN_SYNC);
}

bool Microtouch::initProperties()
{
    INDI::Focuser::initProperties();

    FocusSpeedNP[0].setMin(1);
    FocusSpeedNP[0].setMax(5);
    FocusSpeedNP[0].setValue(1);

    /* Step Mode */
    IUFillSwitch(&MotorSpeedS[0], "Normal", "", ISS_ON);
    IUFillSwitch(&MotorSpeedS[1], "Fast", "", ISS_OFF);
    IUFillSwitchVector(&MotorSpeedSP, MotorSpeedS, 2, getDeviceName(), "Motor Speed", "", OPTIONS_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    /* Focuser temperature */
    IUFillNumber(&TemperatureN[0], "TEMPERATURE", "Celsius", "%6.2f", -50, 70., 0., 0.);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 1, getDeviceName(), "FOCUS_TEMPERATURE", "Temperature",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Maximum Travel
    //    IUFillNumber(&MaxTravelN[0], "MAXTRAVEL", "Maximum travel", "%6.0f", 1., 60000., 0., 10000.);
    //    IUFillNumberVector(&MaxTravelNP, MaxTravelN, 1, getDeviceName(), "FOCUS_MAXTRAVEL", "Max. travel", OPTIONS_TAB,
    //                       IP_RW, 0, IPS_IDLE);

    // Temperature Settings
    IUFillNumber(&TemperatureSettingN[0], "Calibration", "", "%6.2f", -20, 20, 0.01, 0);
    IUFillNumber(&TemperatureSettingN[1], "Coefficient", "", "%6.2f", -20, 20, 0.01, 0);
    IUFillNumberVector(&TemperatureSettingNP, TemperatureSettingN, 2, getDeviceName(), "Temperature Settings", "",
                       OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    // Compensate for temperature
    IUFillSwitch(&TemperatureCompensateS[0], "Enable", "", ISS_OFF);
    IUFillSwitch(&TemperatureCompensateS[1], "Disable", "", ISS_ON);
    IUFillSwitchVector(&TemperatureCompensateSP, TemperatureCompensateS, 2, getDeviceName(), "Temperature Compensate",
                       "", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Reset
    //    IUFillSwitch(&ResetS[0], "Zero", "", ISS_OFF);
    //    IUFillSwitchVector(&ResetSP, ResetS, 1, getDeviceName(), "Reset", "", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0,
    //                       IPS_IDLE);

    //    IUFillNumber(&ResetToPosN[0], "Position", "", "%6.0f", 0, 60000, 1, 0);
    //    IUFillNumberVector(&ResetToPosNP, ResetToPosN, 1, getDeviceName(), "Reset to Position", "", MAIN_CONTROL_TAB, IP_RW,
    //                       0, IPS_IDLE);

    /* Relative and absolute movement */
    FocusRelPosNP[0].setMin(0.);
    FocusRelPosNP[0].setMax(30000.);
    FocusRelPosNP[0].setValue(0);
    FocusRelPosNP[0].setStep(1000.);

    FocusAbsPosNP[0].setMin(0.);
    FocusAbsPosNP[0].setMax(60000.);
    FocusAbsPosNP[0].setValue(0);
    FocusAbsPosNP[0].setStep(1000.);

    addDebugControl();
    serialConnection->setDefaultBaudRate(Connection::Serial::B_19200);

    return true;
}

bool Microtouch::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineProperty(&TemperatureNP);
        //defineProperty(&MaxTravelNP);
        defineProperty(&MotorSpeedSP);
        defineProperty(&TemperatureSettingNP);
        defineProperty(&TemperatureCompensateSP);
        //        defineProperty(&ResetSP);
        //        defineProperty(&ResetToPosNP);

        GetFocusParams();

        LOG_INFO("Microtouch parameters updated, focuser ready for use.");
    }
    else
    {
        deleteProperty(TemperatureNP.name);
        //        deleteProperty(MaxTravelNP.name);
        deleteProperty(MotorSpeedSP.name);
        deleteProperty(TemperatureSettingNP.name);
        deleteProperty(TemperatureCompensateSP.name);
        //        deleteProperty(ResetSP.name);
        //        deleteProperty(ResetToPosNP.name);
    }

    return true;
}

bool Microtouch::Handshake()
{
    tcflush(PortFD, TCIOFLUSH);

    if (Ack())
    {
        LOG_INFO("Microtouch is online. Getting focus parameters...");
        return true;
    }

    LOG_INFO("Error retrieving data from Microtouch, please ensure Microtouch controller is "
             "powered and the port is correct.");
    return false;
}

const char *Microtouch::getDefaultName()
{
    return "Microtouch";
}

bool Microtouch::Ack()
{
    return updatePosition();
}

bool Microtouch::updateTemperature()
{
    char resp[7];

    short int ttemp = 0, tcoeff = 0;
    double raw_temp = 0, raw_coeff = 0, tcomp_coeff = 0;

    if (!(WriteCmdGetResponse(CMD_GET_TEMPERATURE, resp, 6)))
        return false;
    ttemp    = ((short int)resp[1] << 8 | ((short int)resp[2] & 0xff));
    raw_temp = ((double)ttemp) / 16;

    tcoeff    = ((short int)resp[5] << 8 | ((short int)resp[4] & 0xff));
    raw_coeff = ((double)tcoeff) / 16;

    tcomp_coeff = (double)(((double)WriteCmdGetInt(CMD_GET_COEFF)) / 128);

    LOGF_DEBUG("updateTemperature : RESP (%02X %02X %02X %02X %02X %02X)", resp[0], resp[1],
               resp[2], resp[3], resp[4], resp[5]);

    TemperatureN[0].value        = raw_temp + raw_coeff;
    TemperatureSettingN[0].value = raw_coeff;
    TemperatureSettingN[1].value = tcomp_coeff;

    return true;
}

bool Microtouch::updatePosition()
{
    char read[3] = {0};

    if (WriteCmdGetResponse(CMD_GET_POSITION, read, 3))
    {
        FocusAbsPosNP[0].setValue(static_cast<double>(static_cast<uint8_t>(read[2]) << 8 | static_cast<uint8_t>(read[1])));
        return true;
    }

    return false;
}

bool Microtouch::updateSpeed()
{
    // TODO
    /*    int nbytes_written=0, nbytes_read=0, rc=-1;
    char errstr[MAXRBUF];
    char resp[3];
    short speed;

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, ":GD#", 4, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("updateSpeed error: %s.", errstr);
        return false;
    }

    if ( (rc = tty_read(PortFD, resp, 3, MICROTOUCH_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("updateSpeed error: %s.", errstr);
        return false;
    }

    rc = sscanf(resp, "%hX#", &speed);

    if (rc > 0)
    {
        int focus_speed=-1;
        while (speed > 0)
        {
            speed >>= 1;
            focus_speed++;
        }

        currentSpeed = focus_speed;
        FocusSpeedNP[0].setValue(focus_speed);
    }
    else
    {
        LOGF_ERROR("Unknown error: focuser speed value (%s)", resp);
        return false;
    }
    */
    return true;
}

bool Microtouch::updateMotorSpeed()
{
    IUResetSwitch(&MotorSpeedSP);

    LOGF_DEBUG("MotorSpeed: %d.", WriteCmdGetByte(CMD_GET_MOTOR_SPEED));

    if (WriteCmdGetByte(CMD_GET_MOTOR_SPEED) == 8)
        MotorSpeedS[0].s = ISS_ON;
    else if (WriteCmdGetByte(CMD_GET_MOTOR_SPEED) == 4)
        MotorSpeedS[1].s = ISS_ON;
    else
    {
        LOGF_ERROR("Unknown error: updateMotorSpeed (%s)", WriteCmdGetByte(CMD_GET_MOTOR_SPEED));
        return false;
    }

    return true;
}

bool Microtouch::isMoving()
{
    return (WriteCmdGetByte(CMD_IS_MOVING) > 0);
}

bool Microtouch::setTemperatureCalibration(double calibration)
{
    return WriteCmdSetShortInt(CMD_SET_TEMP_OFFSET, (short int)(calibration * 16));
}

bool Microtouch::setTemperatureCoefficient(double coefficient)
{
    int tcoeff = (int)(coefficient * 128);

    LOGF_DEBUG("Setting new temperaturecoefficient  : %d.", tcoeff);

    if (!(WriteCmdSetInt(CMD_SET_COEFF, tcoeff)))
    {
        LOG_ERROR("setTemperatureCoefficient error: Setting temperaturecoefficient failed.");
        return false;
    }
    return true;
}

//bool Microtouch::reset()
//{
//    WriteCmdSetIntAsDigits(CMD_RESET_POSITION, 0x00);
//    return true;
//}

//bool Microtouch::reset(double pos)
//{
//    WriteCmdSetIntAsDigits(CMD_RESET_POSITION, (int)pos);
//    return true;
//}

bool Microtouch::SyncFocuser(uint32_t ticks)
{
    WriteCmdSetIntAsDigits(CMD_RESET_POSITION, ticks);
    return true;
}

bool Microtouch::MoveFocuser(unsigned int position)
{
    LOGF_DEBUG("MoveFocuser to Position: %d", position);

    if (position < FocusAbsPosNP[0].getMin() || position > FocusAbsPosNP[0].getMax())
    {
        LOGF_ERROR("Requested position value out of bound: %d", position);
        return false;
    }
    return WriteCmdSetIntAsDigits(CMD_UPDATE_POSITION, position);
}

bool Microtouch::setMotorSpeed(char speed)
{
    if (speed == FOCUS_MOTORSPEED_NORMAL)
        WriteCmdSetByte(CMD_SET_MOTOR_SPEED, 8);
    else
        WriteCmdSetByte(CMD_SET_MOTOR_SPEED, 4);

    return true;
}

bool Microtouch::setSpeed(unsigned short speed)
{
    INDI_UNUSED(speed);
    /*    int nbytes_written=0, rc=-1;
    char errstr[MAXRBUF];
    char cmd[7];

    int hex_value=1;

    hex_value <<= speed;

    snprintf(cmd, 7, ":SD%02X#", hex_value);

    if ( (rc = tty_write(PortFD, cmd, 6, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("setSpeed error: %s.", errstr);
        return false;
    }
    */
    return true;
}

bool Microtouch::setTemperatureCompensation(bool enable)
{
    if (enable)
    {
        if (WriteCmd(CMD_TEMPCOMP_ON))
            return true;
        else
            return false;
    }
    else if (WriteCmd(CMD_TEMPCOMP_OFF))
        return true;

    return false;
}

bool Microtouch::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Focus Motor Speed
        if (strcmp(MotorSpeedSP.name, name) == 0)
        {
            bool rc          = false;
            int current_mode = IUFindOnSwitchIndex(&MotorSpeedSP);

            IUUpdateSwitch(&MotorSpeedSP, states, names, n);

            int target_mode = IUFindOnSwitchIndex(&MotorSpeedSP);

            if (current_mode == target_mode)
            {
                MotorSpeedSP.s = IPS_OK;
                IDSetSwitch(&MotorSpeedSP, nullptr);
            }

            if (target_mode == 0)
                rc = setMotorSpeed(FOCUS_MOTORSPEED_NORMAL);
            else
                rc = setMotorSpeed(FOCUS_MOTORSPEED_FAST);

            if (!rc)
            {
                IUResetSwitch(&MotorSpeedSP);
                MotorSpeedS[current_mode].s = ISS_ON;
                MotorSpeedSP.s              = IPS_ALERT;
                IDSetSwitch(&MotorSpeedSP, nullptr);
                return false;
            }

            MotorSpeedSP.s = IPS_OK;
            IDSetSwitch(&MotorSpeedSP, nullptr);
            return true;
        }

        if (strcmp(TemperatureCompensateSP.name, name) == 0)
        {
            int last_index = IUFindOnSwitchIndex(&TemperatureCompensateSP);
            IUUpdateSwitch(&TemperatureCompensateSP, states, names, n);

            bool rc = setTemperatureCompensation((TemperatureCompensateS[0].s == ISS_ON));

            if (!rc)
            {
                TemperatureCompensateSP.s = IPS_ALERT;
                IUResetSwitch(&TemperatureCompensateSP);
                TemperatureCompensateS[last_index].s = ISS_ON;
                IDSetSwitch(&TemperatureCompensateSP, nullptr);
                return false;
            }

            TemperatureCompensateSP.s = IPS_OK;
            IDSetSwitch(&TemperatureCompensateSP, nullptr);
            return true;
        }

        //        if (strcmp(ResetSP.name, name) == 0)
        //        {
        //            IUResetSwitch(&ResetSP);

        //            if (reset())
        //                ResetSP.s = IPS_OK;
        //            else
        //                ResetSP.s = IPS_ALERT;

        //            IDSetSwitch(&ResetSP, nullptr);
        //            return true;
        //        }
    }

    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool Microtouch::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        //        if (strcmp(name, MaxTravelNP.name) == 0)
        //        {
        //            IUUpdateNumber(&MaxTravelNP, values, names, n);
        //            MaxTravelNP.s = IPS_OK;
        //            IDSetNumber(&MaxTravelNP, nullptr);
        //            return true;
        //        }

        if (strcmp(name, TemperatureSettingNP.name) == 0)
        {
            IUUpdateNumber(&TemperatureSettingNP, values, names, n);
            if (!setTemperatureCalibration(TemperatureSettingN[0].value) ||
                    !setTemperatureCoefficient(TemperatureSettingN[1].value))
            {
                TemperatureSettingNP.s = IPS_ALERT;
                IDSetNumber(&TemperatureSettingNP, nullptr);
                return false;
            }

            TemperatureSettingNP.s = IPS_OK;
            IDSetNumber(&TemperatureSettingNP, nullptr);
        }

        //        if (strcmp(name, ResetToPosNP.name) == 0)
        //        {
        //            IUUpdateNumber(&ResetToPosNP, values, names, n);
        //            if (!reset(ResetToPosN[0].value))
        //            {
        //                ResetToPosNP.s = IPS_ALERT;
        //                IDSetNumber(&ResetToPosNP, nullptr);
        //                return false;
        //            }

        //            ResetToPosNP.s = IPS_OK;
        //            IDSetNumber(&ResetToPosNP, nullptr);
        //        }
    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

void Microtouch::GetFocusParams()
{
    if (updatePosition())
        FocusAbsPosNP.apply();

    if (updateTemperature())
    {
        IDSetNumber(&TemperatureNP, nullptr);
        IDSetNumber(&TemperatureSettingNP, nullptr);
    }

    /*    if (updateSpeed())
        FocusSpeedNP.apply();
    */
    if (updateMotorSpeed())
        IDSetSwitch(&MotorSpeedSP, nullptr);
}

bool Microtouch::SetFocuserSpeed(int speed)
{
    bool rc = false;

    rc = setSpeed(speed);

    if (!rc)
        return false;

    currentSpeed = speed;

    FocusSpeedNP.setState(IPS_OK);
    FocusSpeedNP.apply();

    return true;
}

IPState Microtouch::MoveFocuser(FocusDirection dir, int speed, uint16_t duration)
{
    if (speed != (int)currentSpeed)
    {
        bool rc = setSpeed(speed);

        if (!rc)
            return IPS_ALERT;
    }

    gettimeofday(&focusMoveStart, nullptr);
    focusMoveRequest = duration / 1000.0;

    if (dir == FOCUS_INWARD)
        MoveFocuser(0);
    else
        MoveFocuser(FocusAbsPosNP[0].getValue() + FocusMaxPosNP[0].getValue() - 1);

    if (duration <= getCurrentPollingPeriod())
    {
        usleep(duration * 1000);
        AbortFocuser();
        return IPS_OK;
    }

    return IPS_BUSY;
}

IPState Microtouch::MoveAbsFocuser(uint32_t targetTicks)
{
    targetPos = targetTicks;

    bool rc = false;

    rc = MoveFocuser(targetPos);

    if (!rc)
        return IPS_ALERT;

    FocusAbsPosNP.setState(IPS_BUSY);

    return IPS_BUSY;
}

IPState Microtouch::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    double newPosition = 0;
    bool rc            = false;

    if (dir == FOCUS_INWARD)
        newPosition = FocusAbsPosNP[0].getValue() - ticks;
    else
        newPosition = FocusAbsPosNP[0].getValue() + ticks;

    rc = MoveFocuser(newPosition);

    if (!rc)
        return IPS_ALERT;

    FocusRelPosNP[0].setValue(ticks);
    FocusRelPosNP.setState(IPS_BUSY);

    return IPS_BUSY;
}

void Microtouch::TimerHit()
{
    if (!isConnected())
    {
        return;
    }

    bool rc = updatePosition();

    if (rc)
    {
        if (fabs(lastPos - FocusAbsPosNP[0].getValue()) > 1)
        {
            FocusAbsPosNP.apply();
            lastPos = FocusAbsPosNP[0].getValue();
        }
    }

    rc = updateTemperature();
    if (rc)
    {
        if (fabs(lastTemperature - TemperatureN[0].value) >= 0.01)
        {
            IDSetNumber(&TemperatureNP, nullptr);
            lastTemperature = TemperatureN[0].value;
        }
    }

    if (FocusTimerNP.getState() == IPS_BUSY)
    {
        float remaining = CalcTimeLeft(focusMoveStart, focusMoveRequest);
        if (remaining <= 0)
        {
            FocusTimerNP.setState(IPS_OK);
            FocusTimerNP[0].setValue(0);
            AbortFocuser();
        }
        else
            FocusTimerNP[0].setValue(remaining * 1000.0);
        FocusTimerNP.apply();
    }

    if (FocusAbsPosNP.getState() == IPS_BUSY || FocusRelPosNP.getState() == IPS_BUSY)
    {
        if (!isMoving())
        {
            FocusAbsPosNP.setState(IPS_OK);
            FocusRelPosNP.setState(IPS_OK);
            FocusAbsPosNP.apply();
            FocusRelPosNP.apply();
            lastPos = FocusAbsPosNP[0].getValue();
            LOG_INFO("Focuser reached requested position.");
        }
    }

    SetTimer(getCurrentPollingPeriod());
}

bool Microtouch::AbortFocuser()
{
    WriteCmd(CMD_HALT);
    FocusAbsPosNP.setState(IPS_IDLE);
    FocusRelPosNP.setState(IPS_IDLE);
    FocusAbsPosNP.apply();
    FocusRelPosNP.apply();
    return true;
}

float Microtouch::CalcTimeLeft(timeval start, float req)
{
    double timesince;
    double timeleft;
    struct timeval now
    {
        0, 0
    };
    gettimeofday(&now, nullptr);

    timesince =
        (double)(now.tv_sec * 1000.0 + now.tv_usec / 1000) - (double)(start.tv_sec * 1000.0 + start.tv_usec / 1000);
    timesince = timesince / 1000;
    timeleft  = req - timesince;
    return timeleft;
}

bool Microtouch::WriteCmd(char cmd)
{
    int nbytes_written = 0, rc = -1;
    char errstr[MAXRBUF];

    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("WriteCmd : %02x ", cmd);

    if ((rc = tty_write(PortFD, &cmd, 1, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("WriteCmd error: %s.", errstr);
        return false;
    }
    return true;
}

bool Microtouch::WriteCmdGetResponse(char cmd, char *readbuffer, char numbytes)
{
    int nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];

    if (WriteCmd(cmd))
    {
        if ((rc = tty_read(PortFD, readbuffer, numbytes, MICROTOUCH_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(rc, errstr, MAXRBUF);
            LOGF_ERROR("WriteCmdGetResponse error: %s.", errstr);
            return false;
        }

        return true;
    }
    else
        return false;
}

char Microtouch::WriteCmdGetByte(char cmd)
{
    char read[2];

    if (WriteCmdGetResponse(cmd, read, 2))
    {
        LOGF_DEBUG("WriteCmdGetByte : %02x %02x ", read[0], read[1]);
        return read[1];
    }
    else
        return -1;
}

bool Microtouch::WriteCmdSetByte(char cmd, char val)
{
    int nbytes_written = 0, rc = -1;
    char errstr[MAXRBUF];
    char write_buffer[2];

    write_buffer[0] = cmd;
    write_buffer[1] = val;

    LOGF_DEBUG("WriteCmdSetByte : CMD %02x %02x ", write_buffer[0], write_buffer[1]);

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write(PortFD, write_buffer, 2, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("WriteCmdSetByte error: %s.", errstr);
        return false;
    }
    return true;
}

bool Microtouch::WriteCmdSetShortInt(char cmd, short int val)
{
    int nbytes_written = 0, rc = -1;
    char errstr[MAXRBUF];
    char write_buffer[3];

    write_buffer[0] = cmd;
    write_buffer[1] = val & 0xFF;
    write_buffer[2] = val >> 8;

    LOGF_DEBUG("WriteCmdSetShortInt : %02x %02x %02x ", write_buffer[0], write_buffer[1],
               write_buffer[2]);

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write(PortFD, write_buffer, 3, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("WriteCmdSetShortInt error: %s.", errstr);
        return false;
    }
    return true;
}

int Microtouch::WriteCmdGetInt(char cmd)
{
    char read[5];

    if (WriteCmdGetResponse(cmd, read, 5))
        return ((unsigned char)read[4] << 24 | (unsigned char)read[3] << 16 | (unsigned char)read[2] << 8 |
                (unsigned char)read[1]);
    else
        return -100000;
}

bool Microtouch::WriteCmdSetInt(char cmd, int val)
{
    int nbytes_written = 0, rc = -1;
    char errstr[MAXRBUF];
    char write_buffer[5];

    write_buffer[0] = cmd;
    write_buffer[1] = val & 0xFF;
    write_buffer[2] = val >> 8;
    write_buffer[3] = val >> 16;
    write_buffer[4] = val >> 24;

    LOGF_DEBUG("WriteCmdSetInt : %02x %02x %02x %02x %02x ", write_buffer[0], write_buffer[1],
               write_buffer[2], write_buffer[3], write_buffer[4]);

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write(PortFD, write_buffer, 5, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("WriteCmdSetInt error: %s.", errstr);
        return false;
    }
    return true;
}

bool Microtouch::WriteCmdSetIntAsDigits(char cmd, int val)
{
    int nbytes_written = 0, rc = -1;
    char errstr[MAXRBUF];
    char write_buffer[5];

    write_buffer[0] = cmd;
    write_buffer[1] = val % 10;
    write_buffer[2] = (val / 10) % 10;
    write_buffer[3] = (val / 100) % 10;
    write_buffer[4] = (val / 1000);

    LOGF_DEBUG("WriteCmdSetIntAsDigits : CMD (%02x %02x %02x %02x %02x) ", write_buffer[0],
               write_buffer[1], write_buffer[2], write_buffer[3], write_buffer[4]);

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write(PortFD, write_buffer, 5, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("WriteCmdSetIntAsDigits error: %s.", errstr);
        return false;
    }
    return true;
}
