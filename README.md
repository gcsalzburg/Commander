# Commander LoRA library


## Packet structure

Each packet consists of:

| Packet | Description | Required? |
| --- | --- | --- |
| RR | 2 character network ID | ✅ |
| x | 1 character device ID | ✅ |
| . | Separator dot | ✅ |
| ###### | Message content | ✅ |
| . | Separator dot | ❔ |
| aaa | Randomised identifier | ❔ |


### Example packets

```javascript
RRl.0c00FF00.3gd // RUROC network, lighting device, set colour to 00FF00
RRs.1p.35w // RUROC network, switches device, button 1 pressed
```

### Packet acknowledgement

To send a read receipt for a packet, transmit back the same packet, with:

+ No message contents
+ No separator dots
+ Single `>` between message parts

Example receipt packets for packets above:

```javascript
Sent: RRl.0c00FF00.3gd
Ack: RRl>>3gd

Sent: RRs.1p.35w
Ack: RRs>35w
```

## Special messages

### Power on

```javascript
RRl.1.xxx
```

### Power off

(may not be sent)

```javascript
RRl.0.xxx
```

### Keep-alive pings

These are sent every 5 seconds (by default). They are not acknowledged.

```javascript
RRl.9.xxx
```
