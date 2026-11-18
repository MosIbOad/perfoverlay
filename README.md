# perfoverlay

Lightweight, no-injection FPS and hardware overlay for Windows. Shows FPS, 1% / 0.1% lows, GPU/CPU usage and temps, and upscaler info (DLSS/FSR/XeSS) in a single exe. Built with C++20 and CMake.

## Metrics & 1% / 0.1% Low Methodology

`perfoverlay` implements the **Average of the Worst 1% / 0.1%** (Expected Shortfall / 1% Average Low) method for reporting 1% Low and 0.1% Low FPS.

### Calculation Method

1. All frame times in the rolling observation window are sorted in ascending order.
2. The slowest `ceil(N * 0.01)` frames (for 1% low) and `ceil(N * 0.001)` frames (for 0.1% low) are isolated.
3. We compute the arithmetic mean of these worst frame times ($T_{\text{worst\_avg}}$).
4. The low FPS is calculated as:
   $$\text{Low FPS} = \frac{1000.0}{T_{\text{worst\_avg}}}$$

### Rationale: Average of Worst 1% vs. Quantile Threshold

There are two primary ways to define 1% Low in game performance analysis:

1. **Quantile Threshold (e.g., 99th Percentile Cutoff)**:
   - Takes only the single boundary value between the top 99% and bottom 1% frames ($1000 / T_{99}$).
   - **Drawback**: It is completely blind to the severity of frame pacing anomalies beyond the threshold. A mild 25 ms hiccup and a severe 500 ms stutter freeze produce identical 99th percentile numbers if their rank is the same.

2. **Average of the Worst 1% (Implemented in `perfoverlay`)**:
   - Computes the mean frame time of all frames in the worst 1% tail.
   - **Advantage**: Directly penalizes long and severe stutters proportionally to their actual duration.
   - **Industry Standard**: Widely adopted by leading benchmark authorities (Gamers Nexus, Digital Foundry, PresentMon) as the most faithful representation of player-perceived stutter and smoothness.
