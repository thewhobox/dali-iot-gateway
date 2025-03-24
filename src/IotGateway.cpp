#include "IotGateway.h"

#include "file_index_html.h"
#include "file_index_js.h"
#include "file_index_css.h"

static esp_err_t ws_handler(httpd_req_t *req);
static esp_err_t web_handler(httpd_req_t *req);

void IotGateway::setup()
{
    httpd_uri_t ws = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = ws_handler,
        .user_ctx = this,
        .is_websocket = true
    };
    httpd_uri_t web = {
        .uri = "/dali*",
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
        .httpd = ws,
        .uri = "/",
        .name = "Dali Websocket",
        .isVisible = false
    };
    openknxWebUI.addService(_ws);
    WebService _web = {
        .httpd = web,
        .uri = "/dali",
        .name = "Dali Monitor"
    };
    openknxWebUI.addService(_web);
    #endif

    xTaskCreate(responseTask, "IotGateway Response", 2096, this, 0, NULL);
}

void IotGateway::responseTask(void *arg)
{
    IotGateway *gw = (IotGateway *)arg;
    while (true)
    {
        if(gw->resp.size() > 0)
        {
            wait_resp *wresp = gw->resp.front();

            Dali::Response resp = gw->masters[wresp->line]->getResponse(wresp->ref);

            if(resp.state == Dali::ResponseState::WAITING || resp.state == Dali::ResponseState::SENT)
            {
                continue;
            }

            if(resp.state == Dali::ResponseState::RECEIVED)
            {
                gw->sendAnswer(wresp->line, 8, resp.frame.data & 0xFF);
            }
            else if(resp.state == Dali::ResponseState::NO_ANSWER)
            {
                gw->sendAnswer(wresp->line, 0, 0);
            }

            gw->resp.erase(gw->resp.begin());
            delete wresp;
        }
    }
}

static void ws_async_send(void *arg)
{
    httpd_ws_frame_t ws_pkt;
    struct async_resp_arg *resp_arg = (async_resp_arg*)arg;
    
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t*)resp_arg->buffer;
    ws_pkt.len = resp_arg->len;
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    if(resp_arg->fd == -1)
    {
        static size_t max_clients = CONFIG_LWIP_MAX_LISTENING_TCP;
        size_t fds = max_clients;
        int client_fds[max_clients];

        esp_err_t ret = httpd_get_client_list(resp_arg->hd, &fds, client_fds);

        if (ret != ESP_OK) {
            return;
        }

        for (int i = 0; i < fds; i++) {
            int client_info = httpd_ws_get_fd_info(resp_arg->hd, client_fds[i]);
            if (client_info == HTTPD_WS_CLIENT_WEBSOCKET) {
                httpd_ws_send_frame_async(resp_arg->hd, client_fds[i], &ws_pkt);
            }
        }
    } else {
        httpd_ws_send_frame_async(resp_arg->hd, resp_arg->fd, &ws_pkt);
    }

    free(resp_arg);
}

void IotGateway::addMaster(Dali::Master *master)
{
    uint8_t index = masters.size();
    printf("Adding master %d\n", index);
    masters.push_back(master);
    master->registerMonitor([this, index](Dali::Frame frame) {
        this->receivedMonitor(index, frame);
    });
}

