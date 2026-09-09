#include "wled.h"
#include "sensor_bus.h"

/*
 * Generic multi-channel GPIO contact provider.
 *
 * Reads up to four independent digital input pins (reed switches, float
 * switches, simple relay/dry-contact outputs, ...) and pushes each one
 * into the Sensor Hub (see ../sensor-hub/usermod_sensor_hub.cpp and
 * ../sensor-hub/sensor_bus.h) as a binary "<prefix>_ch1".."<prefix>_ch4"
 * sensor (SensorTypes::Contact, HA device_class "door"). This usermod
 * never talks to MQTT, the JSON API or the Info tab itself - the hub takes
 * care of all of that once a sensor is registered here.
 *
 * Unlike most providers in this repo, this one doesn't drive a specific
 * chip - each of the four channels is an independently optional GPIO pin
 * (pin < 0 = channel unused), so one instance of this usermod can cover
 * several unrelated contacts (e.g. a front door + a mailbox flap + a
 * cabinet lid) without needing a separate usermod per pin.
 *
 * Each pin is configured as INPUT_PULLUP, matching the common wiring for a
 * reed switch/dry contact to GND - closed reads LOW. Use a channel's
 * "invert" setting if your wiring/switch polarity is the other way around.
 * Pins are not shared WLED globals - each is reserved via WLED's
 * PinManager (to avoid clashing with LEDs/relays/other usermods) right
 * here in this usermod's own settings.
 */

REGISTER_SENSOR_SLOT(_slotCh1, "_ch1", SensorTypes::Contact, 0, 100);
REGISTER_SENSOR_SLOT(_slotCh2, "_ch2", SensorTypes::Contact, 0, 100);
REGISTER_SENSOR_SLOT(_slotCh3, "_ch3", SensorTypes::Contact, 0, 100);
REGISTER_SENSOR_SLOT(_slotCh4, "_ch4", SensorTypes::Contact, 0, 100);
static const SensorSlotDescriptor* const _channelSlots[4] = { &_slotCh1, &_slotCh2, &_slotCh3, &_slotCh4 };

class GPIOSensorUsermod : public Usermod {
  private:
    SensorHub* hub = nullptr;
    uint8_t channelHandle[4] = { SENSOR_HANDLE_INVALID, SENSOR_HANDLE_INVALID, SENSOR_HANDLE_INVALID, SENSOR_HANDLE_INVALID };
    bool channelInitDone[4] = { false, false, false, false };

    bool enabled = true;

    unsigned long lastRead = 0;

    // config
    int8_t pin[4] = { -1, -1, -1, -1 };       // per-channel input pin, unset (disabled) by default
    bool invert[4] = { false, false, false, false }; // per-channel: flip if your contact reads the opposite way
    uint16_t checkIntervalMs = 100;           // how often all configured pins are polled
    String namePrefix = "gpio";               // sensor names become "<prefix>_ch1".."<prefix>_ch4"
    uint8_t priority = 100;                   // getValueBinary() selection priority - lower wins among sensors of the same SensorType (see sensor_bus.h)

    static const char _name[];
    static const char _enabled[];
    static const char _pin1[];
    static const char _invert1[];
    static const char _pin2[];
    static const char _invert2[];
    static const char _pin3[];
    static const char _invert3[];
    static const char _pin4[];
    static const char _invert4[];
    static const char _checkInterval[];
    static const char _namePrefix[];
    static const char _priority[];

    void setupChannel(uint8_t ch) {
      if (pin[ch] < 0) return;
      if (!PinManager::allocatePin(pin[ch], false, PinOwner::UM_Unspecified)) {
        pin[ch] = -1; // conflicts with another pin owner - force reconfiguration
        return;
      }
      pinMode(pin[ch], INPUT_PULLUP);
      channelInitDone[ch] = true;
    }

    void registerSensors() {
      if (!hub) return;
      for (uint8_t ch = 0; ch < 4; ch++) {
        if (!channelInitDone[ch] || channelHandle[ch] != SENSOR_HANDLE_INVALID) continue;
        channelHandle[ch] = hub->attachSensor(_channelSlots[ch], namePrefix.c_str(), 0, priority);
      }
    }

  public:
    void setup() override {
      // Neither this nor setupChannel() touches 'enabled' (the user's own
      // on/off switch, persisted to config) - channelInitDone[] is what
      // actually gates loop() per channel, so a later pin fix takes effect
      // on the next boot instead of staying stuck disabled.
      for (uint8_t ch = 0; ch < 4; ch++) setupChannel(ch);
    }

    void loop() override {
      if (!enabled) return;

      if (!hub) hub = getSensorHub(); // Sensor Hub usermod may finish init after us
      if (hub) registerSensors();

      unsigned long now = millis();
      if (now - lastRead < (unsigned long)checkIntervalMs) return;
      lastRead = now;

      if (!hub) return;
      for (uint8_t ch = 0; ch < 4; ch++) {
        if (!channelInitDone[ch] || channelHandle[ch] == SENSOR_HANDLE_INVALID) continue;
        bool state = digitalRead(pin[ch]);
        if (invert[ch]) state = !state;
        hub->setSensorAvailable(channelHandle[ch], true);
        hub->updateSensorBinary(channelHandle[ch], state);
      }
    }

