// Modified in the alban-ops shutters/stability derivative (2026). See CHANGELOG.md and NOTICE.
#ifndef __E2BP_H__
#define __E2BP_H__

#include <Arduino.h>
#include <nRF24L01.h>
#include "RF/RF24_forked.h"
#include "RF/configurator.h"
#include "RF/device.h"
#include "RF/stabilityPolicy.h"

#define PAYLOAD_LENGTH 9
#define MAIN_LOOP_TIMEOUT_MILLIS 500

enum PayloadType {
    PL_BEGIN = 0,
    PL_END,
    PL_STATUS,
    PL_ON,
    PL_OFF,
    PL_DIM,
    PL_SHUTTERPAUSE
};

// Very low level class to control a Yokis device just like a e2bp remote
class E2bp : public RFConfigurator {
   private:
    Device *device;
    bool loopContinue;
    bool responseReceived = false;
    bool statusQuery = false;
    DeviceStatus firstPayloadStatus, secondPayloadStatus;
    char answerBuf[2];
    uint8_t powerLevel = YokisStability::DEFAULT_POWER;
    volatile uint16_t txEvents = 0, rxEvents = 0, txFailedEvents = 0;

    bool setDeviceStatus(DeviceStatus);
    uint8_t *getPayload(uint8_t *, PayloadType);
    bool press(bool dim);

   protected:
    bool runMainLoop();
    void setupPayload(const uint8_t *);

   protected:
    bool sendPayload(const uint8_t *);
    void stopMainLoop();

   public:
    // uint8_t counter = 0;
    // Constructor
    E2bp(uint16_t, uint16_t);
    ~E2bp();
    void reset();
    void setupRFModule() override;
    bool on();
    bool off();
    bool pauseShutter();
    bool toggle();
    // See Yokis MTV500ER manual for those configs
    // Note: depending on configuration, 2 pulses can set to "memory" or 100%
    // default is 100% for 2 pulses, that's what we will be using here...
    bool dimmerSet(const uint8_t);
    // Set dimmer to "mem" Light
    bool dimmerMem();
    // Set dimmer to max light
    bool dimmerMax();
    // Set dimmer to middle light
    bool dimmerMid();
    // Set dimmer to min light
    bool dimmerMin();
    // Set dimmer to "night light"
    bool dimmerNiL();

    // Set dimmer effect 
    bool dimmerEffectBreath();
    bool dimmerEffectNone();

    // press the button
    bool press();
    // release the button
    bool release();
    // press and hold for a specific duration (ms)
    bool pressAndHoldFor(unsigned long);
    DeviceStatus getLastKnownDeviceStatus();
    // Get the device status (ON or OFF) - experimental !
    DeviceStatus pollForStatus();
    bool hasReceivedResponse() const { return responseReceived; }
    DeviceMode getDeviceModeFromRecvData();
    void interruptTxOk() override;
    void interruptRxReady() override;
    void interruptTxFailed() override;

    // Getters / setters
    void setDevice(Device *);
    const Device *getDevice();
    uint8_t getPowerLevel() const { return powerLevel; }
    void setPowerLevel(uint8_t level) { if (level >= 1 && level <= 3) powerLevel = level; }
};

#endif  // __E2BP_H__
