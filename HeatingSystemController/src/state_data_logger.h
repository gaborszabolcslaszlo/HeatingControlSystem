#pragma once

#include "platform_compat.h"
#include <FS.h>
#include <map>
#include <string>
#include <vector>

#ifndef STATE_LOG_TEMP_THRESHOLD_C
#define STATE_LOG_TEMP_THRESHOLD_C 0.1f
#endif

#ifndef STATE_LOG_MIN_CAPTURE_INTERVAL_MS
#define STATE_LOG_MIN_CAPTURE_INTERVAL_MS 1000UL
#endif

// Binary state history. Values are encoded as uint8_t or IEEE-754 float32;
// field names are stored only in schema records, never in every sample.
class StateDataLogger
{
public:
    static constexpr float TEMPERATURE_CHANGE_THRESHOLD_C = STATE_LOG_TEMP_THRESHOLD_C;
    static constexpr unsigned long MIN_CAPTURE_INTERVAL_MS = STATE_LOG_MIN_CAPTURE_INTERVAL_MS;

    void begin();
    void capture();

    void handleGet();
    void handleDelete();
    void handleList();

    static uint16_t crc16(const uint8_t *data, size_t length);

private:
    enum ValueType : uint8_t
    {
        UINT8_VALUE = 1,
        FLOAT32_VALUE = 2
    };

    struct Field
    {
        uint16_t id;
        ValueType type;
        bool temperature;
        std::string key;
        uint8_t u8;
        float f32;
    };

    struct StoredValue
    {
        ValueType type;
        uint8_t u8;
        float f32;
    };

    std::vector<Field> fields;
    std::map<uint16_t, StoredValue> previousValues;
    std::string schemaSignature;
    std::string writtenSchemaSignature;
    String activeFile;
    unsigned long lastCaptureMs = 0;
    uint32_t previousTimestamp = 0;
    int32_t previousDelta = 0;
    bool hasTimestamp = false;
    bool started = false;

    void collectFields();
    void ensureFile();
    void writeSchema();
    void writeSample(const std::vector<const Field *> &changed);
    String fileNameForDate(const String &date) const;
    String currentDate() const;
    bool validDate(const String &date) const;
    void resetHistory();

    static uint8_t stateCode(const char *value);
    static const char *stateName(uint8_t code);
    static bool valuesEqual(const Field &field, const StoredValue &oldValue);
    static void appendU16(std::vector<uint8_t> &buffer, uint16_t value);
    static void appendU32(std::vector<uint8_t> &buffer, uint32_t value);
};

extern StateDataLogger stateDataLogger;
