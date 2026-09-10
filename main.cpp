#include <iostream>
#include <vector>
#include <random>
#include <numeric>
#include <algorithm>
#include <iomanip>
#include <fstream>

// --- Data Structures ---

struct CqiEntry {
    int index;
    int modulationOrder; // Qm
    double codeRate;     // R
    double spectralEfficiency;
};

struct McsEntry {
    int index;
    int modulationOrder;
    double codeRate;
    double spectralEfficiency;
};

struct IntervalResult {
    int interval;
    double sinrDb;
    int cqi;
    int mcs;
    double throughputMbps;
};

// --- 3GPP TS 38.214 Tables ---

// CQI Table 1 (TS 38.214 Table 5.2.2.1-2)
const std::vector<CqiEntry> cqiTable = {
    {0, 0, 0.0, 0.0},           // Out of range
    {1, 2, 78.0/1024, 0.1523},  // QPSK
    {2, 2, 120.0/1024, 0.2344},
    {3, 2, 193.0/1024, 0.3770},
    {4, 2, 308.0/1024, 0.6016},
    {5, 2, 449.0/1024, 0.8770},
    {6, 2, 602.0/1024, 1.1758},
    {7, 4, 378.0/1024, 1.4766}, // 16QAM
    {8, 4, 490.0/1024, 1.9141},
    {9, 4, 616.0/1024, 2.4063},
    {10, 6, 466.0/1024, 2.7305},// 64QAM
    {11, 6, 567.0/1024, 3.3223},
    {12, 6, 666.0/1024, 3.9023},
    {13, 6, 772.0/1024, 4.5234},
    {14, 6, 873.0/1024, 5.1152},
    {15, 6, 948.0/1024, 5.5547}
};

// MCS Table 1 for PDSCH (TS 38.214 Table 5.1.3.1-1)
const std::vector<McsEntry> mcsTable = {
    {0,  2, 120.0/1024, 0.2344},
    {1,  2, 157.0/1024, 0.3066},
    {2,  2, 193.0/1024, 0.3770},
    {3,  2, 251.0/1024, 0.4902},
    {4,  2, 308.0/1024, 0.6016},
    {5,  2, 379.0/1024, 0.7402},
    {6,  2, 449.0/1024, 0.8770},
    {7,  2, 526.0/1024, 1.0273},
    {8,  2, 602.0/1024, 1.1758},
    {9,  2, 679.0/1024, 1.3262},
    {10, 4, 340.0/1024, 1.3281}, // Start of 16QAM
    {11, 4, 378.0/1024, 1.4766},
    {12, 4, 434.0/1024, 1.6953},
    {13, 4, 490.0/1024, 1.9141},
    {14, 4, 553.0/1024, 2.1602},
    {15, 4, 616.0/1024, 2.4063},
    {16, 4, 658.0/1024, 2.5703},
    {17, 6, 438.0/1024, 2.5664}, // Start of 64QAM
    {18, 6, 466.0/1024, 2.7305},
    {19, 6, 517.0/1024, 3.0293},
    {20, 6, 567.0/1024, 3.3223},
    {21, 6, 616.0/1024, 3.6094},
    {22, 6, 666.0/1024, 3.9023},
    {23, 6, 719.0/1024, 4.2129},
    {24, 6, 772.0/1024, 4.5234},
    {25, 6, 822.0/1024, 4.8164},
    {26, 6, 873.0/1024, 5.1152},
    {27, 6, 910.0/1024, 5.3320},
    {28, 6, 948.0/1024, 5.5547}
    // Indices 29, 30, 31 are reserved
};


// Map SINR to CQI using Shannon bound with an implementation margin (Shannon Gap)
int sinrToCqi(double sinrDb) {
    const double shannonGapDb = 2.0;
    int selectedCqi = 0;

    // Iterate backwards from the highest CQI (15) down to the lowest (1)
    for (int cqi = 15; cqi >= 1; --cqi) {
        double se = cqiTable[cqi].spectralEfficiency;
        double requiredSinrDb = 10.0 * std::log10(std::pow(2.0, se) - 1.0);
        requiredSinrDb += shannonGapDb;

        // If our current SINR is good enough for this CQI, select it and break
        if (sinrDb >= requiredSinrDb) {
            selectedCqi = cqi;
            break;
        }
    }

    return selectedCqi;
}

