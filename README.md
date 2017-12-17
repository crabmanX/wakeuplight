# wakeuplight

Small FastLED & ESP8266 implementation of a wake-up light for homeassistant (or any other software that supports MQTT)
Features include:
- Color temperature based sunrise simulation
- Various other effects (color temperature, transition, rainbow)
- Plain MQTT communucation
- OTA updates

The code is loosely based on the pubsubclient ESP8266 example and https://github.com/mertenats/Open-Home-Automation/tree/master/ha_mqtt_rgbw_light_with_discovery

## Build and Upload

1. Download, install and setup platform.io
1. Adjust `platformio.ini` to your configuration
1. Copy `src/config.h.example` to `src/config.h` and adjust it to your setup
1. run `platformio run -e WemosD1mini -t upload` to compile and install

## Home-Assistant Configuration

This is my homeassistant configuration. I use automate for android with the tasker mqtt plugin to publish my alarm time on the /kilianmobile/alarm MQTT topic.
HA listens on this topic to set its internal alarm clock to 20 minutes before the phones alarm clock starts ringing (SZ_lights_on_mobile_alarm).
It then checks every 10 seconds if i am home (based on my phone bing in the network) and start the sunrise effect at the given time (SZ_lights_alarm).
The wake-up light is then turned off either when i rutn on my bathroom radio, or after one hour (SZ_lights_off)

`configuration.yml`:
```yaml
mqtt:
  broker: <your mqtt broker ip>
  port: 1883
  client_id: home-assistant
  keepalive: 60
  username: <your username>
  password: <your password>
  protocol: 3.1
  
light:
  - platform: mqtt_json
    state_topic: "SZ_wakeuplight/rgb1"
    command_topic: "SZ_wakeuplight/rgb1/set"
    name: SZ_wakeuplight
    brightness: true
    rgb: true
    transition: 2
    effect: true
    effect_list:
      - rainbow
      - warmwhite
      - coldwhite
      - sunrise
    optimistic: false
    qos: 0
  
input_datetime:
  sz_wakeuptime:
    name: "Wecker"
    has_date: true
    has_time: true
    initial: "2000-01-01 00:00"
    
timer:
  sz_wakeuplight_off:
    duration: '01:00:00'
```
      

`automations.yml`:
```yaml
- id: SZ_lights_on_mobile_alarm
  alias: "SZ lights on mobile alarm"
  initial_state: 'on'
  trigger:
    platform: mqtt
    topic: "/kilianmobile/alarm"
  action:
    service: input_datetime.set_datetime
    data_template:
      entity_id: input_datetime.sz_wakeuptime
      time: >
        {% if trigger.payload | int > 0 %}
          {{ (as_timestamp(utcnow()) + (trigger.payload | int) - 1200) | int | timestamp_custom("%H:%M") }}
        {%- else -%}
          00:00
        {%- endif %}
      date: >
        {% if trigger.payload | int > 0 %}
          {{ (as_timestamp(utcnow()) + (trigger.payload | int) - 1200) | int | timestamp_custom("%Y-%m-%d") }}
        {%- else -%}
          2000-01-01
        {%- endif %}
- id: SZ_lights_alarm
  alias: "SZ lights alarm"
  initial_state: 'on'
  trigger:
    platform: time
    seconds: '/10'
  condition:
    condition: and
    conditions:
    - condition: state
      entity_id: 'device_tracker.android_kilian'
      state: 'home'
    - condition: template
      value_template: >
        {{ ((states.input_datetime.sz_wakeuptime.attributes.timestamp|int) < as_timestamp(utcnow())|int)
          and (as_timestamp(utcnow())|int < (states.input_datetime.sz_wakeuptime.attributes.timestamp)|int + 20) }}
  action:
  - service: timer.start
    entity_id: timer.SZ_wakeuplight_off
  - service: light.turn_on
    data:
      entity_id: light.SZ_wakeuplight
      effect: sunrise

- id: SZ_lights_off
  alias: "SZ lights off"
  initial_state: 'on'
  trigger:
    - platform: event
      event_type: timer_finished
      event_data:
          entity_id: timer.SZ_wakeuplight_off
    - platform: state
      entity_id: media_player.silvercrest_sird_14_b1
      to: playing
  action:
  - service: light.turn_off
    data:
      entity_id: light.SZ_wakeuplight
```


## Implementation

The basic concept of the implementation is that everything the LED does is an effect - even when showing a constant color/brightness.
The most simple example is the transition, which fades from the current color to a new one and keeps displaying this coolor infinitely.
New effects can be implemented by subclassing the Effect class.
En exception is power off, which is handled by setting the brightnes to zero. The rationale here is that we dont keep track of the effect history, but still want to be able to resume the last effect after poweing on again.
      
