// Modified in the alban-ops shutters/stability derivative (2026). See CHANGELOG.md and NOTICE.
#include <Arduino.h>

#include "RF/copy.h"
#include "RF/e2bp.h"
#include "RF/irqManager.h"
#include "RF/pairing.h"
#include "RF/scanner.h"
#include "commands/callbacks.h"
#include "globals.h"
#include "printf.h"
#include "serial/serialHelper.h"

#if defined(ESP8266) || defined(ESP32)
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <Ticker.h>
#include <WiFiUdp.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif

#if MQTT_ENABLED
#include "net/mqttHass.h"
#endif

#if WIFI_ENABLED
#include "net/wifi.h"
#endif

#if WEBSERVER_ENABLED
#include "net/webserver.h"
#endif

#endif

// static irqType initialization
IrqType IrqManager::irqType = PAIRING;

// globals' initialization
byte g_ConfigFlags = ~FLAG_RAW & ~FLAG_DEBUG & FLAG_POLLING;
SerialHelper* g_serial;
Pairing* g_pairingRF;
E2bp* g_bp;
Scanner* g_scanner;
Copy* g_copy;
Device* g_currentDevice;
uint8_t g_nb_devices;

#if defined(ESP8266) || defined(ESP32)
#if MQTT_ENABLED
MqttHass* g_mqtt;
#endif // MQTT_ENABLED
TelnetSpy g_telnetAndSerial;
// no need to store more devices than supported by MQTT
Device* g_devices[MAX_YOKIS_DEVICES_NUM];
#endif // ESP8266 || ESP32

//
#if defined(ESP8266) || defined(ESP32)

#if WIFI_ENABLED
WiFiClient espClient;
#endif
#if WEBSERVER_ENABLED
WebServer webserver(80);
#endif

Ticker* g_deviceStatusPollers[MAX_YOKIS_DEVICES_NUM];

// polling
void pollForStatus(Device* device);

#if MQTT_ENABLED
void mqttCallback(char*, uint8_t*, unsigned int);
#endif // MQTT_ENABLED

#endif // ESP8266 || ESP32

// Setup inits everything: singletons and commands' callback
void setup() {
    randomSeed(micros());

    // Globals' initialization
    g_serial = new SerialHelper();
    g_pairingRF = new Pairing(CE_PIN, CSN_PIN);
    g_bp = new E2bp(CE_PIN, CSN_PIN);
    g_scanner = new Scanner(CE_PIN, CSN_PIN);
    g_copy = new Copy(CE_PIN, CSN_PIN);
    g_currentDevice = new Device(CURRENT_DEVICE_DEFAULT_NAME);

#if defined(ESP8266) || defined(ESP32)
    pinMode(STATUS_LED, OUTPUT);
    digitalWrite(STATUS_LED, HIGH);  // pin is inverted so, set it off

    // Load all previously stored devices from LittleFS memory
    reloadConfig(NULL);

    // Setting up configured wifi or AP mode
    // If compilation options are present, override any existing configuration
    #if WIFI_ENABLED
        #ifdef WIFI_SSID
            // Check at runtime if WIFI_SSID is non-empty
            String ssid = WIFI_SSID;
            if (ssid.length() > 0) {
                String psk = "";
                #ifdef WIFI_PASSWORD
                psk = WIFI_PASSWORD;
                #endif // WIFI_PASSWORD
                LOG.print("WIFI_SSID is set, forcing this configuration. SSID=");
                LOG.println(WIFI_SSID);
                setupWifi(ssid, psk);
            } else {
                setupWifi(); // Setup existing configuration of set AP mode for initial config
            }
        #else
            setupWifi(); // Setup existing configuration of set AP mode for initial config
        #endif // WIFI_SSID

        #if WEBSERVER_ENABLED
        // Starting webserver
        webserver.begin();
        #endif

        #if MQTT_ENABLED
            g_mqtt = new MqttHass(espClient);
            g_mqtt->setCallback(mqttCallback);
        #endif
    #endif

    // OTA
    ArduinoOTA.onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH) {
            type = "sketch";
        } else {  // U_FS
            type = "filesystem";
        }
        LOG.println("Start updating " + type);
    });
    ArduinoOTA.onEnd([]() { LOG.println("\nEnd"); });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        LOG.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
        LOG.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) {
            LOG.println("Auth Failed");
        } else if (error == OTA_BEGIN_ERROR) {
            LOG.println("Begin Failed");
        } else if (error == OTA_CONNECT_ERROR) {
            LOG.println("Connect Failed");
        } else if (error == OTA_RECEIVE_ERROR) {
            LOG.println("Receive Failed");
        } else if (error == OTA_END_ERROR) {
            LOG.println("End Failed");
        }
    });
    ArduinoOTA.begin();
