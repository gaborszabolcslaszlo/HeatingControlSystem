#include "state_data_logger.h"

#include "app_globals.h"
#include "comon.h"
#include "HeatingSystemElements/HeatingElement.h"
#include "HeatingSystemElements/Sensor.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

StateDataLogger stateDataLogger;
constexpr float StateDataLogger::TEMPERATURE_CHANGE_THRESHOLD_C;
constexpr unsigned long StateDataLogger::MIN_CAPTURE_INTERVAL_MS;

namespace
{
constexpr uint8_t RECORD_SCHEMA = 1;
constexpr uint8_t RECORD_SAMPLE = 2;
constexpr uint8_t SAMPLE_ABSOLUTE_TIMESTAMP = 0;
constexpr uint8_t SAMPLE_DELTA_OF_DELTA = 1;

void appendFrame(File &file, uint8_t type, const std::vector<uint8_t> &payload)
{
    file.seek(file.size(), SeekSet);
    const uint16_t length = static_cast<uint16_t>(payload.size());
    const uint16_t crc = StateDataLogger::crc16(payload.data(), payload.size());
    file.write(type);
    file.write(static_cast<uint8_t>(length & 0xff));
    file.write(static_cast<uint8_t>((length >> 8) & 0xff));
    file.write(static_cast<uint8_t>(crc & 0xff));
    file.write(static_cast<uint8_t>((crc >> 8) & 0xff));
    if (length > 0)
    {
        file.write(payload.data(), length);
    }
    file.flush();
}

uint16_t readU16(const uint8_t *data)
{
    return static_cast<uint16_t>(data[0]) |
           static_cast<uint16_t>(data[1]) << 8;
}

uint32_t readU32(const uint8_t *data)
{
    return static_cast<uint32_t>(data[0]) |
           static_cast<uint32_t>(data[1]) << 8 |
           static_cast<uint32_t>(data[2]) << 16 |
           static_cast<uint32_t>(data[3]) << 24;
}

void appendSignedVarInt(std::vector<uint8_t> &buffer, int32_t value)
{
    uint32_t encoded = (static_cast<uint32_t>(value) << 1) ^
                       static_cast<uint32_t>(value >> 31);
    while (encoded >= 0x80)
    {
        buffer.push_back(static_cast<uint8_t>(encoded) | 0x80);
        encoded >>= 7;
    }
    buffer.push_back(static_cast<uint8_t>(encoded));
}

bool readSignedVarInt(const std::vector<uint8_t> &buffer, size_t &offset, int32_t &value)
{
    uint32_t encoded = 0;
    uint8_t shift = 0;
    while (offset < buffer.size() && shift <= 28)
    {
        const uint8_t byte = buffer[offset++];
        encoded |= static_cast<uint32_t>(byte & 0x7f) << shift;
        if ((byte & 0x80) == 0)
        {
            value = static_cast<int32_t>((encoded >> 1) ^
                                         static_cast<uint32_t>(-static_cast<int32_t>(encoded & 1)));
            return true;
        }
        shift += 7;
    }
    return false;
}

float readFloat(const uint8_t *data)
{
    float value;
    memcpy(&value, data, sizeof(value));
    return value;
}

void appendJsonString(String &output, const String &value)
{
    output += '"';
    for (size_t i = 0; i < value.length(); ++i)
    {
        const char c = value[i];
        if (c == '"' || c == '\\')
        {
            String escaped("\\");
            escaped += c;
            output += escaped;
        }
        else
        {
            String escaped(c);
            output += escaped;
        }
    }
    output += '"';
}
}

void StateDataLogger::begin()
{
    started = true;
    resetHistory();
}

void StateDataLogger::resetHistory()
{
    fields.clear();
    previousValues.clear();
    schemaSignature.clear();
    writtenSchemaSignature.clear();
    activeFile = "";
    previousTimestamp = 0;
    previousDelta = 0;
    hasTimestamp = false;
    lastCaptureMs = 0;
}

