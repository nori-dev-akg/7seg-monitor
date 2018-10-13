#include "LedControl.h"
#ifdef ESP8266
#include <time.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ESP8266WebServer.h>
ESP8266WebServer server(80);
#else
#include <time.h>
#include "apps/sntp/sntp.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <ESPmDNS.h>
WebServer server(80);
#endif

#define _DNUM 4 // MAX7219 num
#define _DIN 14 // GPIO_NUM_14
#define _CS 12  // GPIO_NUM_12
#define _CLK 13 // GPIO_NUM_13

// WiFi Setting
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#define JST 3600 * 9

// DarkSky
#define _APIKEY ""
#define _PLACE "35.454954,139.631386" // 緯度,経度

LedControl lc = LedControl(_DIN, _CLK, _CS, _DNUM);

enum MODE
{
    MODE_NOP,
    MODE_CLOCK,
    MODE_TEMP,
    MODE_USER
};
MODE mode;
#define INIT_MODE MODE_CLOCK

// User display buffer
String userValue[_DNUM];

void setup()
{
    Serial.begin(115200);

    /*
    The MAX72XX is in power-saving mode on startup,
    we have to do a wakeup call
    */
    for (int d = 0; d < _DNUM; d++)
    {
        lc.shutdown(d, false); // Enable display
        lc.setIntensity(d, 8); // Set brightness level (0 is min, 15 is max)
        lc.clearDisplay(d);    // Clear display register
    }

    /*
    WiFi starting
    */
    Serial.print("WiFi connecting...");
    String dot = "con.";
    printDigit(0, dot);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print('.');
        printDigit(1, dot += ".");
        delay(1000);
    }
    Serial.println();
    Serial.printf("Connected, IP address: ");
    Serial.println(WiFi.localIP());
    Serial.println("WiFi connected!");
    printDigit(1, "on");
    displayIP(); // show IP address
    delay(3000);

    // initialize webserver
    initWebServer();

    clearDisplay();

    mode = INIT_MODE;
}

void loop()
{
    server.handleClient();

    switch (mode)
    {
    case MODE_CLOCK:
        displayClock();
        break;
    case MODE_USER:
        displayUser();
        break;
    case MODE_TEMP:
        displayTemp();
        break;
    case MODE_NOP:
    default:
        break;
    }
}

void printDigit(int device, String digit)
{
    char t[9];

    //Serial.println(digit);
    sprintf(t, "%-8s", digit.c_str());
    digit = t;

    bool dp;
    char c = ' ';
    for (int i = 0, j = 0; j < 9; i++, j++)
    {
        c = (i < digit.length()) ? digit[i] : ' ';
        if (c == '.')
        {
            if (i > 0 && digit[i - 1] != '.')
            {
                c = digit[i - 1];
                j--;
            }
            else
            {
                c = ' ';
            }
            lc.setChar(device, 7 - j, c, true);
        }
        else if (j < 8)
        {
            lc.setChar(device, 7 - j, c, false);
        }
    }
}

void clearDisplay()
{
    for (int d = 0; d < _DNUM; d++)
    {
        lc.clearDisplay(d); // Clear display register
    }
}

void initWebServer()
{
    server.on("/", handleRoot);

    server.on("/user", []() {
        server.send(200, "text/plain", "ok! user");
        mode = MODE_USER;
        for (int i = 0; i < _DNUM; i++)
        {
            String name = String("line") + (i + 1);
            if (server.hasArg(name))
            {
                String value = server.arg(name);
                userValue[i] = value;
            }
        }
    });

    server.on("/clock", []() {
        server.send(200, "text/plain", "ok! clock!");
        mode = MODE_CLOCK;
    });

    server.on("/temp", []() {
        server.send(200, "text/plain", "ok! temperature!");
        mode = MODE_TEMP;
    });

    server.onNotFound(handleNotFound);

    server.begin();
    Serial.println("HTTP server started");
}

void handleRoot()
{
#ifdef ESP8266
    server.send(200, "text/plain", "hello from esp8266!");
#else
    server.send(200, "text/plain", "hello from esp32!");
#endif
}

void handleNotFound()
{
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";
    for (uint8_t i = 0; i < server.args(); i++)
    {
        message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }
    server.send(404, "text/plain", message);
}

void initClock()
{
    clearDisplay();
    /*
    NTP start
    */
    configTime(JST, 0, "ntp.nict.jp", "ntp.jst.mfeed.ad.jp");
    delay(1000);
    mode = MODE_CLOCK;
}

void displayClock()
{
    static bool init = false;

    if (!init)
    {
        initClock();
        init = true;
    }

    time_t t;
    struct tm *tm;
    t = time(NULL);
    tm = localtime(&t);

    char rdate[30], rtime[30];
    sprintf(rdate, "%02d-%02d-%02d",
            tm->tm_year + (1900 - 2000), tm->tm_mon + 1, tm->tm_mday);
    sprintf(rtime, "%02d-%02d-%02d",
            tm->tm_hour, tm->tm_min, tm->tm_sec);

    printDigit(0, rdate);
    printDigit(1, "");
    printDigit(2, rtime);
    printDigit(3, "");
    delay(1000 - millis() % 1000);
}

void displayUser()
{
    // exp:http://192.168.0.202/user?line1=12345678&line2=abcdefgh&line3=IJLoPUY&line4=--1.2.3.4.--
    for (int i = 0; i < _DNUM; i++)
    {
        printDigit(i, userValue[i]);
        Serial.printf("%d:%s\n", i, userValue[i].c_str());
    }
    mode = MODE_NOP;
}

