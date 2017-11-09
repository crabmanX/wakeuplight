/*
 * wakeuplight
 * Copyright (C) 2017  Kilian Lackhove <kilian@lackhove.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>

#include "Effect.hpp"

#include "config.h"

#define DEBUGPRINTLN(x) if(client.connected()){client.publish(MQTT_DEBUG_TOPIC, x);}else{Serial.println(x);}

const int BUFFER_SIZE = JSON_OBJECT_SIZE(10);

WiFiClient espClient;
PubSubClient client(espClient);
CRGB m_leds[NUM_LEDS];
Effect* effect = NULL;

void setup_wifi()
{
    delay(10);
    // We start by connecting to a WiFi network
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(WIFI_SSID);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    randomSeed(micros());

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}


void reconnect()
{
    // Loop until we're reconnected
    while (!client.connected())
    {
        Serial.print("Attempting MQTT connection...");

        // Attempt to connect
        if (client.connect(MQTT_CLIENT_ID,
            MQTT_USERNAME,
            MQTT_PASSWORD,
            MQTT_STATE_TOPIC,
            1,
            true,
            "{\"state\": \"OFF\"}"))
        {
            Serial.println("connected");
            // Once connected, publish an announcement...
            client.publish(MQTT_DEBUG_TOPIC, "alive");
            // ... and resubscribe
            client.subscribe(MQTT_SET_TOPIC);
        }
        else
        {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}


void setupOTA() {
  ArduinoOTA.setHostname(MQTT_CLIENT_ID);
  Serial.print(F("INFO: OTA hostname sets to: "));
  Serial.println(MQTT_CLIENT_ID);

  ArduinoOTA.setPassword((const char *)OTA_PASSWORD);
  Serial.print(F("INFO: OTA password sets to: "));
  Serial.println(OTA_PASSWORD);


  ArduinoOTA.onStart([]() {
    Serial.println(F("INFO: OTA starts"));
  });
  ArduinoOTA.onEnd([]() {
    Serial.println(F("INFO: OTA ends"));
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.print(F("INFO: OTA progresses: "));
    Serial.print(progress / (total / 100));
    Serial.println(F("%"));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.print(F("ERROR: OTA error: "));
    Serial.println(error);
    if (error == OTA_AUTH_ERROR)
      Serial.println(F("ERROR: OTA auth failed"));
    else if (error == OTA_BEGIN_ERROR)
      Serial.println(F("ERROR: OTA begin failed"));
    else if (error == OTA_CONNECT_ERROR)
      Serial.println(F("ERROR: OTA connect failed"));
    else if (error == OTA_RECEIVE_ERROR)
      Serial.println(F("ERROR: OTA receive failed"));
    else if (error == OTA_END_ERROR)
      Serial.println(F("ERROR: OTA end failed"));
  });
  ArduinoOTA.begin();
}

/*
{
  "brightness": 255,
  "color": {
    "r": 255,
    "g": 255,
    "b": 255,
    "x": 0.123,
    "y": 0.123
  },
  "effect": "colorloop",
  "state": "ON",
  "transition": 2,
}
*/
bool processJson(char *message)
{
    StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;

    JsonObject &root = jsonBuffer.parseObject(message);

    if (!root.success())
    {
        DEBUGPRINTLN("parseObject() failed");
        return false;
    }

    CRGB lastC = effect->GetCCurrent();
    uint8_t lastBr = effect->GetBrCurrent();
    delete effect;
    effect = NULL;

    CRGB newC = lastC;
    uint8_t newBr = lastBr;

    if (root.containsKey("state"))
    {
        if (strcmp(root["state"], "ON") == 0)
        {
            if (lastBr == 0)
            {
                newBr = 255;
            }
        }
        else if (strcmp(root["state"], "OFF") == 0)
        {
            newBr = 0;
        }
    }

    if (root.containsKey("color"))
    {
        newC = CRGB(root["color"]["r"], root["color"]["g"], root["color"]["b"]);
    }

    if (root.containsKey("brightness"))
    {
        newBr = root["brightness"];
    }

    unsigned long dt = 1000;
    if (root.containsKey("transition"))
    {
        dt = root["transition"];
        dt = dt * 1000;
    }

    if (root.containsKey("effect"))
    {
        if (strcmp(root["effect"], "rainbow") == 0)
        {
            effect = new Rainbow(lastC, lastBr);
            DEBUGPRINTLN("got rainbow effect");
        }
        else if (strcmp(root["effect"], "warmwhite") == 0)
        {
            effect = new Transition(lastC, lastBr, Tungsten40W, newBr, dt);
        }
        else if (strcmp(root["effect"], "coldwhite") == 0)
        {
            effect = new Transition(lastC, lastBr, Halogen, newBr, dt);
        }
        else if (strcmp(root["effect"], "sunrise") == 0)
        {
            effect = new Sunrise(CRGB::Black, 0, Tungsten40W, 255, 10*60*1000.0);
        }
        else
        {
            effect = new Rainbow(lastC, lastBr);
        }
    }

    if (root.containsKey("color_temp"))
    {
        float temp = root["color_temp"];
        temp = 1E6 / temp;
        effect = new BlackbodyTemp(lastC, lastBr, temp, newBr, dt);
    }

    if (!effect)
    {
        effect = new Transition(lastC, lastBr, newC, newBr, dt);
    }

    return true;
}

void sendState()
{
    StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;

    JsonObject &root = jsonBuffer.createObject();

    if (effect->GetBrEnd() > 0)
    {
        root["state"] = "ON";
    }
    else
    {
        root["state"] = "OFF";
    }

    CRGB currentC = effect->GetCEnd();
    JsonObject &color = root.createNestedObject("color");
    color["r"]        = currentC.red;
    color["g"]        = currentC.green;
    color["b"]        = currentC.blue;

    root["brightness"] = effect->GetBrEnd();

    char buffer[root.measureLength() + 1];
    root.printTo(buffer, sizeof(buffer));

    client.publish(MQTT_STATE_TOPIC, buffer, true);
}

void callback(char *topic, byte *payload, unsigned int length)
{
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");

    char message[length + 1];
    for (int i = 0; i < length; i++)
    {
        message[i] = (char)payload[i];
    }
    message[length] = '\0';
    Serial.println(message);

    if (!processJson(message))
    {
        return;
    }

    sendState();
}


void setup()
{
    pinMode(BUILTIN_LED_PIN,
            OUTPUT); // Initialize the BUILTIN_LED pin as an output
    Serial.begin(115200);

    setup_wifi();

    client.setServer(MQTT_SERVER, MQTT_SERVER_PORT);
    client.setCallback(callback);

    FastLED.addLeds<CHIPSET, DATA_PIN>(m_leds, NUM_LEDS).setCorrection( TypicalLEDStrip );

    effect = new Rainbow(Tungsten40W, 255);

    sendState();

    setupOTA();
}

void loop()
{
    if (!client.connected())
    {
        reconnect();
    }
    client.loop();

    ArduinoOTA.handle();


    effect->Step(m_leds);
}
