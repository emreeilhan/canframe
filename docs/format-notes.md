# Parser format notes

Implemented formats:

- **compact**: `123#AABBCC` — socketcan wire format, `#R` suffix for RTR frames, extended IDs auto-detected by value range
- **candump timestamped**: `(ts) iface ID#DATA` — timestamp extracted via `strtod`, interface name stored in `can_frame_t.ifname`
- **candump verbose**: `iface  ID  [DLC]  HH HH ...` — space-separated tokens, zero-DLC frames supported

Format detection in `parse_line()`:
1. Leading `(` → timestamped
2. `#` present with no spaces before it → compact
3. Everything else → verbose
