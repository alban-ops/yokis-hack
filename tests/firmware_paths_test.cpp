// Generated from the actual candidate functions; hardware is mocked.

#include "RF/stabilityPolicy.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define MQTT_MAX_PACKET_SIZE 1024
#define DEVICE_MAX_FAILED_POLLING_BEFORE_OFFLINE YokisStability::OFFLINE_FAILURES
#define DEC 10
enum DeviceMode { ON_OFF=0, DIMMER, NO_RCPT, SHUTTER, SHUTTER_BUS };
enum DeviceStatus { OFF=0, ON, UNDEFINED, PAUSE_SHUTTER };
enum DeviceAvailability { OFFLINE=0, ONLINE };
enum DimmerBrightness { BRIGHTNESS_OFF=0, BRIGHTNESS_MIN=1, BRIGHTNESS_MID=2, BRIGHTNESS_MAX=4 };
enum IrqType { E2BP };
struct IrqManager { static IrqType irqType; }; IrqType IrqManager::irqType=E2BP;
struct SilentLog {
 template<class T> void print(T) {} template<class T> void println(T) {}
 void print(uint8_t, int) {} void println(uint8_t, int) {}
} LOG;
class Device {
 public:
  DeviceMode mode=ON_OFF;
  DeviceStatus status=OFF;
  DeviceAvailability availability=ONLINE;
  DimmerBrightness brightness=BRIGHTNESS_OFF;
  uint8_t failedPolls=0;
  bool hasToBePolledForStatus=true;
  const char* name="SDB";
  const char* getName() const {return name;}
  DeviceMode getMode() const {return mode;}
  uint8_t getFailedPollings() {return failedPolls;}
  bool isOffline() {return availability==OFFLINE;}
  void online() {availability=ONLINE;}
  void offline() {availability=OFFLINE;status=UNDEFINED;}
  void setStatus(DeviceStatus value) {status=value;}
  DimmerBrightness getBrightness() const {return brightness;}
  void setBrightness(DimmerBrightness value) {brightness=value;}
  void pollMePlease() {hasToBePolledForStatus=true;} void pollingSuccess(); uint8_t pollingFailed();
};
struct E2bp {
 DeviceStatus result=UNDEFINED; bool received=false; unsigned calls=0;
 bool hasReceivedResponse() const {return received;} void setDevice(Device*) {} DeviceStatus pollForStatus(){++calls;return result;}
} radio;
E2bp* g_bp=&radio;
struct MqttHass {
 unsigned onlineCount=0,offlineCount=0,powerCount=0;
 void notifyOnline(const Device*) {++onlineCount;}
 void notifyOffline(const Device*) {++offlineCount;}
 void notifyPower(const Device*) {++powerCount;}
 void notifyBrightness(const Device*) {++powerCount;}
 void recoverFromResponse(Device*,bool);
 static void gatewayAvailabilityTopic(char* out,size_t size) {
   snprintf(out,size,"yokis_hack/TEST/tele/LWT");
 }
 char* newMessageJson(const Device*,char*);
} mqtt;
MqttHass* g_mqtt=&mqtt;
bool flagsEnabled=true;
#define FLAG_POLLING 1
#define FLAG_IS_ENABLED(x) flagsEnabled
void Device::pollingSuccess() {
    this->hasToBePolledForStatus = false;
    this->failedPolls = 0;
}
uint8_t Device::pollingFailed() {
    this->hasToBePolledForStatus = false;
    failedPolls = YokisStability::nextFailureCount(failedPolls);
    return failedPolls;
}
void MqttHass::recoverFromResponse(Device* device, bool validResponse) {
    if (!validResponse || device == NULL) return;
    device->pollingSuccess();
    if (YokisStability::shouldRecover(validResponse, device->isOffline())) {
        device->online();
        notifyOnline(device);
    }
}
void pollForStatus(Device* d) {
    IrqManager::irqType = E2BP;
    g_bp->setDevice(d);
    DeviceStatus ds = g_bp->pollForStatus();

    if (g_bp->hasReceivedResponse()) {  // RX from the addressed module, never TX_OK alone
        if (d->getFailedPollings() > 0) {
            LOG.print("Device ");
            LOG.print(d->getName());
            LOG.println(" recovered");
        }

        d->pollingSuccess();

        g_mqtt->recoverFromResponse(d, true);

        // A received but undecoded answer proves presence, not ON/OFF.
        if (ds == UNDEFINED) {
            d->setStatus(UNDEFINED);
            LOG.print("Device "); LOG.print(d->getName());
            LOG.println(" replied; status undecoded");
            return;
        }
        // Update device status - even if unchanged
        // Hence, in case of hass restart, status are updated
        d->setStatus(ds);
        if (d->getMode() == DIMMER) {
            if (ds == ON && d->getBrightness() == 0)
                d->setBrightness(BRIGHTNESS_MAX);
            g_mqtt->notifyBrightness(d);
        } else {
            g_mqtt->notifyPower(d);
        }
    } else {
        uint8_t failures = d->pollingFailed();
        if (YokisStability::shouldPublishOffline(failures, d->isOffline(), true)) {
            // Device is unreachable
            LOG.print("Device ");
            LOG.print(d->getName());
            LOG.println(" is offline");
            d->offline();
            g_mqtt->notifyOffline(d);
        } else if (!d->isOffline()) {
            LOG.print("Failed to check device ");
            LOG.print(d->getName());
            LOG.print(" ");
            LOG.print(d->getFailedPollings(), DEC);
            LOG.print("/");
            LOG.println(DEVICE_MAX_FAILED_POLLING_BEFORE_OFFLINE, DEC);
        }
    }
}
void pollDevice(Device* d) {
    // Every configured device, including NO_RCPT, is supervised.
    if (d != NULL && FLAG_IS_ENABLED(FLAG_POLLING))
        d->pollMePlease();
}
char* MqttHass::newMessageJson(const Device* device, char* buf) {
    if (device->getMode() == DIMMER) {
        sprintf(buf,
                "{"
                "\"name\":\"Dimmer\","
                "\"optimistic\":\"false\","  // if false, cannot know the status
                                             // of the device
                "\"on_command_type\":\"brightness\","  // only send brightness
                "\"bri_cmd_t\":\"~cmnd/BRIGHTNESS\","
                "\"bri_scl\":\"4\","  // 0=toggle, 1=min, 2=mid, 3/4=max
                "\"bri_stat_t\":\"~tele/BRIGHTNESS\","
                "\"bri_val_tpl\":\"{{value_json.BRIGHTNESS}}\","
                "\"fx_cmd_t\":\"~cmnd/FX\","
                "\"fx_stat_t\":\"~tele/FX\","
                "\"fx_val_tpl\":\"{{value_json.FX}}\","
                "\"fx_list\": [\"None\",\"Breath\"],"
                "\"cmd_t\":\"~cmnd/POWER\","
                "\"pl_off\":\"OFF\","
                "\"state_topic\":\"~tele/STATE\","
                "\"state_value_template\":\"{{value_json.POWER}}\","
                "\"avty_t\":\"~tele/LWT\","
                "\"pl_avail\":\"Online\","
                "\"pl_not_avail\":\"Offline\","
                "\"uniq_id\":\"esp-%s\","
                "\"device\":{"
                  "\"name\":\"%s\","
                  "\"identifiers\":[\"yokis-%s\"],"
                  "\"model\":\"MTV500ERX\","
                  "\"mf\":\"Yokis\""
                "},"
                "\"~\":\"%s/\""
                "}",
                device->getName(), device->getName(),
                device->getName(), device->getName());
    } else if (device->getMode() == SHUTTER || device->getMode() == SHUTTER_BUS) {
        // Le MVR500 fonctionne en OPEN/CLOSE/STOP, sans retour de position fiable.
        // Pas de state_topic: Home Assistant affiche un etat optimiste.
        snprintf(buf, MQTT_MAX_PACKET_SIZE,
                 R"json({"name":"Volet","device_class":"shutter","command_topic":"~cmnd/COVER","payload_open":"OPEN","payload_close":"CLOSE","payload_stop":"STOP","availability_topic":"~tele/LWT","payload_available":"Online","payload_not_available":"Offline","unique_id":"esp-%s","device":{"name":"%s","identifiers":["yokis-%s"],"model":"MVR500ERX","manufacturer":"Yokis"},"~":"%s/"})json",
                 device->getName(), device->getName(),
                 device->getName(), device->getName());
    } else {
        sprintf(buf,
                "{"
                "\"name\":\"Switch\","
                "\"optimistic\":\"false\","  // if false, cannot know the status
                                             // of the device
                "\"cmd_t\":\"~cmnd/POWER\","
                "\"state_topic\":\"~tele/STATE\","
                "\"state_value_template\":\"{{value_json.POWER}}\","
                "\"pl_off\":\"OFF\","
                "\"pl_on\":\"ON\","
                "\"avty_t\":\"~tele/LWT\","
                "\"pl_avail\":\"Online\","
                "\"pl_not_avail\":\"Offline\","
                "\"uniq_id\":\"esp-%s\","
                "\"device\":{"
                  "\"name\":\"%s\","
                  "\"identifiers\":[\"yokis-%s\"],"
                  "\"model\":\"MTR2000ERX\","
                  "\"mf\":\"Yokis\""
                "},"
                "\"~\":\"%s/\""
                "}",
                device->getName(), device->getName(),
                device->getName(), device->getName());
    }

    return buf;
}
int main() {
 Device d;
 for(unsigned i=1;i<=5;++i){pollForStatus(&d); assert(d.failedPolls==i);assert(!d.isOffline());}
 assert(mqtt.offlineCount==0);
 pollForStatus(&d);assert(d.isOffline());assert(d.status==UNDEFINED);assert(mqtt.offlineCount==1);
 for(unsigned i=0;i<1000;++i)pollForStatus(&d);
 assert(mqtt.offlineCount==1);assert(d.failedPolls==255);
 mqtt.recoverFromResponse(&d,false);assert(d.isOffline());assert(d.failedPolls==255);
 mqtt.recoverFromResponse(&d,true);assert(!d.isOffline());assert(d.failedPolls==0);assert(mqtt.onlineCount==1);
 mqtt.recoverFromResponse(&d,true);assert(mqtt.onlineCount==1);
 for(unsigned i=0;i<6;++i)pollForStatus(&d);
 assert(mqtt.offlineCount==2 && d.isOffline());
 radio.result=ON;radio.received=true;pollForStatus(&d);
 assert(!d.isOffline() && d.failedPolls==0 && d.status==ON && mqtt.onlineCount==2);
 // NO_RCPT uses its own real radio supervision and the same six-failure threshold.
 d.mode=NO_RCPT;radio.result=UNDEFINED;radio.received=false;
 unsigned calls=radio.calls;
 for(unsigned i=0;i<5;++i)pollForStatus(&d);
 assert(radio.calls==calls+5 && !d.isOffline());
 pollForStatus(&d);assert(d.isOffline() && mqtt.offlineCount==3);
 for(unsigned i=0;i<10;++i)pollForStatus(&d);
 assert(mqtt.offlineCount==3);
 // A real reply with unrecognised status recovers availability, but publishes no power.
 unsigned powerBefore=mqtt.powerCount;
 radio.received=true;pollForStatus(&d);
 assert(!d.isOffline() && d.failedPolls==0 && d.status==UNDEFINED);
 assert(mqtt.onlineCount==3 && mqtt.powerCount==powerBefore);
 // Recognised status is published and can follow either ON or OFF.
 radio.result=ON;pollForStatus(&d);assert(d.status==ON);
 radio.result=OFF;pollForStatus(&d);assert(d.status==OFF);
 // Scheduling NO_RCPT obeys the polling flag just like all other modes.
 for(unsigned mode=0;mode<5;++mode) {
   d.mode=static_cast<DeviceMode>(mode);d.hasToBePolledForStatus=false;
   flagsEnabled=true;pollDevice(&d);assert(d.hasToBePolledForStatus);
   d.hasToBePolledForStatus=false;flagsEnabled=false;pollDevice(&d);
   assert(!d.hasToBePolledForStatus);
 }
 // Dump the actual generated discovery JSON for all five modes.
 char json[MQTT_MAX_PACKET_SIZE];
 for(unsigned mode=0;mode<5;++mode) {
   d.mode=static_cast<DeviceMode>(mode);
   mqtt.newMessageJson(&d,json);
   printf("%s\n",json);
 }
 fprintf(stderr,"PASS: actual polling/recovery paths and all discovery modes\n");
}
