#pragma once

#ifndef IOT_GW_USE_WEBUI
#include "WebsocketsServer.h"
#else
#include "WebUI.h"
#endif

#include "dali/Master.h"
#include "dali/Frame.h"
#include "ArduinoJson.h"

struct async_resp_arg {
    httpd_handle_t hd;
    char *buffer;
    uint8_t len;
    int fd = -1;
};

class DaliGateway
{
    #ifndef IOT_GW_USE_WEBUI
    httpd_handle_t server = NULL;
    #endif
    std::vector<Dali::Master *> masters;
    std::vector<uint32_t> sent;
    uint32_t counter;

    public:
        int fd = -1;

        void setup();
        void addMaster(Dali::Master *master);
        void receivedMonitor(uint8_t line, Dali::Frame frame);
        void handleData(httpd_req_t *ctx, uint8_t * payload);
        
        void generateInfoMessage();

    private:
        void sendJson(JsonDocument &doc, bool appendTimeSignature = true);
        void sendResponse(uint8_t line, uint8_t status);
        void sendAnswer(uint8_t line, uint8_t status, uint8_t answer);
        void sendRawWebsocket(const char *data);
};