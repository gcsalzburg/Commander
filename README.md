# Commander LoRA library


## Packet structure

Packet structure:

```
RRx.########
^
| Marks as a RUROC packet
  ^
  | Single letter identifier for this project (s=switches, l=lighting, r=remote)
   ^
   | Divider
    ^
    | Message contents
```