void displayIP()
{
    int byte1, byte2, byte3, byte4;

    sscanf(WiFi.localIP().toString().c_str(), "%d.%d.%d.%d", &byte1, &byte2, &byte3, &byte4);
    printDigit(0, (String(byte1) + ".").c_str());
    printDigit(1, (String(byte2) + ".").c_str());
    printDigit(2, (String(byte3) + ".").c_str());
    printDigit(3, (String(byte4)).c_str());

    mode = MODE_NOP;
}

void displayTemp()
{
    clearDisplay();
    printDigit(0, "con...");

    WiFiClientSecure client;
    String server = "api.darksky.net";
    String path = "/forecast/" + String(_APIKEY) + "/" + String(_PLACE) + "?exclude=minutely,hourly,alerts,flags&units=si";
    Serial.println(server + path);
    Serial.println("Starting connection to server...");
    if (!client.connect("api.darksky.net", 443))
    {
        Serial.println("Connection failed!");
        printDigit(1, "Error 1");
    }
    else
    {
        Serial.println("Connected to server!\n");
        client.println("GET " + path + " HTTP/1.1");
        client.println("Host: " + server);
        client.println("Connection: close");
        client.println();
        printDigit(0, "con...done");

        while (client.connected())
        {
            String line = client.readStringUntil('\n');
            if (line == "\r")
            {
                Serial.println("headers received");
                break;
            }
        }
        String payload = "";
        //payload = client.readString(); // 遅い
        // Buffer size, 128 bytes in this case
        uint8_t *_buffer = new uint8_t[128];
        while (client.available())
        {
            int actualLength = client.read(_buffer, 128);
            if (actualLength <= 0)
                break;
            payload += String((char *)_buffer).substring(0, actualLength);
        }
        delete[] _buffer;
        //Serial.println(payload.length());
        //Serial.println(payload);

        client.stop();

        tm dt;
        char rdate[30];
        String temp, now, fore, tempLow, tempHigh;

        // 現在の気温
        // ["currently"]["time"]
        int s = payload.indexOf("\"time\":");
        int e = payload.indexOf(",", s);
        now = payload.substring(s + 7, e);
        // ["currently"]["temperature"]
        s = payload.indexOf("\"temperature\":", e);
        e = payload.indexOf(",", s);
        temp = payload.substring(s + 14, e);
        getJstTime(now, &dt);
        float ftemp = temp.toFloat();
        sprintf(rdate, "%02d.%02d %4.1f", dt.tm_mon, dt.tm_mday, ftemp);
        printDigit(0, rdate);
        Serial.println(rdate);

        // 今日の最低最高気温
        // ["daily"]["data"][0]["time"]
        s = payload.indexOf("\"time\":", e);
        e = payload.indexOf(",", s);
        fore = payload.substring(s + 7, e);
        // ["daily"]["data"][0]["temperatureHigh"]
        s = payload.indexOf("\"temperatureHigh\":", e);
        e = payload.indexOf(",", s);
        tempHigh = payload.substring(s + 18, e);
        // ["daily"]["data"][0]["temperatureLow"]
        s = payload.indexOf("\"temperatureLow\":", e);
        e = payload.indexOf(",", s);
        tempLow = payload.substring(s + 17, e);
        getJstTime(fore, &dt);
        sprintf(rdate, " %4.1f %4.1f", tempLow.toFloat(), tempHigh.toFloat());
        printDigit(1, rdate);
        Serial.println(rdate);

        // 明日の最低最高気温
        // ["daily"]["data"][1]["time"]
        s = payload.indexOf("\"time\":", e);
        e = payload.indexOf(",", s);
        fore = payload.substring(s + 7, e);
        // ["daily"]["data"][1]["temperatureHigh"]
        s = payload.indexOf("\"temperatureHigh\":", e);
        e = payload.indexOf(",", s);
        tempHigh = payload.substring(s + 18, e);
        // ["daily"]["data"][1]["temperatureLow"]
        s = payload.indexOf("\"temperatureLow\":", e);
        e = payload.indexOf(",", s);
        tempLow = payload.substring(s + 17, e);
        getJstTime(fore, &dt);
        sprintf(rdate, "%02d.%02d", dt.tm_mon, dt.tm_mday);
        printDigit(2, rdate);
        sprintf(rdate, " %4.1f %4.1f", tempLow.toFloat(), tempHigh.toFloat());
        Serial.println(rdate);
        printDigit(3, rdate);
    }
    mode = MODE_NOP;
}

void getJstTime(String utc, tm *dt)
{
#ifdef ESP8266
    time_t t = (time_t)utc.toInt() + JST;
#else
    time_t t = (time_t)utc.toInt();
#endif
    struct tm *tm = localtime(&t);

    dt->tm_year = tm->tm_year + (1900 - 2000);
    dt->tm_mon = tm->tm_mon + 1;
    dt->tm_mday = tm->tm_mday;
    dt->tm_hour = tm->tm_hour;
    dt->tm_min = tm->tm_min;
    dt->tm_sec = tm->tm_sec;
    Serial.printf("%04d/%02d/%02d %02d:%02d:%02d\n",
        tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
        tm->tm_hour, tm->tm_min, tm->tm_sec);
}