#endif

    // Registering all commands
    registerAllCallbacks();

    // Handle interrupt pin
    pinMode(IRQ_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(IRQ_PIN), IrqManager::processIRQ, FALLING);

    printf_begin();  // Works only for Arduino devices...
    LOG.println("Setup finished - device ready !");
    g_serial->executeCallback("help");
    LOG.println();
    g_serial->prompt();
}

void loop() {
#if defined(ESP8266) || defined(ESP32)
    LOG.handle(); // telnetspy handling
    ArduinoOTA.handle();

    #if MQTT_ENABLED
    g_mqtt->loop();

    if (g_mqtt->connected() && !g_mqtt->isDiscoveryDone()) {
        LOG.print("Publishing homeassistant discovery data... ");
        for (uint8_t i = 0; i < g_nb_devices; i++) {
            if (g_devices[i] != NULL) {
                if (g_mqtt->publishDevice(g_devices[i])) {
                    g_mqtt->subscribeDevice(g_devices[i]);
                } else {
                    LOG.println("KO");
                    break;
                }
            }
        }

        if (g_nb_devices == 0) g_mqtt->setDiscoveryDone(true);

        if (g_mqtt->isDiscoveryDone()) LOG.println("OK");

    } else if (g_mqtt->connected() && g_mqtt->isDiscoveryDone()) {
        // Verify polling statuses and update via MQTT if needed
        // At most one radio poll per loop: service MQTT and the console between polls.
        static uint8_t nextPoll = 0;
        for (uint8_t examined = 0;
             examined < g_nb_devices && FLAG_IS_ENABLED(FLAG_POLLING);
             examined++) {
            uint8_t i = (nextPoll + examined) % g_nb_devices;
            if (g_devices[i] != NULL && g_devices[i]->needsPolling()) {
                pollForStatus(g_devices[i]);
                nextPoll = (i + 1) % g_nb_devices;
                break;
            }
        }
    }
    else {
        // Disconnected, force discovery again next time
        g_mqtt->setDiscoveryDone(false);
    }
#endif // MQTT_ENABLED
#endif // ESP8266 || ESP32
    g_serial->readFromSerial();
    delay(1);
}


#if (defined(ESP8266) || defined(ESP32)) && MQTT_ENABLED
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
#endif

