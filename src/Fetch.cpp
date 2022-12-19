#include "Fetch.h"
#include "debug.h"
#include <WiFiClientSecure.h>

Response fetch(const char* url, RequestOptions options) {
    // Parsing URL.
    Url parsedUrl = parseUrl(url);
    Serial.printf("url %s\r\n", url);
    Serial.printf("scheme %s\r\n", parsedUrl.scheme.c_str());
    Serial.printf("username %s\r\n", parsedUrl.username.c_str());
    Serial.printf("password %s\r\n", parsedUrl.password.c_str());
    Serial.printf("host %s\r\n", parsedUrl.host.c_str());
    Serial.printf("path %s\r\n", parsedUrl.path.c_str());
    Serial.printf("afterPath %s\r\n", parsedUrl.afterPath.c_str());
    Serial.printf("query %s\r\n", parsedUrl.query.c_str());
    Serial.printf("fragment %s\r\n", parsedUrl.fragment.c_str());
    Serial.printf("port %d\r\n", parsedUrl.port);

    bool useHttps = parsedUrl.scheme == "https";
    WiFiClient clientInsecure;
    WiFiClientSecure clientSecure;

    // Set fingerprint if https.
    if(useHttps) {
        #ifdef ESP8266
        if(options.fingerprint == "" && options.caCert == "") {
            DEBUG_FETCH("[INFO] No fingerprint or caCert is provided. Using the INSECURE mode for connection!");
            clientSecure.setInsecure();
        }
        else if(options.caCert != "") clientSecure.setTrustAnchors(new X509List(options.caCert.c_str()));
        else clientSecure.setFingerprint(options.fingerprint.c_str());
        #elif defined(ESP32)
        if(options.caCert == "") {
            DEBUG_FETCH("[INFO] No CA Cert is provided. Using the INSECURE mode for connection!");
            clientSecure.setInsecure();
        }
        else clientSecure.setCACert(options.caCert.c_str());
        #endif
    }

    // Retry every 15 seconds.
    uint16_t timeout = 15000;
    if (useHttps) {
        clientSecure.setTimeout(timeout);
    } else {
        clientInsecure.setTimeout(timeout);
    }

    // Connecting to server.
    if (useHttps) {
        while(!clientSecure.connect(parsedUrl.host.c_str(), parsedUrl.port)) {
            // TODO (FB): add timeout
            delay(1000);
            Serial.print(".");
        }
    } else {
        while(!clientInsecure.connect(parsedUrl.host.c_str(), parsedUrl.port)) {
            // TODO (FB): add timeout
            delay(1000);
            Serial.print(".");
        }
    }

    // Forming request.
    String request =
        options.method + " " + parsedUrl.path + parsedUrl.afterPath + " HTTP/1.1\r\n" +
        "Host: " + parsedUrl.host + "\r\n" +
        options.headers.text() +
        options.body + "\r\n\r\n";

    DEBUG_FETCH("-----REQUEST START-----\n%s\n-----REQUEST END-----", request.c_str());

    // Sending request.
    if (useHttps) {
        clientSecure.print(request);
    } else {
        clientInsecure.print(request);
    }

    DEBUG_FETCH("Sent request");

    // Getting response headers.
    Response response;
    String getAll;
    String getBody;

    if (useHttps) {
        for(int nLine = 1; clientSecure.connected(); nLine++) {
            DEBUG_FETCH("Getting response");

            // Reading headers line by line.
            String line = clientSecure.readStringUntil('\n');
            // Parse status and statusText from line 1.
            if(nLine == 1) {
                response.status = line.substring(line.indexOf(" ")).substring(0, line.indexOf(" ")).toInt();
                response.statusText = line.substring(line.indexOf(String(response.status)) + 4);
                response.statusText.trim();
                continue;
            }

            response.headers += line + "\n";
            // If headers end, move on.
            if(line == "\r") break;
        }
    } else {

        int timoutTimer = 2000;
        long startTimer = millis();
        boolean state = false;

        while ((startTimer + timoutTimer) > millis()) {
            Serial.print(".");
            delay(100);
            while (clientInsecure.available()) {
                char c = clientInsecure.read();
                // Serial.printf("%s", c);
                if (c == '\n') {
                    if (getAll.length()==0) {
                        state=true;
                    }
                    getAll = "";
                }
                else if (c != '\r') {
                    getAll += String(c);
                }
                if (state == true) {
                    getBody += String(c);
                }
                startTimer = millis();
            }

            if (getBody.length() > 0) {
                break;
            }
        }

        // for(int nLine = 1; clientInsecure.connected(); nLine++) {
        //     DEBUG_FETCH("Getting response");
        //     // Reading headers line by line.
        //     String line = clientInsecure.readStringUntil('\n');
        //     DEBUG_FETCH("Read string until");

        //     // Parse status and statusText from line 1.
        //     if(nLine == 1) {
        //         response.status = line.substring(line.indexOf(" ")).substring(0, line.indexOf(" ")).toInt();
        //         response.statusText = line.substring(line.indexOf(String(response.status)) + 4);
        //         response.statusText.trim();
        //         continue;
        //     }

        //     response.headers += line + "\n";
        //     // If headers end, move on.
        //     if(line == "\r") break;
        // }
    }

    DEBUG_FETCH("-----HEADERS START-----\n%s\n-----HEADERS END-----", response.headers.text().c_str());

    // Getting response body.
    if (useHttps) {
        while(clientSecure.available()) {
            response.body += clientSecure.readStringUntil('\n');
        }

        // Stopping the client.
        clientSecure.stop();
    } else {
        // while(clientInsecure.available()) {
        //     response.body += clientInsecure.readStringUntil('\n');
        // }
        response.body = getBody;

        // Stopping the client.
        clientInsecure.stop();
    }

    return response;
}

