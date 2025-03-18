#include "DaliGateway.h"

void DaliGateway::setup()
{
    webSocket.begin();
    printf("WebSocket server started\n");
    webSocket.onEvent([this](uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
        this->webSocketEvent(num, type, payload, length);
    });
}

void DaliGateway::loop()
{
    webSocket.loop();
}

void DaliGateway::addMaster(Dali::Master *master)
{
    uint8_t index = masters.size();
    printf("Adding master %d\n", index);
    masters.push_back(master);
    master->registerMonitor([this, index](Dali::Frame frame) {
        this->receivedMonitor(index, frame);
    });
}

void DaliGateway::webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
    switch (type)
    {
    case WStype_CONNECTED:
    {
        printf("WebSocket connected\n");
        JsonDocument doc;
        doc["type"] = "info";
        doc["data"]["name"] = "dali-iot";
        doc["data"]["version"] = "v1.2.0/1.0.9";
        doc["data"]["tier"] = "plus";
        doc["data"]["emergencyLight"] = false;
        doc["data"]["errors"] = JsonObject();
        doc["data"]["descriptor"]["lines"] = masters.size();
        doc["data"]["descriptor"]["bufferSize"] = 32;
        doc["data"]["descriptor"]["tickResolution"] = 1978; // TODO what does this?
        doc["data"]["descriptor"]["maxYnFrameSize"] = 32;
        doc["data"]["descriptor"]["deviceListSpecifier"] = true;
        doc["data"]["descriptor"]["protocolVersion"] = "1.0";
        doc["data"]["device"]["serial"] = 1234567890;
        doc["data"]["device"]["gtin"] = 1234567890;
        doc["data"]["device"]["pcb"] = "9a";
        doc["data"]["device"]["articleNumber"] = 1234567890;
        doc["data"]["device"]["articleInfo"] = "";
        doc["data"]["device"]["productionYear"] = 2024;
        doc["data"]["device"]["productionWeek"] = 31;

        counter = 0;
        sendJson(doc);
        break;
    }

    case WStype_PING:
        // pong will be send automatically
        // Serial.println("[WSc] get ping");
        break;

    case WStype_PONG:
        // answer to a ping we send
        // Serial.println("[WSc] get pong");
        break;

    case WStype_DISCONNECTED:
        printf("WebSocket disconnected\n");
        // webSocket.close();
        // webSocket.disconnect();
        break;

    case WStype_TEXT:
    {
        printf("WebSocket text\n");
        printf("Received: %s\n", payload);
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);
        if (error)
        {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.c_str());
            return;
        }
        printf("Received: %s\n", doc["type"].as<String>());
        handleData(0, doc);
        break;
    }

    case WStype_BIN:
        Serial.printf("[%u] Binärdaten empfangen\n", num);
        break;

    default:
        Serial.printf("Unhandled WebSocket event: %d\n", type);
        break;
    }
}

void DaliGateway::handleData(uint8_t num, JsonDocument &doc)
{
    uint8_t line = doc["data"]["line"];

    // {"data":
    //     {"daliData":[255,6],"line":0,"mode":
    //         {"priority":4,"sendTwice":false,"waitForAnswer":false},
    //     "numberOfBits":16},
    // "type":"daliFrame"}
    if (doc["type"] != "daliFrame")
        return; // we only handle dali frames

    if(doc["data"]["line"] >= masters.size()) {
        sendAnswer(num, 5, 0);
        return;
    }

    Dali::Frame frame;
    frame.size = doc["data"]["numberOfBits"];
    frame.flags = DALI_FRAME_FORWARD;

    uint32_t ref = masters[line]->sendRaw(frame);
    sent.push_back(ref);
}

void DaliGateway::receivedMonitor(uint8_t line, Dali::Frame frame)
{
    if(frame.flags & DALI_FRAME_ECHO && frame.flags & DALI_FRAME_FORWARD)
    {
        for(uint32_t &r : sent)
        {
            if(r == frame.ref)
            {
                JsonDocument sent;
                sent["type"] = "daliFrame";
                sent["data"]["line"] = line;
                sent["data"]["result"] = 0;
                sent.remove(r);
                break;
            }
        }
    }

    JsonDocument doc;
    doc["type"] = "daliMonitor";
    doc["data"]["tick_us"] = 86454; // TODO set this correct
    doc["data"]["timestamp"] = frame.timestamp / 1000000.0;
    doc["data"]["bits"] = frame.size;
    doc["data"]["line"] = line;
    doc["data"]["isEcho"] = (frame.flags & DALI_FRAME_ECHO) ? true : false;

    uint8_t indexStart = 1;
    if(frame.size == 16)
        indexStart = 2;
    else if(frame.size == 8)
        indexStart = 3;

    for(uint8_t i = indexStart; i < 4; i++)
    {
        uint8_t data = (frame.data >> ((3 - i)*8)) & 0xFF;
        doc["data"]["data"].add(data);
    }

    if(frame.flags & DALI_FRAME_ERROR)
        doc["data"]["framingError"] = true;

    sendJson(doc);
}

void DaliGateway::sendJson(JsonDocument &doc, bool appendTimeSignature)
{
    if(appendTimeSignature)
    {
        doc["timeSignature"]["timestamp"] = esp_timer_get_time() / 1000000.0;
        doc["timeSignature"]["counter"] = 0; //counter++;
    }
    String jsonString;
    serializeJson(doc, jsonString);
    webSocket.broadcastTXT(jsonString);
}

void DaliGateway::sendAnswer(uint8_t line, uint8_t status, uint8_t answer)
{
    JsonDocument doc;
    doc["type"] = "daliAnswer";
    doc["data"]["line"] = line;
    doc["data"]["result"] = status;
    doc["data"]["daliData"] = answer;

    sendJson(doc, false);
}