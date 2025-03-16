#pragma once

#include "WebsocketsServer.h"
#include "dali/Master.h"
#include "dali/Frame.h"
#include "ArduinoJson.h"

class DaliGateway
{
    WebSocketsServer webSocket = WebSocketsServer(80);
    std::vector<Dali::Master *> masters;
    std::vector<uint32_t> sent;
    uint32_t counter;

    void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length);

    public:
        void setup();
        void loop();
        void addMaster(Dali::Master *master);
        void handleData(uint8_t num, JsonDocument &doc);
        void receivedMonitor(uint8_t line, Dali::Frame frame);

        void sendJson(JsonDocument &doc, bool appendTimeSignature = true);
        void sendAnswer(uint8_t num, uint8_t status, uint8_t answer);
};