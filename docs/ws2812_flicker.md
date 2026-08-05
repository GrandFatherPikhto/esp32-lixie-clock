# WS2812 Display Flicker — Diagnosis & Mitigation

Analysis of an intermittent "wrong digit" flicker on the WS2812 clock display: what it is, why it happens (software vs power), how to tell them apart, and the possible measures.

Related: [architect.md](architect.md) (display data path), [`led_display.c`](../main/led_display.c) (RMT channel + `led_display_send`), [`main.c`](../main/main.c) (display task, 30 ms timer).

## 1. Symptom

- Occasionally a digit position lights a **different LED than the intended digit**, with a **strongly different colour**.
- It is **stochastic and rare**. Initially it happened often (the **last 3–4 digits** flickering on every seconds change); after increasing the RMT memory block and the interrupt priority it became rare.

## 2. What it means — WS2812 bitstream corruption

WS2812 LEDs are daisy-chained: each LED samples its 24 GRB bits from the incoming serial stream and passes the remainder through. If **one** LED samples a bit wrongly (a timing violation, glitch or noise), the data shifts and **every LED after that point** receives a pixel meant for a different position.

That is exactly why the flicker appeared in the **last 3–4 digits**: corruption starts somewhere mid-chain, the LEDs before it stay correct, the ones after show shifted data. The "strongly different colour" is simply a colour from another part of the frame landing in that position.

## 3. Root causes

Both a **software timing** fault and a **power/wiring** fault can produce exactly this symptom.

### 3.1 Software — RMT refill latency (primary, confirmed on this project)

- A 60-LED frame is ~**2880 RMT symbols** (1440 bits × 2 symbols per bit), plus the reset code.
- The ESP32 (classic) RMT has **512 symbols** of total memory and **no DMA**, so for a full frame the driver must **refill the memory block via ISR** — about **6× per frame** at `mem_block_symbols = 512`, and ~45× at 64.
- If one refill is delayed — a Wi-Fi interrupt, an **NVS/flash write** (which disables caches for milliseconds), or any high-priority ISR — the bitstream stalls and the WS2812 misreads → a desync for the rest of the frame.
- **Evidence:** changing `mem_block_symbols` 64 → 512 and setting `intr_priority = 3` in [`led_display_init()`](../main/led_display.c) turned the flicker from frequent to rare. Only a software-timing fault behaves this way, so the RMT ISR latency was the dominant cause.

### 3.2 Hardware — power / wiring (possible)

- 60 WS2812s at full white draw ~**3–3.6 A**; voltage sag on thin wires or a weak supply makes the LED logic misread bits → the same wrong-LED symptom, stochastic and brightness-dependent.
- A **3.3 V data signal** driving a 5 V strip (high threshold ~0.7·VDD ≈ 3.5 V) over a long data wire is marginal.
- A power fault would **not** have been improved by the memory/priority change, so it is the secondary suspect for the residual rare flicker.

## 4. How to tell them apart (no hardware changes)

1. **Brightness test** — `set brightness=20 save`. If the flicker disappears → power/current. If it persists → data timing.
2. **White vs colour** — white (max current) flicker → power.
3. **Event correlation** — does it happen right after `configure_clock.py ... save`, a Wi-Fi reconnect, an NTP resync, or a slot-machine run? If yes → NVS/Wi-Fi ISR delay (software).
4. **Monitor logs** — [`led_display_send()`](../main/led_display.c) logs `rmt_transmit failed` and `RMT tx not done in time; skipping frame`. If flicker coincides with these → software.

## 5. Possible measures

### Software

1. **`mem_block_symbols = 512`** (the full RMT memory block) — already done; cuts ISR refills ~7×.
2. **`intr_priority = 3`** — already done; keeps the RMT ISR from being preempted.
3. **Defer / coalesce NVS writes** so a flash erase never overlaps an RMT transmission (a flash write disables caches and can delay the refill ISR for milliseconds). `app_config_save()` is the main trigger.
4. **Verify `rmt_tx_wait_all_done` never times out** — the 500 ms bound is far above the ~2 ms transmission; a timeout would mean the transmit path/queue is misbehaving.
5. **Keep `trans_queue_depth = 4`** and the 30 ms frame rate: the ~2 ms transmission is a small duty cycle and the queue never backs up.
6. **On DMA-capable chips (ESP32-S3/C3/H2)**, enable RMT **DMA** (`flags.with_dma`): the bitstream streams from a large DMA buffer with no CPU/ISR involvement, eliminating refill latency entirely. Not available on ESP32 classic.

### Hardware (the classic WS2812 checklist)

1. **Power supply** rated for the strip's peak current — ≥ 4 A for 60 LEDs at full white — with thick, short 5 V and GND wires.
2. **Bulk capacitor 470–1000 µF** across 5 V **at the strip input**.
3. **Series resistor 330–470 Ω on the DIN data line**, close to the ESP32 pin.
4. **Data wire routing** — short, and kept away from the 5 V power lines (separated or twisted).
5. **Level shifter (74HCT245 / SN74AHCT125)** if the data is 3.3 V on a 5 V strip over a longer run.
6. **Common ground** between the ESP32 and the strip power supply.

## 6. Diagnostic instrumentation (proposed)

Add counters in [`led_display_send()`](../main/led_display.c) for `rmt_transmit` failures and `rmt_tx_wait_all_done` timeouts, and surface them (e.g. via the console `status` command). If the flicker always correlates with a counter increment, the residual cause is software (RMT timing); if it happens with counters at zero, it is power/wiring.

## 7. Summary

The flicker is **WS2812 bitstream corruption** (shifted data → wrong LED/colour in a position). The dominant cause on this hardware is **RMT refill ISR latency**, which the `mem_block_symbols = 512` + `intr_priority = 3` changes largely fixed. The residual rare flicker is most likely either a rare ISR delay (NVS write, Wi-Fi event) or marginal power/wiring — the brightness test and the proposed counters will tell which.
