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

#include <FastLED.h>

#include "config.h"

class Effect
{

public:
    Effect(CRGB cBegin, uint8_t brBegin)
        : m_cBegin(cBegin), m_cCurrent(cBegin), m_cFinal(cBegin),
          m_brBegin(brBegin), m_brCurrent(brBegin), m_brFinal(brBegin)
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

    CRGB GetCFinal()
    {
        return m_cFinal;
    };

    uint8_t GetBrFinal()
    {
        return m_brFinal;
    };

protected:
    unsigned long m_tStart;

    CRGB m_cBegin;
    CRGB m_cCurrent;
    CRGB m_cFinal;
    uint8_t m_brBegin;
    uint8_t m_brCurrent;
    uint8_t m_brFinal;
};

class Rainbow : public Effect
{
public:
    Rainbow(CRGB cBegin, uint8_t brBegin) : Effect(cBegin, brBegin), m_done(false){};

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

    FastLED.setBrightness(m_brFinal);
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
               CRGB c_goal,
               uint8_t br_goal,
               unsigned long duration)
        : Effect(cBegin, brBegin), m_duration(duration)
    {
        m_cFinal  = c_goal;
        m_brFinal = br_goal;
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
    uint8_t r  = m_cBegin.red + (m_cFinal.red - m_cBegin.red) * frac;
    uint8_t g  = m_cBegin.green + (m_cFinal.green - m_cBegin.green) * frac;
    uint8_t b  = m_cBegin.blue + (m_cFinal.blue - m_cBegin.blue) * frac;
    uint8_t br = m_brBegin + (m_brFinal - m_brBegin) * frac;

    m_cCurrent  = CRGB(r, g, b);
    m_brCurrent = br;

    FastLED.setBrightness(m_brCurrent);
    fill_solid(leds, NUM_LEDS, m_cCurrent);
    FastLED.show();

    return true;
};

class Warmwhite : public Effect
{
public:
    Warmwhite(CRGB cBegin, uint8_t brBegin) : Effect(cBegin, brBegin), m_done(false){};

protected:
    virtual bool Step(CRGB *leds);

    bool m_done;

private:
};

bool Warmwhite::Step(CRGB *leds)
{
    if (m_done)
    {
        return false;
    }

    m_cCurrent = CRGB(255, 246, 237);

    FastLED.setBrightness(m_brFinal);
    fill_solid(leds, NUM_LEDS, m_cCurrent);
    FastLED.show();

    m_done = true;

    return true;
};

#endif // EFFECT_H
