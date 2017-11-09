/*
 * <one line to give the program's name and a brief idea of what it does.>
 * Copyright (C) 2017  Kilian Lackhove <email>
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

#ifndef EFFECT_H
#define EFFECT_H

#include <Arduino.h>
#include <FastLED.h>

#include "config.h"


CRGB TempToRGB(float temp)
{
    // http://www.tannerhelland.com/4435/convert-temperature-rgb-algorithm-code/
    float red, green, blue;

    temp = temp > 40E3 ? 40E3 : temp;
    temp = temp < 1E3 ? 1E3 : temp;

    temp = temp / 100;

    // red
    if (temp <= 66)
    {
        red = 255;
    }
    else
    {
        red = temp - 60;
        red       = 329.698727446 * pow(red, -0.1332047592);
        red = red < 0 ? 0 : red;
        red = red > 255 ? 255 : red;
    }

    // green
    if (temp <= 66)
    {
        green = temp;
        green       = 99.4708025861 * log(green) - 161.1195681661;
    }
    else
    {
        green = temp - 60;
        green       = 288.1221695283 * pow(green, -0.0755148492);
    }
    green = green < 0 ? 0 : green;
    green = green > 255 ? 255 : green;

    // blue
    if (temp >= 66)
    {
        blue = 255;
    }
    else
    {
        if (temp <= 19)
        {
            blue = 0;
        }
        else
        {
            blue = temp - 10;
            blue       = 138.5177312231 * log(blue) - 305.0447927307;
            blue = blue < 0 ? 0 : blue;
            blue = blue > 255 ? 255 : blue;
        }
    }

    return CRGB((int) red, (int) green, (int) blue);
}


class Effect
{

public:
    Effect(CRGB cBegin, uint8_t brBegin)
        : m_cBegin(cBegin), m_cCurrent(cBegin), m_cEnd(cBegin),
          m_brBegin(brBegin), m_brCurrent(brBegin), m_brEnd(brBegin)
    {
        m_tStart = millis();
    };

    virtual ~Effect(){};

    virtual bool Step(CRGB *leds){};

    CRGB GetCCurrent()
    {
        return m_cCurrent;
    };

    uint8_t GetBrCurrent()
    {
        return m_brCurrent;
    };

    CRGB GetCEnd()
    {
        return m_cEnd;
    };

    uint8_t GetBrEnd()
    {
        return m_brEnd;
    };

protected:
    unsigned long m_tStart;

    CRGB m_cBegin;
    CRGB m_cCurrent;
    CRGB m_cEnd;
    uint8_t m_brBegin;
    uint8_t m_brCurrent;
    uint8_t m_brEnd;
};

class Rainbow : public Effect
{
public:
    Rainbow(CRGB cBegin, uint8_t brBegin)
        : Effect(cBegin, brBegin), m_done(false){};

protected:
    virtual bool Step(CRGB *leds);

    bool m_done;

private:
};

bool Rainbow::Step(CRGB *leds)
{
    if (m_done)
    {
        return false;
    }

    FastLED.setBrightness(m_brEnd);
    fill_rainbow(leds, NUM_LEDS, 0, 5);
    FastLED.show();

    m_done = true;

    return true;
};

class Transition : public Effect
{
public:
    Transition(CRGB cBegin,
               uint8_t brBegin,
               CRGB cEnd,
               uint8_t brEnd,
               unsigned long duration)
        : Effect(cBegin, brBegin), m_duration(duration)
    {
        m_cEnd  = cEnd;
        m_brEnd = brEnd;
    };

protected:
    virtual bool Step(CRGB *leds);

private:
    unsigned long m_duration;
};

bool Transition::Step(CRGB *leds)
{
    unsigned long t = millis() - m_tStart;
    if (t > m_duration)
    {
        return false;
    }

    float frac = (float)t / m_duration;
    uint8_t r  = m_cBegin.red + (m_cEnd.red - m_cBegin.red) * frac;
    uint8_t g  = m_cBegin.green + (m_cEnd.green - m_cBegin.green) * frac;
    uint8_t b  = m_cBegin.blue + (m_cEnd.blue - m_cBegin.blue) * frac;
    uint8_t br = m_brBegin + (m_brEnd - m_brBegin) * frac;

    m_cCurrent  = CRGB(r, g, b);
    m_brCurrent = br;

    FastLED.setBrightness(m_brCurrent);
    fill_solid(leds, NUM_LEDS, m_cCurrent);
    FastLED.show();

    return true;
};

class Sunrise : public Effect
{
public:
    Sunrise(CRGB cBegin,
            uint8_t brBegin,
            CRGB cEnd,
            uint8_t brEnd,
            unsigned long duration)
        : Effect(cBegin, brBegin), m_duration(duration)
    {
        m_cEnd  = cEnd;
        m_brEnd = brEnd;
    };

protected:
    virtual bool Step(CRGB *leds);

private:
    unsigned long m_duration;
};

bool Sunrise::Step(CRGB *leds)
{
    //TODO: fade from 0K to 2800K
    unsigned long t = millis() - m_tStart;
    if (t > m_duration)
    {
        return false;
    }

    uint8_t frac = map(t, 0, m_duration, 0, 240);
    m_cCurrent   = ColorFromPalette(HeatColors_p, frac);
    m_brCurrent  = map(t, 0, m_duration, m_brBegin, m_brEnd);

    FastLED.setBrightness(m_brCurrent);
    fill_solid(leds, NUM_LEDS, m_cCurrent);
    FastLED.show();

    return true;
};

class BlackbodyTemp : public Effect
{
public:
    BlackbodyTemp(CRGB cBegin,
                  uint8_t brBegin,
                  float temp,
                  uint8_t brEnd,
                  unsigned long duration)
        : Effect(cBegin, brBegin), m_duration(duration)
    {
        m_cEnd  = TempToRGB(temp);
        m_brEnd = brEnd;
    };

protected:
    virtual bool Step(CRGB *leds);

private:
    unsigned long m_duration;

};

bool BlackbodyTemp::Step(CRGB *leds)
{
    unsigned long t = millis() - m_tStart;
    if (t > m_duration)
    {
        return false;
    }

    float frac = (float)t / m_duration;
    uint8_t r  = m_cBegin.red + (m_cEnd.red - m_cBegin.red) * frac;
    uint8_t g  = m_cBegin.green + (m_cEnd.green - m_cBegin.green) * frac;
    uint8_t b  = m_cBegin.blue + (m_cEnd.blue - m_cBegin.blue) * frac;
    uint8_t br = m_brBegin + (m_brEnd - m_brBegin) * frac;

    m_cCurrent  = CRGB(r, g, b);
    m_brCurrent = br;

    FastLED.setBrightness(m_brCurrent);
    fill_solid(leds, NUM_LEDS, m_cCurrent);
    FastLED.show();

    return true;
};

#endif // EFFECT_H