#if (defined(ESP8266) || defined(ESP32)) && MQTT_ENABLED
void mqttCallback(char* topic, uint8_t* payload, unsigned int length) {
    char name[MAX_COMMAND_FULL_LENGTH];
    char mCmnd[32];
    char mPayload[64];
    if (!YokisStability::parseCommandTopic(topic, name, sizeof(name),
                                          mCmnd, sizeof(mCmnd)) ||
        payload == NULL || length == 0 || length >= sizeof(mPayload) ||
        memchr(payload, '\0', length) != NULL) {
        LOG.println("Ignoring malformed MQTT command");
        return;
    }
    Device* d = Device::getFromList(g_devices, g_nb_devices, name);
    if (d == NULL) {
        LOG.println("Ignoring MQTT command for unknown device");
        return;
    }

    // If we update this device too soon, ignore the payload except for FX
    unsigned long now = millis();
    if (strcmp(mCmnd, "COVER") != 0 &&
        static_cast<unsigned long>(now - d->getLastUpdateMillis()) < MQTT_UPDATE_MILLIS_WINDOW &&
        strcmp(mCmnd, "FX") != 0) {
        LOG.println(
            "Ignoring MQTT message: received too soon for this device");
        LOG.print("Last update: ");
        LOG.println(d->getLastUpdateMillis(), DEC);
        LOG.print("This update: ");
        LOG.println(now, DEC);
        LOG.print("Difference: ");
        LOG.println(now - d->getLastUpdateMillis());

        return;
    }

    // Get payload
    memcpy(mPayload, payload, length);
    mPayload[length] = 0;

    // Processing MQTT message
    IrqManager::irqType = E2BP;
    g_bp->setDevice(d);
    // An ignored/invalid payload must not reuse the last transaction's response.
    g_bp->reset();
    switch (d->getMode()) {
        case SHUTTER:
        case SHUTTER_BUS:
            if (strcmp(mCmnd, "COVER") == 0) {
                if (strcmp(mPayload, "OPEN") == 0) g_bp->on();
                else if (strcmp(mPayload, "CLOSE") == 0) g_bp->off();
                else if (strcmp(mPayload, "STOP") == 0) g_bp->pauseShutter();
            }
            break;
        case ON_OFF:
        case NO_RCPT:
            if (strcmp(mPayload, "ON") == 0) {
                g_bp->on();
            } else if (strcmp(mPayload, "OFF") == 0) {
                g_bp->off();
            } else if (strcmp(mPayload, "PAUSE") == 0) {
                g_bp->pauseShutter();
            }
            if (d->getMode() != NO_RCPT || g_bp->getLastKnownDeviceStatus() != UNDEFINED)
                g_mqtt->notifyPower(d);
            break;
        case DIMMER:
            if (strcmp(mCmnd, "POWER") == 0 || strcmp(mCmnd,"BRIGHTNESS") == 0) {
                // When changing from effect Breath to None, HA sends both a FX change message and a brightness value (the original brightness value, not related to the current brightness of the bulb).
                // When breathing, we want to skip the brightness value so that the user can choose custom brightness using the breath effect (from Breath to None)
                // If we are in effect breathing, only take into account the power command.
                if (d->getDimmerEffect() != DimmerEffect::EFFECT_BREATHING || strcmp(mCmnd,"POWER") == 0) {
                    // brightness will be 0 for anything that is not a number
                    // so will set light to OFF for all possible POWER cases (ON OR
                    // OFF) HASS will send only POWER OFF, never POWER ON because
                    // on_command_type=brightness set on MQTT configuration (see
                    // MqttHass class)
                    int brightness = (uint8_t)atoi(mPayload);

                    switch (brightness) {
                        case BRIGHTNESS_OFF:
                            g_bp->off();
                            break;
                        case BRIGHTNESS_MIN:
                            g_bp->dimmerMin();
                            break;
                        case BRIGHTNESS_MID:
                            g_bp->dimmerMid();
                            break;
                        case BRIGHTNESS_MAX:
                        g_bp->dimmerMax();
                        break;
                        default:  // MAX values
                        g_bp->on();
                        break;
                    }

                    g_mqtt->notifyBrightness(d);
                } else {
                    LOG.println("Breathing, ignore brightness request");
                }
            } else if (strcmp(mCmnd, "FX") == 0) {
                if (strcmp(mPayload,"None") == 0) {
                    g_bp->dimmerEffectNone();
                } else if (strcmp(mPayload,"Breath") == 0) {
                    g_bp->dimmerEffectBreath();
                } else {
                    LOG.println("Effect not handled.");
                }
            } else {
                LOG.print("Command not handled :");
                LOG.println(mCmnd);
            }
            break;
    }

    g_mqtt->recoverFromResponse(d, g_bp->hasReceivedResponse());
}
#endif  // #if (defined(ESP8266) || defined(ESP32)) && defined(MQTT_ENABLED)
