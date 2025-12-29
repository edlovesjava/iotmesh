# Serial Commands

Connect to any node via serial monitor (115200 baud) to access the command interface.

## Built-in Commands

| Command | Description |
|---------|-------------|
| `status` | Show node info (ID, role, peers, heap) |
| `peers` | List all known peers |
| `state` | Show all shared state entries |
| `set <key> <value>` | Set a shared state value |
| `get <key>` | Get a shared state value |
| `sync` | Broadcast full state to all nodes |
| `scan` | I2C scan (if display enabled) |
| `reboot` | Restart the node |

## Remote Command Protocol (RCP)

Send commands to other nodes in the mesh:

```
cmd <target> <command> [key=value ...]
```

### Examples

```bash
# Broadcast ping to all nodes
cmd * ping

# Get status from specific node
cmd Node1 status

# Get status from node by ID
cmd 12345678 status

# Get specific state key
cmd Node1 get key=temp

# Set state on remote node
cmd Node1 set key=led value=1

# Force state sync
cmd * sync
```

### Target Formats

| Format | Example | Description |
|--------|---------|-------------|
| `*` | `cmd * ping` | Broadcast to all nodes |
| Name | `cmd PIR status` | Target by node name |
| ID | `cmd 12345678 status` | Target by node ID |

### Available RCP Commands

| Command | Arguments | Description |
|---------|-----------|-------------|
| `status` | - | Node info (ID, role, heap, uptime) |
| `peers` | - | List connected peers |
| `state` | - | Get all shared state |
| `get` | `key=<name>` | Get specific state key |
| `set` | `key=<name> value=<val>` | Set state key/value |
| `sync` | - | Force state broadcast |
| `ping` | - | Connectivity test |
| `info` | - | Node capabilities |
| `reboot` | - | Restart node |

## Node-Specific Commands

### PIR Node

| Command | Description |
|---------|-------------|
| `pir` | Show PIR sensor status and model |

### DHT Node

| Command | Description |
|---------|-------------|
| `dht` | Show DHT sensor status |

## Adding Custom Commands

Register custom serial command handlers in your sketch:

```cpp
swarm.onSerialCommand([](const String& input) -> bool {
  if (input == "mycommand") {
    Serial.println("Executing my command...");
    return true;  // Command was handled
  }

  if (input.startsWith("custom ")) {
    String arg = input.substring(7);
    Serial.printf("Custom with arg: %s\n", arg.c_str());
    return true;
  }

  return false;  // Not our command, try others
});
```

## Output Format

### Status Command

```
=== Node Status ===
ID: 12345678
Name: PIR
Role: NODE
Coordinator: 87654321
Peers: 3
Heap: 245632
Uptime: 3600s
```

### Peers Command

```
=== Peers (3) ===
[1] 87654321 (Gateway) - COORD - 5s ago
[2] 11111111 (LED) - NODE - 2s ago
[3] 22222222 (Button) - NODE - 3s ago
```

### State Command

```
=== Shared State ===
led = 1 (v3, origin: 22222222)
motion = 0 (v15, origin: 12345678)
temp = 72.5 (v8, origin: 33333333)
```

## Tips

1. **Use `status` first** to verify node connectivity and identity

2. **Check `peers`** to see which nodes are visible

3. **Use `state`** to debug synchronization issues

4. **Use `cmd * ping`** to test mesh connectivity

5. **Serial output** shows command responses and mesh activity