String StateDataLogger::currentDate() const
{
    return getFormattedDate();
}

String StateDataLogger::fileNameForDate(const String &date) const
{
    return String("/state_") + date + ".bin";
}

bool StateDataLogger::validDate(const String &date) const
{
    if (date.length() != 10 || date[4] != '-' || date[7] != '-')
    {
        return false;
    }
    for (size_t i = 0; i < date.length(); ++i)
    {
        if (i == 4 || i == 7)
            continue;
        if (date[i] < '0' || date[i] > '9')
            return false;
    }
    return true;
}

void StateDataLogger::collectFields()
{
    fields.clear();

    for (const auto &pair : IOMap)
    {
        Field field{};
        field.type = UINT8_VALUE;
        field.temperature = false;
        field.key = "io/" + std::to_string(pair.first);
        field.u8 = static_cast<uint8_t>(pair.second);
        fields.push_back(field);
    }

    for (const auto &pair : Sensor::SensorsValue)
    {
        Field field{};
        field.type = FLOAT32_VALUE;
        field.temperature = true;
        field.key = "sensor/" + pair.first;
        field.f32 = pair.second;
        fields.push_back(field);
    }

    for (const auto &element : HeatingElement::ElementsStateMap)
    {
        for (const auto &state : element.second)
        {
            Field field{};
            field.key = "state/" + element.first + "/" + state.first;
            field.temperature = false;

            if (state.first == "HT")
            {
                field.type = FLOAT32_VALUE;
                field.f32 = static_cast<float>(strtod(state.second.data(), nullptr));
            }
            else
            {
                field.type = UINT8_VALUE;
                field.u8 = stateCode(state.second.data());
            }
            fields.push_back(field);
        }
    }

    std::sort(fields.begin(), fields.end(), [](const Field &left, const Field &right)
              { return left.key < right.key; });

    std::string signature;
    uint16_t id = 1;
    for (Field &field : fields)
    {
        field.id = id++;
        signature += field.key;
        signature += static_cast<char>(field.type);
        signature += '|';
    }
    schemaSignature = signature;
}

void StateDataLogger::ensureFile()
{
    const String date = currentDate();
    const String fileName = fileNameForDate(date);
    if (activeFile == fileName && SPIFFS.exists(fileName))
    {
        return;
    }

    activeFile = fileName;
    File file = SPIFFS.open(activeFile, "a");
    if (!file)
    {
        logError("Binary state log megnyitasa sikertelen: %s\n", activeFile.c_str());
        return;
    }
    if (file.size() == 0)
    {
        const uint8_t header[] = {'H', 'L', 'O', 'G', 1, 0, 0, 0};
        file.write(header, sizeof(header));
        file.flush();
    }
    file.seek(file.size(), SeekSet);
    file.close();
}

void StateDataLogger::writeSchema()
{
    File file = SPIFFS.open(activeFile, "a");
    if (!file)
        return;

    std::vector<uint8_t> payload;
    appendU32(payload, 1); // schema version; records remain self-describing
    appendU16(payload, static_cast<uint16_t>(fields.size()));
    for (const Field &field : fields)
    {
        appendU16(payload, field.id);
        payload.push_back(static_cast<uint8_t>(field.type));
        payload.push_back(static_cast<uint8_t>(field.key.size()));
        payload.insert(payload.end(), field.key.begin(), field.key.end());
    }
    appendFrame(file, RECORD_SCHEMA, payload);
    file.close();
}

