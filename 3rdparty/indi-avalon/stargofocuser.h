#ifndef STARGO_FOCUSER_H
#define STARGO_FOCUSER_H

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

#include "stargo.h"

#include "indifocuserinterface.h"
#include "defaultdevice.h"


class StarGoFocuser : public StarGoTelescope, public INDI::FocuserInterface
{
public:
    StarGoFocuser();
    virtual ~StarGoFocuser() = default;

    bool initProperties() override;
    bool updateProperties() override;
    bool ReadScopeStatus() override;

    bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);
    bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n);

protected:

    bool SetFocuserSpeed(int speed) override;
    IPState MoveFocuser(FocusDirection dir, int speed, uint16_t duration) override;
    IPState MoveAbsFocuser(uint32_t absolutePosition) override;
    IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;
//    bool ReverseFocuser(bool enabled) override;
    bool AbortFocuser() override;
    bool SyncFocuser(uint32_t ticks) override;
//    bool SetFocuserMaxPosition(uint32_t ticks) override;

    int targetFocuserPosition;
    bool startMovingFocuserInward;
    bool startMovingFocuserOutward;
    uint32_t moveFocuserDurationRemaining;

    // LX200 commands
    bool getFocuserPosition(int* position);

private:
    StarGoTelescope* baseDevice;
    const char* deviceName;

};

#endif // STARGO_FOCUSER_H
