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

#include "Effect.hpp"

#include "config.h"

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

/*
SAMPLE PAYLOAD:
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
        Serial.println("parseObject() failed");
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

    if (root.containsKey("effect"))
    {
        if (strcmp(root["effect"], "rainbow") == 0)
        {
            effect = new Rainbow(lastC, lastBr);
        }
        else if (strcmp(root["effect"], "warmwhite") == 0)
        {
            effect = new Warmwhite(lastC, lastBr);
        }
        else
        {
            effect = new Rainbow(lastC, lastBr);
        }
    }

    unsigned long dt = 1000;
    if (root.containsKey("transition"))
    {
        dt = root["transition"];
        dt = dt * 1000;
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

    if (effect->GetBrFinal() > 0)
    {
        root["state"] = "ON";
    }
    else
    {
        root["state"] = "OFF";
    }

    CRGB currentC = effect->GetCFinal();
    JsonObject &color = root.createNestedObject("color");
    color["r"]        = currentC.red;
    color["g"]        = currentC.green;
    color["b"]        = currentC.blue;

    root["brightness"] = effect->GetBrFinal();

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

void reconnect()
{
    // Loop until we're reconnected
    while (!client.connected())
    {
        Serial.print("Attempting MQTT connection...");

        // Attempt to connect
        if (client.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD))
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

void setup()
{
    pinMode(BUILTIN_LED_PIN,
            OUTPUT); // Initialize the BUILTIN_LED pin as an output
    Serial.begin(115200);

    setup_wifi();

    client.setServer(MQTT_SERVER, MQTT_SERVER_PORT);
    client.setCallback(callback);

    FastLED.addLeds<CHIPSET, DATA_PIN>(m_leds, NUM_LEDS);

    effect = new Rainbow(CRGB::Black, 100);

    sendState();
}

void loop()
{
    if (!client.connected())
    {
        reconnect();
    }
    client.loop();

    effect->Step(m_leds);
}