void StateDataLogger::writeSample(const std::vector<const Field *> &changed)
{
    if (changed.empty())
        return;

    const uint32_t timestamp = static_cast<uint32_t>(timeClient.getEpochTime());
    std::vector<uint8_t> payload;
    const uint8_t mode = hasTimestamp ? SAMPLE_DELTA_OF_DELTA : SAMPLE_ABSOLUTE_TIMESTAMP;
    payload.push_back(mode);

    if (!hasTimestamp)
    {
        appendU32(payload, timestamp);
    }
    else
    {
        const int32_t delta = static_cast<int32_t>(timestamp - previousTimestamp);
        appendSignedVarInt(payload, delta - previousDelta);
        previousDelta = delta;
    }
    appendU16(payload, static_cast<uint16_t>(changed.size()));

    for (const Field *field : changed)
    {
        appendU16(payload, field->id);
        payload.push_back(static_cast<uint8_t>(field->type));
        if (field->type == FLOAT32_VALUE)
        {
            const uint8_t *raw = reinterpret_cast<const uint8_t *>(&field->f32);
            payload.insert(payload.end(), raw, raw + sizeof(float));
        }
        else
        {
            payload.push_back(field->u8);
        }

        StoredValue value{};
        value.type = field->type;
        value.u8 = field->u8;
        value.f32 = field->f32;
        previousValues[field->id] = value;
    }

    File file = SPIFFS.open(activeFile, "a");
    if (file)
    {
        appendFrame(file, RECORD_SAMPLE, payload);
        file.close();
    }
    previousTimestamp = timestamp;
    hasTimestamp = true;
}

void StateDataLogger::capture()
{
    if (!started || millis() - lastCaptureMs < MIN_CAPTURE_INTERVAL_MS)
        return;
    lastCaptureMs = millis();

    const String date = currentDate();
    const String fileName = fileNameForDate(date);
    if (activeFile != fileName)
    {
        resetHistory();
        activeFile = fileName;
    }

    collectFields();
    ensureFile();
    if (schemaSignature.empty())
        return;
    if (writtenSchemaSignature != schemaSignature)
    {
        writeSchema();
        writtenSchemaSignature = schemaSignature;
        previousValues.clear();
    }

    std::vector<const Field *> changed;
    for (const Field &field : fields)
    {
        const auto old = previousValues.find(field.id);
        if (old == previousValues.end() || !valuesEqual(field, old->second))
        {
            changed.push_back(&field);
        }
    }
    writeSample(changed);
}

bool StateDataLogger::valuesEqual(const Field &field, const StoredValue &oldValue)
{
    if (field.type != oldValue.type)
        return false;
    if (field.type == FLOAT32_VALUE)
    {
        if (field.temperature)
            return fabsf(field.f32 - oldValue.f32) < TEMPERATURE_CHANGE_THRESHOLD_C;
        return field.f32 == oldValue.f32;
    }
    return field.u8 == oldValue.u8;
}

