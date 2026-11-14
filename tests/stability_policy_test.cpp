#include "RF/stabilityPolicy.h"
#include <assert.h>
#include <stdio.h>

using namespace YokisStability;

int main() {
    // Six consecutive failures; notify exactly once, then allow real recovery.
    uint8_t failures = 0;
    bool offline = false;
    unsigned notifications = 0;
    for (unsigned i = 1; i <= 1000; ++i) {
        failures = nextFailureCount(failures);
        if (shouldPublishOffline(failures, offline, true)) {
            offline = true;
            ++notifications;
            assert(i == 6);
        }
        assert(offline == (i >= 6));
    }
    assert(failures == 255 && notifications == 1);
    assert(!shouldRecover(false, true));  // TX alone cannot restore online.
    assert(shouldRecover(true, true));
    assert(!shouldRecover(true, false));
    // A valid response resets a streak: five failures, response, then six more.
    failures = 0;
    for (unsigned i = 0; i < 5; ++i) failures = nextFailureCount(failures);
    assert(!shouldPublishOffline(failures, false, true));
    failures = 0;
    for (unsigned i = 0; i < 5; ++i) failures = nextFailureCount(failures);
    assert(!shouldPublishOffline(failures, false, true));
    assert(shouldPublishOffline(nextFailureCount(failures), false, true));
    // Generic optional-response policy; NO_RCPT now uses expectsResponse=true.
    for (unsigned i = 0; i <= 255; ++i)
        assert(!shouldPublishOffline(static_cast<uint8_t>(i), false, false));

    for (unsigned type=0;type<256;++type)
        assert(canDecodeStatus(type) == (type==0 || type==1 || type==0x1e || type==0x2f));
    for (unsigned type=0;type<256;++type)
        assert(canDecodeStatus(type) == (type==0 || type==1 || type==0x1e || type==0x2f));
    assert(DEFAULT_POWER == 3 && rfSetup(DEFAULT_POWER) == 0x27);
    assert(rfSetup(1) == 0x23);
    assert(rfSetup(2) == 0x25 && rfSetup(3) == 0x27);
    for (uint8_t i = 1; i <= 3; ++i) assert((rfSetup(i) & ~0x06) == (0x23 & ~0x06));
    uint8_t power = 2;
    assert(parsePowerRequest("rfPower", power) == POWER_READ && power == 2);
    assert(parsePowerRequest("rfPower low", power) == POWER_SET && power == 1);
    assert(parsePowerRequest("rfPower high", power) == POWER_SET && power == 2);
    assert(parsePowerRequest("rfPower max", power) == POWER_SET && power == 3);
    assert(parsePowerRequest("rfPower  low  ", power) == POWER_SET && power == 1);
    const char* invalidPower[] = {NULL, "", "rfPowerX max", "rfPower LOW", "rfPower min",
                                  "rfPower max trailing", "rfPower 3", "rfPowe"};
    for (const char* text : invalidPower) {
        power = 2;
        assert(parsePowerRequest(text, power) == POWER_INVALID && power == 2);
    }

    char name[64], command[32];
    assert(parseCommandTopic("volet_salon/cmnd/COVER", name, sizeof(name), command, sizeof(command)));
    assert(strcmp(name, "volet_salon") == 0 && strcmp(command, "COVER") == 0);
    assert(parseCommandTopic("SDB/cmnd/POWER", name, sizeof(name), command, sizeof(command)));
    assert(strcmp(name, "SDB") == 0 && strcmp(command, "POWER") == 0);
    const char* invalidTopic[] = {NULL, "", "/cmnd/POWER", "SDB", "SDB/cmnd", "SDB/cmnd/",
                                 "SDB/tele/POWER", "SDB//POWER", "SDB/cmnd/POWER/extra"};
    for (const char* text : invalidTopic)
        assert(!parseCommandTopic(text, name, sizeof(name), command, sizeof(command)));
    struct Guarded { char before; char value[4]; char after; } guarded = {'a', {}, 'z'};
    assert(parseCommandTopic("SDB/cmnd/ON", guarded.value, sizeof(guarded.value), command, sizeof(command)));
    assert(!parseCommandTopic("longer/cmnd/ON", guarded.value, sizeof(guarded.value), command, sizeof(command)));
    assert(guarded.before == 'a' && guarded.after == 'z');
    assert(!parseCommandTopic("SDB/cmnd/POWER", name, 0, command, sizeof(command)));
    assert(!parseCommandTopic("SDB/cmnd/POWER", name, sizeof(name), command, 5));
    // Exercise arbitrary terminated input without reading/writing outside buffers.
    unsigned random = 1234567;
    const char alphabet[] = "SDB/cmndPOWERx_";
    for (unsigned i = 0; i < 10000; ++i) {
        char input[80];
        random = random * 1664525u + 1013904223u;
        unsigned length = random % (sizeof(input) - 1);
        for (unsigned j = 0; j < length; ++j) {
            random = random * 1664525u + 1013904223u;
            input[j] = alphabet[random % (sizeof(alphabet) - 1)];
        }
        input[length] = '\0';
        if (parseCommandTopic(input, name, sizeof(name), command, sizeof(command))) {
            char rebuilt[100];
            snprintf(rebuilt, sizeof(rebuilt), "%s/cmnd/%s", name, command);
            assert(strcmp(input, rebuilt) == 0);
        }
    }
    puts("PASS: threshold, single offline transition, recovery, saturation, NO_RCPT policy,");
    puts("power parsing/register bits, command topics, bounds, malformed-input stress.");
}