static esp_err_t ws_handler(httpd_req_t *req)
{
    IotGateway *gw = (IotGateway *)req->user_ctx;

    if(req->method == HTTP_GET)
    {
        char *host = NULL;
        char *upgrade = NULL;
        size_t buf_len;
        buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
        if (buf_len > 1)
        {
            host = (char*)malloc(buf_len);
            if (httpd_req_get_hdr_value_str(req, "Host", host, buf_len) != ESP_OK)
            {
                /* if something is wrong we just 0 the whole memory */
                memset(host, 0x00, buf_len);
            }
        }
        if(host == NULL)
        {
            httpd_resp_set_status(req, "400 Bad Request");
            httpd_resp_send(req, NULL, 0);
            return ESP_ERR_NOT_FOUND;
        }

        buf_len = httpd_req_get_hdr_value_len(req, "Upgrade") + 1;
        if (buf_len > 1)
        {
            upgrade = (char*)malloc(buf_len);
            if (httpd_req_get_hdr_value_str(req, "Upgrade", upgrade, buf_len) != ESP_OK)
            {
                /* if something is wrong we just 0 the whole memory */
                memset(upgrade, 0x00, buf_len);
            }
        }

        /* this is no websocket so we redirect it to the startpage */
        if(upgrade == NULL)
        {
            httpd_resp_set_status(req, "302 Found");
            char *redirect_url = NULL;
            #ifndef IOT_GW_USE_WEBUI
            asprintf(&redirect_url, "http://%s/dali", host);
            #else
            asprintf(&redirect_url, "http://%s%s", host, openknxWebUI.getBaseUri());
            #endif
            httpd_resp_set_hdr(req, "Location", redirect_url);
            httpd_resp_send(req, NULL, 0);

            free(redirect_url);
            if(host != NULL)
                free(host);
            return ESP_ERR_NOT_ALLOWED;
        }

        printf("Got new Websocket from %s\n", host);
        free(host);
        free(upgrade);
        gw->fd = httpd_req_to_sockfd(req);
        gw->generateInfoMessage();
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    /* Set max_len = 0 to get the frame len */
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        printf("ws_handler: httpd_ws_recv_frame failed to get frame len with %d\n", ret);
        return ret;
    }
    
    if (ws_pkt.len) {
        /* ws_pkt.len + 1 is for NULL termination as we are expecting a string */
        buf = (uint8_t*)calloc(1, ws_pkt.len + 1);
        if (buf == NULL) {
            printf("ws_handler: Failed to calloc memory for buf\n");
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = buf;
        /* Set max_len = ws_pkt.len to get the frame payload */
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            printf("ws_handler: httpd_ws_recv_frame failed with %d\n", ret);
            free(buf);
            return ret;
        }
        gw->handleData(req, ws_pkt.payload);
    }
    free(buf);
    return ret;
}

static esp_err_t web_handler(httpd_req_t *req)
{
    if(strcmp(req->uri, "/dali") == 0)
    {
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
        httpd_resp_send(req, file_index_html, file_index_html_len);
        return ESP_OK;
    }
    else if(strcmp(req->uri, "/dali.css") == 0)
    {
        httpd_resp_set_type(req, "text/css");
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
        httpd_resp_send(req, file_index_css, file_index_css_len);
        return ESP_OK;
    }
    else if(strcmp(req->uri, "/dali.js") == 0)
    {
        httpd_resp_set_type(req, "application/javascript");
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
        httpd_resp_send(req, file_index_js, file_index_js_len);
        return ESP_OK;
    }

    httpd_resp_send_404(req);
    return ESP_FAIL;
}

void IotGateway::handleData(httpd_req_t *ctx, uint8_t * payload)
{
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        return;
    }
    // printf("Received: %s\n", doc["type"].as<String>());

    uint8_t line = doc["data"]["line"];

    // {"data":
    //     {"daliData":[255,6],"line":0,"mode":
    //         {"priority":4,"sendTwice":false,"waitForAnswer":false},
    //     "numberOfBits":16},
    // "type":"daliFrame"}

    if (doc["type"] != "daliFrame")
        return; // we only handle dali frames

    if(line >= masters.size()) {
        sendResponse(line, 5);
        return;
    }

    Dali::Frame frame;
    frame.size = doc["data"]["numberOfBits"];
    frame.flags = DALI_FRAME_FORWARD;

    JsonArray bytes = doc["data"]["daliData"].as<JsonArray>();
    uint8_t index = 0;
    for (JsonVariant value : bytes) {
        frame.data = (frame.data << 8) | value.as<uint8_t>();
        index++;
    }

    uint32_t ref = masters[line]->sendRaw(frame);
    sent.push_back(ref);
    if(doc["data"]["mode"]["sendTwice"])
    {
        ref = masters[line]->sendRaw(frame);
        sent.push_back(ref);
    }

    if(doc["data"]["mode"]["waitForAnswer"])
    {
        wait_resp *wresp = new wait_resp();
        wresp->line = line;
        wresp->ref = ref;
        resp.push_back(wresp);
    }
}

