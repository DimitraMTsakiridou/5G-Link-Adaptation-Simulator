# 5G NR Link Adaptation Simulator

A standalone C++17 application simulating the downlink link-adaptation procedure for a single User Equipment (UE) in a 5G New Radio (NR) network. 

The simulator dynamically maps time-varying Signal-to-Interference-plus-Noise Ratio (SINR) to Channel Quality Indicator (CQI), selects the optimal Modulation and Coding Scheme (MCS), and estimates the achievable physical layer throughput.

## Build and Run Instructions (Linux)

This project uses CMake for cross-platform builds. To compile and run the simulation:

```bash
mkdir build && cd build
cmake ..
make
./link_adaptation
```

## Architectural & Engineering Assumptions

This simulator bridges the gap between **3GPP TS 38.214**, **TS 38.211**, and **TS 38.306** specifications and practical RF physics. The following engineering assumptions were implemented:

### 3GPP Standardization
The application rigidly utilizes the full standard **NR CQI Table 1** (TS 38.214 Table 5.2.2.1-2) and **NR PDSCH MCS Table 1** (TS 38.214 Table 5.1.3.1-1). Reserved MCS indices (29-31) used for HARQ retransmissions are omitted as this simulator evaluates first-transmission capacity.

### Channel Modeling (AR1)
Instead of fully independent random AWGN variations, the channel fading is modeled using a First-Order Auto-Regressive AR(1) process to introduce realistic time-correlation (memory). 
* `SINR(t) = α * SINR(t-1) + (1 - α) * μ + noise`

**Key Parameters:**
* **μ (Mean):** Represents the baseline average SINR of the channel. The term `(1 - α) * μ` constantly pulls the signal back towards this equilibrium state, preventing the random noise from driving the signal to infinity.
* **α (Correlation Coefficient):** Represents the channel's "memory". A higher value (closer to 1.0) represents static, ideal conditions (e.g., a stationary user) where the current signal strongly depends on the previous one. We assume α = 0.85 (low-mobility/pedestrian coherence) and a fast-fading noise standard deviation of 1.5 dB.

### SINR-to-CQI Mapping (Shannon Bound)
3GPP does not define a universal numerical SINR-to-CQI table. Instead of using hardcoded empirical thresholds, the minimum required SINR is calculated dynamically based on the Shannon-Hartley theorem. 
For each CQI's Spectral Efficiency (SE), the required SINR is: `SINR_dB = 10 * log10(2^SE - 1) + Δ`

An implementation margin (Shannon gap) of Δ = 2.0 dB is added to account for real-world 5G receiver imperfections.
* **How it works:** The algorithm evaluates the CQI table in reverse (from highest CQI 15 down to 1). It compares the current SINR against the calculated theoretical requirement for each CQI and selects the highest possible index that can be decoded with a Transport Block Error Rate (BLER) of ≤ 10%.

### CQI-to-MCS Mapping
CQI and MCS indices are not strictly equivalent. The base station (gNB) logic scans the MCS table and safely selects the highest available MCS whose spectral efficiency does not exceed the spectral efficiency dictated by the UE's reported CQI.
* *Example:* If the UE reports CQI 9 (SE = 2.4063), the gNB scans the MCS table and selects MCS 15 (SE = 2.4063). It will not select a higher MCS, ensuring the transmission remains within the safe decoding limits of the channel.

### Throughput Estimation (TS 38.211 & TS 38.306)
Instead of a complex Transport Block Size (TBS) quantization, throughput is mathematically estimated via a spectral-efficiency model assuming a standard 20 MHz channel configuration (106 RBs, 144 REs per RB, 1 Spatial Layer).

* **Time Domain (TS 38.211):** Each transmission interval is modeled as one NR slot. Assuming 30 kHz subcarrier spacing (numerology μ = 1), TS 38.211 gives 2 slots per 1 ms subframe, so the slot duration is 0.5 ms. Therefore, 1,000 intervals correspond to 0.5 seconds of simulated real-world time.
* **Data Rate Calculation (TS 38.306):** The simplified `estimateThroughput` function shares the same structure as the official TS 38.306 max data rate formula (Section 4.1.2). The difference is that while 38.306 uses the maximum code rate (R_max = 948/1024) to compute the theoretical peak, our function uses the dynamic code rate of the specific MCS selected by the link adaptation process, estimating the achievable throughput per interval as the channel varies.

## Simulation Results

Executing the simulation for 1,000 Transmission Time Intervals (TTIs) yields the following metrics, demonstrating the dynamic nature of the link adaptation:

```text
--- Simulation Results (1000 TTIs) ---
Average SINR:       9.85 dB
Average CQI:        9.51
Average MCS:        16.55
Average Throughput: 79.84 Mbps
Min Throughput:     26.77 Mbps
Max Throughput:     138.09 Mbps
-> Per-interval data saved to 'simulation_results.csv'
```

*Note: The maximum throughput of ~138 Mbps reflects a single spatial layer (SISO) over 20 MHz. Scaling this up to 100 MHz bandwidth with 4x4 MIMO would theoretically yield multi-gigabit speeds aligning with 5G commercial claims.*

## Data Visualization

The simulator automatically exports the per-interval data to a `simulation_results.csv` file. Below is the MATLAB visualization of the generated data, showcasing the AR(1) time-correlated fading channel (top) and the resulting quantized throughput adjustments (bottom) as the network switches MCS levels.

![5G NR Link Adaptation Simulation](5G_NR_Link_Adaptation_Simulation.png)
