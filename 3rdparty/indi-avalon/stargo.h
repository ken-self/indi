/*
    Avalon StarGo driver

    Copyright (C) 2019 Christopher Contaxis, Wolfgang Reissenberger,
    Ken Self and Tonino Tasselli

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

#ifndef STARGO_TELESCOPE_H
#define STARGO_TELESCOPE_H

//#pragma once

#include <indiguiderinterface.h>
#include <inditelescope.h>

#include <indicom.h>
#include <indilogger.h>
#include <termios.h>

#include <cstring>
#include <string>
#include <unistd.h>

#define STARGO_TIMEOUT 5 /* FD timeout in seconds */
#define RB_MAX_LEN    64
#define AVALON_TIMEOUT                                  2
#define AVALON_COMMAND_BUFFER_LENGTH                    32
#define AVALON_RESPONSE_BUFFER_LENGTH                   32
#define STARGO_GENERIC_SLEWRATE 5        /* slew rate, degrees/s */


enum TDirection
{
    STARGO_NORTH,
    STARGO_WEST,
    STARGO_EAST,
    STARGO_SOUTH,
    STARGO_ALL
};

// StarGo specific tabs
extern const char *RA_DEC_TAB;

class StarGoTelescope : public INDI::Telescope, public INDI::GuiderInterface
//class StarGoTelescope : public INDI::Telescope
{
public:
    enum TrackMode
    {
        TRACK_SIDEREAL=0, //=Telescope::TelescopeTrackMode::TRACK_SIDEREAL,
        TRACK_SOLAR=1, //=Telescope::TelescopeTrackMode::TRACK_SOLAR,
        TRACK_LUNAR=2, //=Telescope::TelescopeTrackMode::TRACK_LUNAR,
        TRACK_NONE=3
    };
    enum MotorsState
    {
        MOTORS_OFF=0,
        MOTORS_DEC_ONLY=1,
        MOTORS_RA_ONLY=2,
        MOTORS_ON=3
    };
    TrackMode CurrentTrackMode;
    MotorsState CurrentMotorsState;
    TelescopeSlewRate CurrentSlewRate;

    StarGoTelescope();

    virtual const char *getDefaultName() override;

    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual void ISGetProperties(const char *dev)override;

    virtual bool initProperties() override;
    virtual bool updateProperties() override;
    virtual bool Handshake() override;

protected:

    // Sync Home Position
    ISwitchVectorProperty SyncHomeSP;
    ISwitch SyncHomeS[1];

    // firmware info
    ITextVectorProperty MountFirmwareInfoTP;
    IText MountFirmwareInfoT[3] = {};
    
    // RA Track Adjust
    INumberVectorProperty TrackAdjustNP;
    INumber TrackAdjustN[1];

    // Gear ratios
    INumberVectorProperty GearRatioNP;
    INumber GearRatioN[2];

    // Max slew speed
    INumberVectorProperty MaxSlewNP;
    INumber MaxSlewN[2];

    // Motor Step Position
    INumberVectorProperty MotorStepNP;
    INumber MotorStepN[2];

    // goto home
    ISwitchVectorProperty MountGotoHomeSP;
    ISwitch MountGotoHomeS[1];

    // parking position
    ISwitchVectorProperty MountSetParkSP;
    ISwitch MountSetParkS[1];

    // guiding
    INumberVectorProperty GuidingSpeedNP;
    INumber GuidingSpeedN[2];

    ISwitchVectorProperty ST4StatusSP;
    ISwitch ST4StatusS[2];

    // meridian flip
    ISwitchVectorProperty MeridianFlipModeSP;
    ISwitch MeridianFlipModeS[3];

 /* Use pulse-guide commands */
//    ISwitchVectorProperty UsePulseCmdSP;
//    ISwitch UsePulseCmdS[2];
    bool usePulseCommand { true };

    bool sendTimeOnStartup=true, sendLocationOnStartup=true;
    uint8_t DBG_SCOPE;

    double targetRA, targetDEC;
    double currentRA, currentDEC;
    
    bool ParkOptionBusy { false };

    virtual bool ReadScopeStatus() override;
    virtual bool Goto(double ra, double dec) override;
    virtual bool Sync(double ra, double dec) override;
    virtual bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command) override;
    virtual bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command) override;
    virtual bool Park() override;
    virtual bool UnPark() override;
    virtual bool Abort() override;
    virtual bool SetTrackMode(uint8_t mode) override;
    virtual bool SetTrackRate(double raRate, double deRate) override;
    virtual bool SetTrackEnabled(bool enabled) override;
