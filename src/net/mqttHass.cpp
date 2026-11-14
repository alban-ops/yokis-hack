// Modified in the alban-ops shutters/stability derivative (2026). See CHANGELOG.md and NOTICE.
#if WIFI_ENABLED && (defined(ESP8266) || defined(ESP32)) && MQTT_ENABLED
#include "net/mqttHass.h"
#include "RF/device.h"
#include "globals.h"

MqttHass::MqttHass(WiFiClient& wifiClient) : Mqtt(wifiClient) {
    MqttConfig config = MqttConfig::loadFromLittleFS();

    if(!config.isEmpty()) {
        this->setConnectionInfo(config, false);
    }
}

MqttHass::MqttHass(WiFiClient& wifiClient, const char* host,
                   const uint16_t port, const char* username,
                   const char* password)
    : Mqtt(wifiClient) {
        this->setConnectionInfo(host, port, username, password, false);
}

bool MqttHass::isDiscoveryDone() {
    return discoveryDone;
}

void MqttHass::setDiscoveryDone(bool status) {
    discoveryDone = status;
}

// Get JSON message to publish for HASS discovery
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

char* MqttHass::newPublishTopic(const Device* device, char* buf) {
    if (device->getMode() == SHUTTER || device->getMode() == SHUTTER_BUS)
        snprintf(buf, 128, "%s/cover/%s/config", HASS_PREFIX, device->getName());
    else
        snprintf(buf, 128, "%s/light/%s/config", HASS_PREFIX, device->getName());
    return buf;
}

// Publish device to MQTT for HASS discovery
bool MqttHass::publishDevice(const Device* device) {
    bool ret;
    char topic[128];
    char payload[MQTT_MAX_PACKET_SIZE];

    // Efface l'ancienne fausse entite 'light' du volet, uniquement pour un volet.
    if (device->getMode() == SHUTTER || device->getMode() == SHUTTER_BUS) {
        char oldTopic[128];
        snprintf(oldTopic, sizeof(oldTopic), "%s/light/%s/config", HASS_PREFIX, device->getName());
        this->publish(oldTopic, "", true);
    }
    newPublishTopic(device, topic);
    newMessageJson(device, payload);

    LOG.print("Sending MQTT message, topic_len=");
    LOG.print(strlen(topic));
    LOG.print(", payload_len=");
    LOG.println(strlen(payload));

    ret = this->publish(topic, payload, true);
    if (ret) {
        if (device->getAvailability() == OFFLINE) notifyOffline(device);
        else notifyOnline(device);
    }

    if(ret) {
        setDiscoveryDone(true);
    }

    return ret;
}

void MqttHass::notifyAvailability(const Device* device, const char* status) {
    char buf[64];

    sprintf(buf, "%s/tele/LWT", device->getName());
    publish(buf, status, true);
}

void MqttHass::notifyOnline(const Device* device) {
    notifyAvailability(device, "Online");
}

void MqttHass::notifyOffline(const Device* device) {
    notifyAvailability(device, "Offline");
}

void MqttHass::recoverFromResponse(Device* device, bool validResponse) {
    if (!validResponse || device == NULL) return;
    device->pollingSuccess();
    if (YokisStability::shouldRecover(validResponse, device->isOffline())) {
        device->online();
        notifyOnline(device);
    }
}

void MqttHass::notifyPower(const Device* device) {
    notifyPower(device, device->getStatus());
}

void MqttHass::notifyPower(const Device* device, DeviceStatus ds) {
    char buf[64];
    char bufPayload[64];

    sprintf(buf, "%s/tele/STATE", device->getName());
    sprintf(bufPayload, "{\"POWER\":\"%s\"}", Device::getStatusAsString(ds));
    publish(buf, bufPayload, false);
}

void MqttHass::notifyBrightness(const Device* device) {
    char buf[64];
    char bufPayload[64];

    notifyPower(device, (device->getBrightness() == BRIGHTNESS_OFF ? OFF : ON));

    sprintf(buf, "%s/tele/BRIGHTNESS", device->getName());
    sprintf(bufPayload, "{\"BRIGHTNESS\":\"%d\"}", device->getBrightness());
    publish(buf, bufPayload, false);
}

// Subscribe device to be able to be controlled over MQTT
void MqttHass::subscribeDevice(const Device* device) {
    char buf[64];

    if (device->getMode() == SHUTTER || device->getMode() == SHUTTER_BUS) {
        sprintf(buf, "%s/cmnd/COVER", device->getName());
        this->subscribe(buf);
    } else if (device->getMode() == ON_OFF || device->getMode() == NO_RCPT) {
        sprintf(buf, "%s/cmnd/POWER", device->getName());
        this->subscribe(buf);
    } else if (device->getMode() == DIMMER) {
        sprintf(buf, "%s/cmnd/POWER", device->getName());
        this->subscribe(buf);
        sprintf(buf, "%s/cmnd/BRIGHTNESS", device->getName());
        this->subscribe(buf);
        sprintf(buf, "%s/cmnd/FX", device->getName());
        this->subscribe(buf);
    }
}
#endif
