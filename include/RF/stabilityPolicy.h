// Modified in the alban-ops shutters/stability derivative (2026). See CHANGELOG.md and NOTICE.
#ifndef YOKIS_STABILITY_POLICY_H
#define YOKIS_STABILITY_POLICY_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace YokisStability {
static const uint8_t OFFLINE_FAILURES = 6;
static const uint8_t DEFAULT_POWER = 3;  // PA_MAX, restored on every boot.

inline bool canDecodeStatus(uint8_t kind) {
    return kind == 0 || kind == 1 || kind == 0x1e || kind == 0x2f;
}

inline uint8_t nextFailureCount(uint8_t count) {
    return count == UINT8_MAX ? count : static_cast<uint8_t>(count + 1);
}

inline bool shouldPublishOffline(uint8_t failures, bool alreadyOffline,
                                 bool expectsResponse) {
    return expectsResponse && !alreadyOffline && failures >= OFFLINE_FAILURES;
}

inline bool shouldRecover(bool validResponse, bool alreadyOffline) {
    return validResponse && alreadyOffline;
}

inline uint8_t rfSetup(uint8_t power) {
    // Preserve 250 kbit/s and the existing low bit; change only RF_PWR.
    return static_cast<uint8_t>((0x23 & ~0x06) | ((power & 3) << 1));
}

enum PowerRequest { POWER_INVALID, POWER_READ, POWER_SET };
inline PowerRequest parsePowerRequest(const char* text, uint8_t& power) {
    if (!text || strncmp(text, "rfPower", 7) != 0) return POWER_INVALID;
    text += 7;
    if (*text != '\0' && *text != ' ') return POWER_INVALID;
    while (*text == ' ') ++text;
    if (*text == '\0') return POWER_READ;
    const char* end = text;
    while (*end && *end != ' ') ++end;
    size_t size = static_cast<size_t>(end - text);
    while (*end == ' ') ++end;
    if (*end) return POWER_INVALID;
    uint8_t parsed;
    if (size == 3 && strncmp(text, "low", 3) == 0) parsed = 1;
    else if (size == 4 && strncmp(text, "high", 4) == 0) parsed = 2;
    else if (size == 3 && strncmp(text, "max", 3) == 0) parsed = 3;
    else return POWER_INVALID;
    power = parsed;
    return POWER_SET;
}

inline bool parseCommandTopic(const char* topic, char* name, size_t nameCapacity,
                              char* command, size_t commandCapacity) {
    if (!topic || !name || !command || !nameCapacity || !commandCapacity) return false;
    const char* first = strchr(topic, '/');
    if (!first || first == topic) return false;
    if (strncmp(first, "/cmnd/", 6) != 0) return false;
    const char* verb = first + 6;
    if (!*verb || strchr(verb, '/')) return false;
    size_t nameSize = static_cast<size_t>(first - topic);
    size_t commandSize = strlen(verb);
    if (nameSize >= nameCapacity || commandSize >= commandCapacity) return false;
    memcpy(name, topic, nameSize);
    name[nameSize] = '\0';
    memcpy(command, verb, commandSize + 1);
    return true;
}
}  // namespace YokisStability
#endif