//    virtual bool updateTime(ln_date *utc, double utc_offset);
    virtual bool updateLocation(double latitude, double longitude, double elevation) override;
    virtual bool SetSlewRate(int index) override;

    virtual bool saveConfigItems(FILE *fp) override;

    virtual IPState GuideNorth(uint32_t ms) override;
    virtual IPState GuideSouth(uint32_t ms) override;
    virtual IPState GuideEast(uint32_t ms) override;
    virtual IPState GuideWest(uint32_t ms) override;
//    virtual void GuideComplete(INDI_EQ_AXIS axis) override;

    virtual bool SetParkPosition(double Axis1Value, double Axis2Value) override;
    virtual bool SetCurrentPark() override;
    virtual bool SetDefaultPark() override;

    // StarGo stuff
    void WaitParkOptionReady();
    bool isGuiding();
    bool setHomeSync();
    bool setParkPosition();
    bool setGotoHome();
    bool getParkHomeStatus (char* status);

    bool getScopeAlignmentStatus(char *mountType, bool *isTracking, int *alignmentPoints);
    bool setSlewMode(int slewMode);
    bool setObjectCoords(double ra, double dec);
    bool getEqCoordinates(double *ra, double *dec);

    // autoguiding
    bool setGuidingSpeeds(int raSpeed, int decSpeed);
    bool getGuidingSpeeds(int *raSpeed, int *decSpeed);
    bool getGearRatios(int *raRatio, int *decRatio);
    bool getMaxSlews(int *raSlew, int *decSlew);
    bool getMotorSteps(double *raSteps, double *decSteps);

    bool setST4Enabled(bool enabled);
    bool getST4Status(bool *isEnabled);
    int SendPulseCmd(int8_t direction, uint32_t duration_msec) ;

    // location
    bool sendScopeLocation();
    bool getSiteLatitude(double *siteLat);
    bool getSiteLongitude(double *siteLong);
    bool setSiteLatitude(double Lat);
    bool setSiteLongitude(double Long);

    bool setLocalSiderealTime(double longitude);
    bool setLocalDate(uint8_t days, uint8_t months, uint16_t years) ;
    bool setLocalTime24(uint8_t hour, uint8_t minute, uint8_t second) ;
    bool setUTCOffset(double offset) ;
    bool getLST_String(char* input);
    bool getLocalDate(char *dateString) ;
    bool getLocalTime(char *timeString) ;
    bool getUTCOffset(double *offset) ;
    bool sendScopeTime();

    // meridian flip
    bool SetMeridianFlipMode(int index);
    bool GetMeridianFlipMode(int *index);

    // scope status
    void getBasicData();
    bool getFirmwareInfo(char *version, char *mount, char *tcb );
    bool getMotorStatus(int *xSpeed, int *ySpeed);
    bool getSideOfPier();

// Simulate Mount in simulation mode
    void mountSim();

    // queries to the scope interface. Wait for specified end character
    bool sendQuery(const char* cmd, char* response, char end, int wait=AVALON_TIMEOUT);
    // Wait for default "#' character
    bool sendQuery(const char* cmd, char* response, int wait=AVALON_TIMEOUT);
    bool ParseMotionState(char* state);


    // helper functions
protected:
    bool receive(char* buffer, int* bytes, int wait=AVALON_TIMEOUT);
    bool receive(char* buffer, int* bytes, char end, int wait=AVALON_TIMEOUT);
    void flush();
    bool transmit(const char* buffer);
    double ahex2int(char* ahex);

};
inline bool StarGoTelescope::isGuiding(){
    return (GuideNSNP.s == IPS_BUSY || GuideWENP.s == IPS_BUSY);
}
inline bool StarGoTelescope::sendQuery(const char* cmd, char* response, int wait)
{
    return sendQuery(cmd, response, '#', wait);
}
inline bool StarGoTelescope::receive(char* buffer, int* bytes, int wait)
{
    return receive(buffer, bytes, '#', wait);
}
inline double StarGoTelescope::ahex2int(char* ahex)
{
    double val=0;
    for(unsigned int i=0; i<strlen(ahex); i++)
    {
        val = val*16 + ahex[i] - 48;
    }
    return val;
}

#endif // STARGO_TELESCOPE_H
