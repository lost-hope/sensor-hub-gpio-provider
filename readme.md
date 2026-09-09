# GPIO Contact Provider

A [Sensor Hub](../sensor-hub/readme.md) provider usermod for generic
digital contacts - reed switches, float switches, dry relay outputs, or
any other simple on/off GPIO signal. Unlike most providers in this repo it
isn't tied to a specific chip: it reads up to **four independent pins**,
each registered as its own binary `<prefix>_ch1`..`<prefix>_ch4` sensor
(`SensorTypes::Contact`, HA device_class `door`), so one instance can cover
several unrelated contacts (front door, mailbox, cabinet lid, ...) without
needing a separate usermod per pin.

## Hardware

Each channel's **Pin** is independently optional - leave it unset (`-1`)
to disable that channel. Configured pins are reserved through WLED's
PinManager (won't silently clash with LEDs, relays or other usermods) and
read as `INPUT_PULLUP`, matching the common wiring for a reed switch/dry
contact to GND (closed = LOW). If your wiring or switch polarity is the
other way around, flip that channel's **Invert** setting instead of
rewiring.

## Usage

Add `sensor-hub-gpio-provider` to `custom_usermods` next to the
[Sensor Hub](../sensor-hub/readme.md) itself.

## Usermod Settings

| Setting | Default | Description |
|---|---|---|
| Enabled | on | Master on/off switch |
| Pin 1-4 | unset | Channel's input pin - unset disables that channel |
| Invert 1-4 | off | Flip that channel if its contact reads the opposite way |
| Check interval | 100 ms | How often all configured pins are polled |
| Name prefix | `gpio` | Sensor names become `<prefix>_ch1`..`<prefix>_ch4` - must be unique across every provider registered with the hub |
| Priority | 100 | `getValueBinary()` selection priority - lower wins if another provider also registers a Contact sensor |