uint8_t StateDataLogger::stateCode(const char *value)
{
    static const char *const names[] = {
        "COOLD", "RETOUR_PROTECTION", "HEATING", "OVERHEATED", "PREHEAT_FROM_PUFFER",
        "CHARGE", "DISCHARGE", "FULL_CHARGED", "FULL_DISCHARGED", "STANDBY",
        "STATE_STANDBY", "STATE_WAITING_FOR_HEATING", "STATE_RECEIVING_HEATING",
        "STATE_FAULT", "CHARGING_BUFFER", "CHARGING_BUFFER_AND_HEATING",
        "DISCHARGING_BUFFER_AND_HEATING", "PREHEATING_BOILER", "FAULT", "true", "false"};
    for (uint8_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
    {
        if (strcmp(value, names[i]) == 0)
            return i + 1;
    }
    uint8_t hash = 0;
    for (const char *cursor = value; *cursor; ++cursor)
        hash = static_cast<uint8_t>(hash * 33U + static_cast<uint8_t>(*cursor));
    return hash == 0 ? 255 : hash;
}

const char *StateDataLogger::stateName(uint8_t code)
{
    static const char *const names[] = {
        "COOLD", "RETOUR_PROTECTION", "HEATING", "OVERHEATED", "PREHEAT_FROM_PUFFER",
        "CHARGE", "DISCHARGE", "FULL_CHARGED", "FULL_DISCHARGED", "STANDBY",
        "STATE_STANDBY", "STATE_WAITING_FOR_HEATING", "STATE_RECEIVING_HEATING",
        "STATE_FAULT", "CHARGING_BUFFER", "CHARGING_BUFFER_AND_HEATING",
        "DISCHARGING_BUFFER_AND_HEATING", "PREHEATING_BOILER", "FAULT", "true", "false"};
    return code >= 1 && code <= sizeof(names) / sizeof(names[0]) ? names[code - 1] : nullptr;
}

void StateDataLogger::handleGet()
{
    const String date = server.hasArg("date") ? server.arg("date") : currentDate();
    if (!validDate(date))
    {
        server.send(400, "application/json", "{\"error\":\"date must be YYYY-MM-DD\"}");
        return;
    }

    const String fileName = fileNameForDate(date);
    File file = SPIFFS.open(fileName, "r");
    if (!file)
    {
        String error = String("{\"error\":\"state log not found\",\"file\":\"") + fileName + "\"}";
        server.send(404, "application/json", error);
        return;
    }

    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    // send() only starts the response; sendContent() appends the complete JSON
    // stream. Passing '[' as the body would advertise a one-byte response on
    // some ESP8266 WebServer versions and Postman would show an empty body.
    server.send(200, "application/json", "");
    server.sendContent("[");
    bool firstSample = true;
    std::map<uint16_t, std::string> keyById;
    std::map<uint16_t, ValueType> typeById;
    uint32_t timestamp = 0;
    int32_t previousDeltaRead = 0;
    bool hasTimestampRead = false;

    uint8_t header[8];
    const size_t headerBytes = file.read(header, sizeof(header));
    if (headerBytes != sizeof(header))
    {
        file.close();
        server.sendContent("]");
        return;
    }
    // Older firmware versions could write the first schema record at offset 0
    // and therefore omit the HLOG header. Read those files as frame streams.
    if (memcmp(header, "HLOG", 4) != 0)
    {
        file.seek(0, SeekSet);
    }

    while (file.available())
    {
        uint8_t frameHeader[5];
        if (file.read(frameHeader, sizeof(frameHeader)) != sizeof(frameHeader))
            break;
        const uint8_t recordType = frameHeader[0];
        const uint16_t length = readU16(frameHeader + 1);
        const uint16_t expectedCrc = readU16(frameHeader + 3);
        std::vector<uint8_t> payload(length);
        if (file.read(payload.data(), length) != length || crc16(payload.data(), length) != expectedCrc)
            break;

        if (recordType == RECORD_SCHEMA && length >= 6)
        {
            size_t offset = 4;
            const uint16_t count = readU16(payload.data() + offset);
            offset += 2;
            keyById.clear();
            typeById.clear();
            for (uint16_t i = 0; i < count && offset + 4 <= length; ++i)
            {
                const uint16_t id = readU16(payload.data() + offset);
                const ValueType type = static_cast<ValueType>(payload[offset + 2]);
                const uint8_t keyLength = payload[offset + 3];
                offset += 4;
                if (offset + keyLength > length)
                    break;
                keyById[id] = std::string(reinterpret_cast<const char *>(payload.data() + offset), keyLength);
                typeById[id] = type;
                offset += keyLength;
            }
        }
        else if (recordType == RECORD_SAMPLE && length >= 7)
        {
            size_t offset = 0;
            const uint8_t mode = payload[offset++];
            if (mode == SAMPLE_ABSOLUTE_TIMESTAMP)
            {
                if (offset + 4 > length)
                    continue;
                timestamp = readU32(payload.data() + offset);
                offset += 4;
                previousDeltaRead = 0;
            }
            else
            {
                if (!hasTimestampRead)
                    continue;
                int32_t deltaOfDelta = 0;
                if (!readSignedVarInt(payload, offset, deltaOfDelta))
                    continue;
                const int32_t delta = previousDeltaRead + deltaOfDelta;
                timestamp = static_cast<uint32_t>(timestamp + delta);
                previousDeltaRead = delta;
            }
            hasTimestampRead = true;
            const uint16_t count = readU16(payload.data() + offset);
            offset += 2;

            String sampleJson;
            if (!firstSample)
                sampleJson += ',';
            firstSample = false;
            sampleJson += String("{\"timestamp\":") + String(timestamp) + ",\"changes\":{";
            bool firstChange = true;
            for (uint16_t i = 0; i < count && offset + 3 <= length; ++i)
            {
                const uint16_t id = readU16(payload.data() + offset);
                const ValueType type = static_cast<ValueType>(payload[offset + 2]);
                offset += 3;
                auto key = keyById.find(id);
                if (key == keyById.end())
                    break;
                if (!firstChange)
                    sampleJson += ',';
                firstChange = false;
                appendJsonString(sampleJson, String(key->second.c_str()));
                sampleJson += ':';
                if (type == FLOAT32_VALUE && offset + 4 <= length)
                {
                    sampleJson += String(readFloat(payload.data() + offset), 4);
                    offset += 4;
                }
                else if (type == UINT8_VALUE && offset < length)
                {
                    const uint8_t value = payload[offset++];
                    const char *name = stateName(value);
                    if (key->second.find("state/") == 0 && name != nullptr)
                    {
                        appendJsonString(sampleJson, String(name));
                    }
                    else
                    {
                        sampleJson += String(value);
                    }
                }
            }
            sampleJson += "}}";
            server.sendContent(sampleJson);
        }
    }
    file.close();
    server.sendContent("]");
}

void StateDataLogger::handleDelete()
{
    const String date = server.hasArg("date") ? server.arg("date") : "";
    if (!validDate(date))
    {
        server.send(400, "application/json", "{\"error\":\"date must be YYYY-MM-DD\"}");
        return;
    }
    const String fileName = fileNameForDate(date);
    if (!SPIFFS.exists(fileName) || !SPIFFS.remove(fileName))
    {
        server.send(404, "application/json", "{\"error\":\"state log not found\"}");
        return;
    }
    if (activeFile == fileName)
        resetHistory();
    server.send(200, "application/json", "{\"status\":\"deleted\"}");
}

void StateDataLogger::handleList()
{
    String json = "{\"currentDate\":\"" + currentDate() + "\",\"files\":[";
    bool first = true;

#ifdef ESP32
    File root = SPIFFS.open("/");
    File entry = root.openNextFile();
    while (entry)
    {
        const String name = entry.name();
        if ((name.startsWith("/state_") || name.startsWith("state_")) && name.endsWith(".bin"))
        {
            if (!first)
                json += ",";
            first = false;
            json += "{\"name\":\"" + name + "\",\"size\":" + String(entry.size()) + "}";
        }
        entry = root.openNextFile();
    }
#else
    Dir directory = SPIFFS.openDir("/");
    while (directory.next())
    {
        const String name = directory.fileName();
        if ((name.startsWith("/state_") || name.startsWith("state_")) && name.endsWith(".bin"))
        {
            if (!first)
                json += ",";
            first = false;
            json += "{\"name\":\"" + name + "\",\"size\":" + String(directory.fileSize()) + "}";
        }
    }
#endif

    json += "]}";
    server.send(200, "application/json", json);
}

uint16_t StateDataLogger::crc16(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xffff;
    while (length--)
    {
        crc ^= *data++;
        for (uint8_t bit = 0; bit < 8; ++bit)
            crc = (crc & 1) ? static_cast<uint16_t>((crc >> 1) ^ 0xa001) : static_cast<uint16_t>(crc >> 1);
    }
    return crc;
}

void StateDataLogger::appendU16(std::vector<uint8_t> &buffer, uint16_t value)
{
    buffer.push_back(static_cast<uint8_t>(value & 0xff));
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
}

void StateDataLogger::appendU32(std::vector<uint8_t> &buffer, uint32_t value)
{
    buffer.push_back(static_cast<uint8_t>(value & 0xff));
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
    buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xff));
    buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xff));
}

