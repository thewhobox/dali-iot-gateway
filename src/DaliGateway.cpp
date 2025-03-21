#include "DaliGateway.h"

#include "index.h"

static esp_err_t ws_handler(httpd_req_t *req);
static esp_err_t web_handler(httpd_req_t *req);

void DaliGateway::setup()
{
    httpd_uri_t ws = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = ws_handler,
        .user_ctx = this,
        .is_websocket = true
    };
    httpd_uri_t web = {
        .uri = "/dali",
        .method = HTTP_GET,
        .handler = web_handler,
        .user_ctx = this
    };

    #ifndef IOT_GW_USE_WEBUI
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    if (httpd_start(&server, &config) == ESP_OK) {
        printf("Registering URI handlers\n");
        httpd_register_uri_handler(server, &ws);
        httpd_register_uri_handler(server, &web);
    } else {
        printf("Failed to start the server\n");
    }
    #else
    WebService _ws = {
        .uri = ws,
        .name = "Dali Websocket",
        .isVisible = false
    };
    openknxWebUI.addService(_ws);
    WebService _web = {
        .uri = web,
        .name = "Dali Monitor"
    };
    openknxWebUI.addService(_web);
    #endif
}

void DaliGateway::loop()
{
    
}

static void ws_async_send(void *arg)
{
    httpd_ws_frame_t ws_pkt;
    struct async_resp_arg *resp_arg = (async_resp_arg*)arg;
    httpd_handle_t hd = resp_arg->hd;
    int fd = resp_arg->fd;

    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t*)resp_arg->buffer;
    ws_pkt.len = strlen(resp_arg->buffer+1);
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    
    static size_t max_clients = CONFIG_LWIP_MAX_LISTENING_TCP;
    size_t fds = max_clients;
    int client_fds[max_clients];

    esp_err_t ret = httpd_get_client_list(hd, &fds, client_fds);

    if (ret != ESP_OK) {
        return;
    }

    for (int i = 0; i < fds; i++) {
        int client_info = httpd_ws_get_fd_info(hd, client_fds[i]);
        if (client_info == HTTPD_WS_CLIENT_WEBSOCKET) {
            httpd_ws_send_frame_async(hd, client_fds[i], &ws_pkt);
        }
    }
    free(resp_arg);
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

const char* TAG = "dali-iot-gateway";
static esp_err_t ws_handler(httpd_req_t *req)
{
    printf("IOT URI: %s\n", req->uri);
    DaliGateway *gw = (DaliGateway *)req->user_ctx;

    if(req->method == HTTP_GET)
    {
        // TODO redirect every request which is not a websocket upgrade
        #ifndef IOT_GW_USE_WEBUI
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_status(req, "301 Moved Permanently");
        httpd_resp_set_hdr(req, "Location", "/dali");
        httpd_resp_send(req, NULL, 0);
        #else
        // redirect to WEBUI_BASE_URI
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_status(req, "301 Moved Permanently");
        httpd_resp_set_hdr(req, "Location", "/start");
        httpd_resp_send(req, NULL, 0);
        #endif
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    /* Set max_len = 0 to get the frame len */
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    }
    ESP_LOGI(TAG, "frame len is %d", ws_pkt.len);
    if (ws_pkt.len) {
        /* ws_pkt.len + 1 is for NULL termination as we are expecting a string */
        buf = (uint8_t*)calloc(1, ws_pkt.len + 1);
        if (buf == NULL) {
            ESP_LOGE(TAG, "Failed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = buf;
        /* Set max_len = ws_pkt.len to get the frame payload */
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
            free(buf);
            return ret;
        }
        ESP_LOGI(TAG, "Got packet with message: %s", ws_pkt.payload);
        gw->handleData(req, ws_pkt.payload);
    }
    free(buf);
    return ret;
}

static esp_err_t web_handler(httpd_req_t *req)
{
    printf("IOT URI: %s\n", req->uri);
    if(strcmp(req->uri, "/dali") == 0)
    {
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, index_html, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    httpd_resp_send_404(req);
    return ESP_FAIL;
}

void DaliGateway::handleData(httpd_req_t *ctx, uint8_t * payload)
{
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        return;
    }
    printf("Received: %s\n", doc["type"].as<String>());

    uint8_t line = doc["data"]["line"];

    // {"data":
    //     {"daliData":[255,6],"line":0,"mode":
    //         {"priority":4,"sendTwice":false,"waitForAnswer":false},
    //     "numberOfBits":16},
    // "type":"daliFrame"}
    if (doc["type"] != "daliFrame")
        return; // we only handle dali frames

    if(doc["data"]["line"] >= masters.size()) {
        sendAnswer(doc["data"]["line"], 5, 0);
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
    sendRawWebsocket(jsonString.c_str());
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

void DaliGateway::sendRawWebsocket(const char *data)
{
    struct async_resp_arg *resp_arg = (struct async_resp_arg *)malloc(sizeof(struct async_resp_arg));

    #ifndef IOT_GW_USE_WEBUI
    resp_arg->hd = &server; // the httpd handle
    #else
    resp_arg->hd = openknxWebUI.getHandler(); // the httpd handle
    #endif
    uint32_t length = strlen(data);
    char* buffer = (char*)malloc(length+1);
    memcpy(buffer, data, length);
    resp_arg->buffer = buffer; // a malloc'ed buffer to transmit

    httpd_queue_work(resp_arg->hd, ws_async_send, resp_arg);
}