FetchClient fetch(const char* url, RequestOptions options, OnResponseCallback onResponseCallback) {
    // Parsing URL.
    Url parsedUrl = parseUrl(url);

    WiFiClientSecure client;
    // Retry every 15 seconds.
    client.setTimeout(15000);

    // Set fingerprint if https.
    if(parsedUrl.scheme == "https") {
        #ifdef ESP8266
        if(options.fingerprint == "" && options.caCert == "") {
            DEBUG_FETCH("[INFO] No fingerprint or caCert is provided. Using the INSECURE mode for connection!");
            client.setInsecure();
        }
        else if(options.caCert != "") client.setTrustAnchors(new X509List(options.caCert.c_str()));
        else client.setFingerprint(options.fingerprint.c_str());
        #elif defined(ESP32)
        if(options.caCert == "") {
            DEBUG_FETCH("[INFO] No CA Cert is provided. Using the INSECURE mode for connection!");
            client.setInsecure();
        }
        else client.setCACert(options.caCert.c_str());
        #endif
    }

    // Connecting to server.
    while(!client.connect(parsedUrl.host.c_str(), parsedUrl.port)) {
        delay(1000);
        Serial.print(".");
    }

    // Forming request.
    String request =
        options.method + " " + parsedUrl.path + parsedUrl.afterPath + " HTTP/1.1\r\n" +
        "Host: " + parsedUrl.host + "\r\n" +
        options.headers.text() +
        options.body + "\r\n\r\n";

    DEBUG_FETCH("-----REQUEST START-----\n%s\n-----REQUEST END-----", request.c_str());

    // Sending request.
    client.print(request);

    return FetchClient(client, onResponseCallback);
}

FetchClient::FetchClient() {}

FetchClient::FetchClient(WiFiClientSecure& client, OnResponseCallback onResponseCallback) : _client(client), _OnResponseCallback(onResponseCallback) {}

void FetchClient::loop() {
    if(_client.available()) {
        DEBUG_FETCH("[Info] Receiving response.");
        // Getting response headers.
        Response response;

        for(int nLine = 1; _client.connected(); nLine++) {
            // Reading headers line by line.
            String line = _client.readStringUntil('\n');
            // Parse status and statusText from line 1.
            if(nLine == 1) {
                response.status = line.substring(line.indexOf(" ")).substring(0, line.indexOf(" ")).toInt();
                response.statusText = line.substring(line.indexOf(String(response.status)) + 4);
                response.statusText.trim();
                continue;
            }

            response.headers += line + "\n";
            // If headers end, move on.
            if(line == "\r") break;
        }

        DEBUG_FETCH("-----HEADERS START-----\n%s\n-----HEADERS END-----", response.headers.text().c_str());

        // Getting response body.
        while(_client.available()) {
            response.body += _client.readStringUntil('\n');
        }

        // Stopping the client.
        _client.stop();

        _OnResponseCallback(response);
    }
}

ResponseHeaders::ResponseHeaders(): _text("") {}

String ResponseHeaders::get(String headerName) {
    String tillEnd = _text.substring(_text.lastIndexOf(headerName + ": "));
    return tillEnd.substring(tillEnd.indexOf(" ") + 1, tillEnd.indexOf("\n") - 1);
}

String& ResponseHeaders::text() {
    return _text;
}

void ResponseHeaders::operator+=(const String& s) {
    _text += s;
}

String ResponseHeaders::operator[](const String& headerName) {
    return get(headerName);
}

Body::Body(): _text("") {}

String Body::text() {
    return this->_text;
}

String Body::operator+(String str) {
    return this->_text + str;
}

String Body::operator=(String str) {
    return this->_text = str;
}

String operator+(String str, Body body) {
    return str + body.text();
}

Response::Response(): ok(false), status(200), statusText("OK"),
    redirected(false), type(""), headers({}), body("") {}

String Response::text() {
    return this->body;
}

size_t Response::printTo(Print& p) const {
    size_t r = 0;

    r += p.printf(
        (String("{") +
            "\n\t\"ok\": %d" +
            "\n\t\"status\": %d" +
            "\n\t\"statusText\": \"%s\"" +
            "\n\t\"body\": \"%s\"" +
        "\n}").c_str(),
        ok, status, statusText.c_str(), body.c_str());

    return r;
}

RequestOptions::RequestOptions(): method("GET"), headers(Headers()), body(Body()),
#if defined(ESP8266)
fingerprint(""),
#endif
caCert("") {}