// Map CQI to MCS by selecting the highest MCS that doesn't exceed CQI spectral efficiency
int cqiToMcs(int cqi) {
    if (cqi == 0) return 0; // Out of range
    double targetEfficiency = cqiTable[cqi].spectralEfficiency;
    int selectedMcs = 0;
    for (const auto& mcs : mcsTable) {
        if (mcs.spectralEfficiency <= targetEfficiency) {
            selectedMcs = mcs.index;
        }
    }
    return selectedMcs;
}

// Simplified Spectral-Efficiency-based Throughput calculation
double estimateThroughput(const McsEntry& mcs) {
    const int numberOfRbs = 106;
    const int dataResPerRb = 144;
    const int layers = 1;
    const double slotDurationSeconds = 0.5e-3;

    if (mcs.modulationOrder == 0) return 0.0;

    double bitsPerSlot = numberOfRbs * dataResPerRb * mcs.modulationOrder * mcs.codeRate * layers;
    return (bitsPerSlot / slotDurationSeconds) / 1e6;
}

// Get full MCS entry by index
McsEntry getMcsEntry(int index) {
    for (const auto& mcs : mcsTable) {
        if (mcs.index == index) return mcs;
    }
    return mcsTable[0];
}

// --- Main Execution ---

int main() {
    const int numIntervals = 1000;
    const double averageSinr = 10.0;
    std::vector<IntervalResult> results;
    results.reserve(numIntervals);

    // --- AR1 Channel Model Setup ---
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<double> noise_dist(0.0, 1.5);
    const double alpha = 0.85;
    double currentSinr = averageSinr;

    std::cout << "Starting 5G NR Link Adaptation Simulation..." << "\n";

    // 1. Run Simulation
    for (int t = 0; t < numIntervals; ++t) {
        double noise = noise_dist(gen);
        currentSinr = alpha * currentSinr + (1.0 - alpha) * averageSinr + noise;

        int cqi = sinrToCqi(currentSinr);
        int mcsIndex = cqiToMcs(cqi);
        McsEntry mcs = getMcsEntry(mcsIndex);
        double throughput = estimateThroughput(mcs);

        results.push_back({t, currentSinr, cqi, mcsIndex, throughput});
    }

    // 2. Calculate Statistics
    double sumSinr = 0, sumCqi = 0, sumMcs = 0, sumThroughput = 0;
    double minThroughput = results[0].throughputMbps;
    double maxThroughput = results[0].throughputMbps;

    for (const auto& res : results) {
        sumSinr += res.sinrDb;
        sumCqi += res.cqi;
        sumMcs += res.mcs;
        sumThroughput += res.throughputMbps;

        if (res.throughputMbps < minThroughput) minThroughput = res.throughputMbps;
        if (res.throughputMbps > maxThroughput) maxThroughput = res.throughputMbps;
    }

    // 3. Print Final Results
    std::cout << "\n--- Simulation Results (" << numIntervals << " TTIs) ---\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Average SINR:       " << sumSinr / numIntervals << " dB\n";
    std::cout << "Average CQI:        " << sumCqi / numIntervals << "\n";
    std::cout << "Average MCS:        " << sumMcs / numIntervals << "\n";
    std::cout << "Average Throughput: " << sumThroughput / numIntervals << " Mbps\n";
    std::cout << "Min Throughput:     " << minThroughput << " Mbps\n";
    std::cout << "Max Throughput:     " << maxThroughput << " Mbps\n";

    // 4. Export per-interval data to CSV
    std::ofstream outFile("simulation_results.csv");
    if (outFile.is_open()) {
        outFile << "Interval,SINR_dB,CQI,MCS,Throughput_Mbps\n";
        for (const auto& res : results) {
            outFile << res.interval << ","
                    << res.sinrDb << ","
                    << res.cqi << ","
                    << res.mcs << ","
                    << res.throughputMbps << "\n";
        }
        outFile.close();
        std::cout << "-> Per-interval data saved to 'simulation_results.csv'\n";
    } else {
        std::cerr << "Error: Could not open file for writing.\n";
    }
    return 0;
}
