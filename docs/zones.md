# Zones

Zones allow multiple sensors of the same type to coexist on the mesh without overwriting each other's data. Each sensor node publishes both generic and zone-specific state keys.

## How Zones Work

A PIR sensor in "zone1" publishes:
- `motion` - generic key (last sensor to update wins)
- `motion_zone1` - zone-specific key (unique to this sensor)

A DHT11 sensor in "zone2" publishes:
- `temp` and `humidity` - generic keys
- `temp_zone2` and `humidity_zone2` - zone-specific keys

## Configuration

Set the zone in each sketch's configuration section:

```cpp
#define MOTION_ZONE  "zone1"   // PIR node
#define SENSOR_ZONE  "zone1"   // DHT11 node
```

## Example Multi-Zone Setup

| Node | Zone | State Keys |
|------|------|------------|
| PIR (front door) | `entrance` | `motion`, `motion_entrance` |
| PIR (backyard) | `backyard` | `motion`, `motion_backyard` |
| DHT11 (living room) | `living` | `temp`, `humidity`, `temp_living`, `humidity_living` |
| DHT11 (garage) | `garage` | `temp`, `humidity`, `temp_garage`, `humidity_garage` |

## Watching Zone-Specific State

### Watch Any Zone

```cpp
// Watch all motion events (any zone)
swarm.watchState("motion", [](const String& key, const String& value, const String& oldValue) {
  Serial.printf("Motion somewhere: %s\n", value.c_str());
});
```

### Watch Specific Zone

```cpp
// Watch specific zone
swarm.watchState("motion_entrance", [](const String& key, const String& value, const String& oldValue) {
  Serial.printf("Front door motion: %s\n", value.c_str());
});
```

### Filter by Prefix

```cpp
// Watch all state changes and filter by prefix
swarm.watchState("*", [](const String& key, const String& value, const String& oldValue) {
  if (key.startsWith("temp_")) {
    String zone = key.substring(5);  // Extract zone name
    Serial.printf("Temperature in %s: %s\n", zone.c_str(), value.c_str());
  }
});
```

## Best Practices

1. **Use descriptive zone names**: `kitchen`, `garage`, `bedroom` are better than `zone1`, `zone2`

2. **Keep zone names short**: They become part of state keys, so brevity helps

3. **Document your zones**: Keep a list of which physical location maps to which zone name

4. **Consistent naming**: Use the same zone name for all sensors in one location

## Zone Naming Conventions

| Pattern | Example | Use Case |
|---------|---------|----------|
| Location | `kitchen`, `garage` | Room-based automation |
| Function | `entry`, `exit` | Access control |
| Floor | `floor1`, `basement` | Multi-story buildings |
| Device | `sensor1`, `sensor2` | Simple numbered devices |

## Aggregating Zone Data

To aggregate data from multiple zones:

```cpp
// Track all temperature readings
std::map<String, float> temperatures;

swarm.watchState("*", [&](const String& key, const String& value, const String& oldValue) {
  if (key.startsWith("temp_")) {
    String zone = key.substring(5);
    temperatures[zone] = value.toFloat();

    // Calculate average
    float sum = 0;
    for (auto& pair : temperatures) {
      sum += pair.second;
    }
    float avg = sum / temperatures.size();
    Serial.printf("Average temp: %.1f\n", avg);
  }
});
```