void IotGateway::receivedMonitor(uint8_t line, Dali::Frame frame)
{
    if(frame.flags & DALI_FRAME_ECHO && frame.flags & DALI_FRAME_FORWARD)
    {
        for(uint32_t &r : sent)
        {
            if(r == frame.ref)
            {
                sendResponse(line, 0);
                for(int i = 0; i < sent.size(); i++)
                {
                    if(sent[i] == frame.ref)
                    {
                        sent.erase(sent.begin() + i);
                        break;
                    }
                }
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

void IotGateway::sendJson(JsonDocument &doc, bool appendTimeSignature)
{
    if(appendTimeSignature)
    {
        doc["timeSignature"]["timestamp"] = esp_timer_get_time() / 1000000.0;
        doc["timeSignature"]["counter"] = counter++;
    }
    String jsonString;
    serializeJson(doc, jsonString);
    sendRawWebsocket(jsonString.c_str());
}

/*
0 Der Befehl wurde an den DALI Bus gesendet.
1 Der Befehl wurde aufgrund der Spannungsversorgung des DALI Bus nicht gesendet.
2 Der Befehl wurde aufgrund des DALI Initialize Modus nicht gesendet.
3 Der Befehl wurde aufgrund des DALI Quiescent Modus nicht gesendet.
4 Der Sendepuffer des DALI Interface ist voll.
5 Das DALI Interface unterstützt die angeforderte Linie nicht.
6 Der Befehl enthält einen Syntax-Fehler.
7 Der Befehl wurde aufgrund eines aktiven Makros nicht gesendet.
61 Der Befehl wurde aufgrund einer Kollision am DALI Bus nicht gesendet.
62 Der Befehl wurde aufgrund eines DALI Bus Fehlers nicht gesendet.
63 Der Befehl wurde aufgrund einer Zeitüberschreitung nicht gesendet.
100 Das DALI Interface hat keine Antwort zurückgegeben
*/
void IotGateway::sendResponse(uint8_t line, uint8_t status)
{
    JsonDocument doc;
    doc["type"] = "daliFrame";
    doc["data"]["line"] = line;
    doc["data"]["result"] = status;

    sendJson(doc, false);
}

/*
0 keine Antwort
8 antwort
63 framing error
*/
void IotGateway::sendAnswer(uint8_t line, uint8_t status, uint8_t answer)
{
    JsonDocument doc;
    doc["type"] = "daliAnswer";
    doc["data"]["line"] = line;
    doc["data"]["result"] = status;
    doc["data"]["daliData"] = answer;

    sendJson(doc, false);
}

void IotGateway::sendRawWebsocket(const char *data)
{
    struct async_resp_arg *resp_arg = (struct async_resp_arg *)malloc(sizeof(struct async_resp_arg));

    #ifndef IOT_GW_USE_WEBUI
    resp_arg->hd = &server; // the httpd handle
    #else
    resp_arg->hd = openknxWebUI.getHandler(); // the httpd handle
    #endif

    uint32_t length = strlen(data);
    resp_arg->len = length;
    char* buffer = (char*)malloc(length+1);
    memset(buffer, 0, length+1);
    memcpy(buffer, data, length);
    resp_arg->buffer = buffer; // a malloc'ed buffer to transmit
    resp_arg->fd = fd;
    fd = -1;

    httpd_queue_work(resp_arg->hd, ws_async_send, resp_arg);
}

void IotGateway::generateInfoMessage()
{
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
}