    void addToConfig(JsonObject& root) override {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top[FPSTR(_enabled)] = enabled;
      top[FPSTR(_pin1)] = pin[0];
      top[FPSTR(_invert1)] = invert[0];
      top[FPSTR(_pin2)] = pin[1];
      top[FPSTR(_invert2)] = invert[1];
      top[FPSTR(_pin3)] = pin[2];
      top[FPSTR(_invert3)] = invert[2];
      top[FPSTR(_pin4)] = pin[3];
      top[FPSTR(_invert4)] = invert[3];
      top[FPSTR(_checkInterval)] = checkIntervalMs;
      top[FPSTR(_namePrefix)] = namePrefix;
      top[FPSTR(_priority)] = priority;
    }

    bool readFromConfig(JsonObject& root) override {
      int8_t oldPin[4] = { pin[0], pin[1], pin[2], pin[3] };

      JsonObject top = root[FPSTR(_name)];
      bool configComplete = !top.isNull();
      configComplete &= getJsonValue(top[FPSTR(_enabled)], enabled);
      configComplete &= getJsonValue(top[FPSTR(_pin1)], pin[0]);
      configComplete &= getJsonValue(top[FPSTR(_invert1)], invert[0]);
      configComplete &= getJsonValue(top[FPSTR(_pin2)], pin[1]);
      configComplete &= getJsonValue(top[FPSTR(_invert2)], invert[1]);
      configComplete &= getJsonValue(top[FPSTR(_pin3)], pin[2]);
      configComplete &= getJsonValue(top[FPSTR(_invert3)], invert[2]);
      configComplete &= getJsonValue(top[FPSTR(_pin4)], pin[3]);
      configComplete &= getJsonValue(top[FPSTR(_invert4)], invert[3]);
      configComplete &= getJsonValue(top[FPSTR(_checkInterval)], checkIntervalMs);
      configComplete &= getJsonValue(top[FPSTR(_namePrefix)], namePrefix);
      configComplete &= getJsonValue(top[FPSTR(_priority)], priority);

      for (uint8_t ch = 0; ch < 4; ch++) {
        if (pin[ch] != oldPin[ch]) {
          // pin changed at runtime via the Settings UI - release the old one (if any) and re-init on the new one
          if (channelInitDone[ch] && oldPin[ch] >= 0) PinManager::deallocatePin(oldPin[ch], PinOwner::UM_Unspecified);
          channelInitDone[ch] = false;
          setupChannel(ch);
        }
      }
      return configComplete;
    }

    void appendConfigData(Print& settingsScript) override {
      settingsScript.print(F("addInfo('GPIOSensor:pin1',1,'channel 1 input pin (INPUT_PULLUP) - empty/-1 disables this channel');"));
      settingsScript.print(F("addInfo('GPIOSensor:invert1',1,'flip channel 1 if your contact reads the opposite way');"));
      settingsScript.print(F("addInfo('GPIOSensor:pin2',1,'channel 2 input pin (INPUT_PULLUP) - empty/-1 disables this channel');"));
      settingsScript.print(F("addInfo('GPIOSensor:invert2',1,'flip channel 2 if your contact reads the opposite way');"));
      settingsScript.print(F("addInfo('GPIOSensor:pin3',1,'channel 3 input pin (INPUT_PULLUP) - empty/-1 disables this channel');"));
      settingsScript.print(F("addInfo('GPIOSensor:invert3',1,'flip channel 3 if your contact reads the opposite way');"));
      settingsScript.print(F("addInfo('GPIOSensor:pin4',1,'channel 4 input pin (INPUT_PULLUP) - empty/-1 disables this channel');"));
      settingsScript.print(F("addInfo('GPIOSensor:invert4',1,'flip channel 4 if your contact reads the opposite way');"));
      settingsScript.print(F("addInfo('GPIOSensor:checkInterval',1,'milliseconds between pin reads');"));
      settingsScript.print(F("addInfo('GPIOSensor:namePrefix',1,'sensor names become &lt;prefix&gt;_ch1.._ch4 - must be unique across all sensor providers');"));
      settingsScript.print(F("addInfo('GPIOSensor:priority',1,'getValueBinary() selection priority - lower wins if another provider also registers a Contact sensor');"));
    }
};

const char GPIOSensorUsermod::_name[]          PROGMEM = "GPIOSensor";
const char GPIOSensorUsermod::_enabled[]       PROGMEM = "enabled";
const char GPIOSensorUsermod::_pin1[]          PROGMEM = "pin1";
const char GPIOSensorUsermod::_invert1[]       PROGMEM = "invert1";
const char GPIOSensorUsermod::_pin2[]          PROGMEM = "pin2";
const char GPIOSensorUsermod::_invert2[]       PROGMEM = "invert2";
const char GPIOSensorUsermod::_pin3[]          PROGMEM = "pin3";
const char GPIOSensorUsermod::_invert3[]       PROGMEM = "invert3";
const char GPIOSensorUsermod::_pin4[]          PROGMEM = "pin4";
const char GPIOSensorUsermod::_invert4[]       PROGMEM = "invert4";
const char GPIOSensorUsermod::_checkInterval[] PROGMEM = "checkInterval";
const char GPIOSensorUsermod::_namePrefix[]    PROGMEM = "namePrefix";
const char GPIOSensorUsermod::_priority[]      PROGMEM = "priority";

static GPIOSensorUsermod gpio_sensor;
REGISTER_USERMOD(gpio_sensor);
