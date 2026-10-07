// Hardware performance counters for the cache-miss experiment (Figure 10).
//
// Counted events (raw codes for Raptor Cove P-cores, Intel i7-14700K):
//   L1 / L2 / L3 load misses: MEM_LOAD_RETIRED.{L1,L2,L3}_MISS
//   instructions, cycles, branch misses
// All counters are opened on the P-core PMU (cpu_core) and the process is
// pinned to a P-core (see cache_bench.h), so P-core and E-core counts are never
// mixed. Only loads are counted; write cost shows up in the measured time.
// If the kernel multiplexes the counters, each value is scaled by
// time_enabled / time_running (as in Viktor Leis's PerfEvent.hpp).
//
// Requires: sudo sysctl -w kernel.perf_event_paranoid=1

#ifndef PERF_COUNTER_H
#define PERF_COUNTER_H

#include <linux/perf_event.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>

static long perf_event_open(struct perf_event_attr *attr, pid_t pid, int cpu, int group_fd, unsigned long flags) {
    return syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
}

// The performance-core PMU has its own "type" number (typically 4). Read it at
// runtime; if the file is missing (non-hybrid CPU) fall back to PERF_TYPE_RAW.
static uint32_t pcore_pmu_type() {
    FILE *f = fopen("/sys/devices/cpu_core/type", "r");
    if (!f) return PERF_TYPE_RAW;
    unsigned t = 0;
    if (fscanf(f, "%u", &t) != 1) { fclose(f); return PERF_TYPE_RAW; }
    fclose(f);
    return (uint32_t)t;
}

// Raw event codes (config = event | (umask<<8)) for Raptor Cove P-cores.
static const uint64_t EV_L1_MISS      = 0x08d1;  // MEM_LOAD_RETIRED.L1_MISS
static const uint64_t EV_L2_MISS      = 0x10d1;  // MEM_LOAD_RETIRED.L2_MISS
static const uint64_t EV_L3_MISS      = 0x20d1;  // MEM_LOAD_RETIRED.L3_MISS
static const uint64_t EV_INSTRUCTIONS = 0x00c0;  // INST_RETIRED.ANY
static const uint64_t EV_CYCLES       = 0x003c;  // CPU_CLK_UNHALTED.THREAD_P
static const uint64_t EV_BRANCH_MISS  = 0x00c5;  // BR_MISP_RETIRED.ALL_BRANCHES

// ----------------------------------------------------------------------------
// One hardware counter, with Leis-style multiplexing correction.
//
// We ask the kernel to also report TOTAL_TIME_ENABLED (how long the counter was
// supposed to be running) and TOTAL_TIME_RUNNING (how long it actually had a
// physical register). If those differ (because more events than registers were
// requested), value*enabled/running rescales to the true estimate.
//
// Usage: reset() snapshots a "prev" baseline; read_value() snapshots "now" and
// returns the multiplexing-corrected DELTA since reset(). enable()/disable()
// only pause and resume counting without re-baselining: while disabled, the
// counter and its enabled/running clocks freeze, so paused periods (e.g. the
// queries during the insert pass) are excluded from the delta.
// ----------------------------------------------------------------------------
class PerfEvent {
public:
    struct Sample { uint64_t value = 0, enabled = 0, running = 0; };

    int fd = -1;
    bool ok = false;
    std::string name;
    Sample prev;

    PerfEvent(uint32_t type, uint64_t config, const std::string &label) : name(label) {
        struct perf_event_attr attr;
        memset(&attr, 0, sizeof(attr));
        attr.type = type;
        attr.size = sizeof(attr);
        attr.config = config;
        attr.disabled = 1;
        attr.exclude_kernel = 1;   // user-space only
        attr.exclude_hv = 1;
        attr.read_format = PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING;

        fd = perf_event_open(&attr, 0, -1, -1, 0);
        if (fd == -1) {
            ok = false;
            std::cerr << "[perf] could not open counter '" << name
                      << "': " << strerror(errno) << "\n";
            if (errno == EACCES || errno == EPERM) {
                std::cerr << "[perf] HINT: hardware counters are locked down. Try:\n"
                          << "[perf]   sudo sysctl -w kernel.perf_event_paranoid=1\n";
            }
        } else {
            ok = true;
        }
    }

    // Read the raw tri(value, time_enabled, time_running) from the fd.
    Sample sample() {
        Sample s;
        if (!ok) return s;
        uint64_t buf[3] = {0, 0, 0};
        if (read(fd, buf, sizeof(buf)) != (ssize_t)sizeof(buf)) return s;
        s.value = buf[0]; s.enabled = buf[1]; s.running = buf[2];
        return s;
    }

    void reset() {
        if (!ok) return;
        ioctl(fd, PERF_EVENT_IOC_RESET, 0);
        prev = sample();              // baseline for the upcoming measured region
    }

    void enable() { if (ok) ioctl(fd, PERF_EVENT_IOC_ENABLE, 0); }
    void disable(){ if (ok) ioctl(fd, PERF_EVENT_IOC_DISABLE, 0); }

    // Multiplexing-corrected delta since enable().
    double read_value() {
        if (!ok) return 0.0;
        Sample now = sample();
        double dval = (double)(now.value   - prev.value);
        double den  = (double)(now.enabled - prev.enabled);
        double run  = (double)(now.running - prev.running);
        double corr = (run > 0.0) ? den / run : 1.0;   // =1 when no multiplexing
        return dval * corr;
    }

    ~PerfEvent() { if (fd != -1) close(fd); }
};

// ----------------------------------------------------------------------------
// All metrics for a measured region.
// ----------------------------------------------------------------------------
struct CacheStats {
    double l1_miss = 0;       // retired loads that missed L1
    double l2_miss = 0;       // retired loads that missed L2
    double l3_miss = 0;       // retired loads that missed L3 (RAM trips)
    double instructions = 0;  // retired instructions
    double cycles = 0;        // core cycles
    double branch_misses = 0; // mispredicted branches

    double ipc() const { return cycles > 0 ? instructions / cycles : 0.0; }
};

// ----------------------------------------------------------------------------
// All six counters as a unit. The i7-14700K P-core has at least six general
// counters, so they normally fit without multiplexing.
// ----------------------------------------------------------------------------
class CacheProfiler {
public:
    uint32_t type;
    PerfEvent l1m, l2m, l3m, inst, cyc, brm;

    CacheProfiler()
        : type(pcore_pmu_type()),
          l1m (type, EV_L1_MISS,      "L1-load-miss"),
          l2m (type, EV_L2_MISS,      "L2-load-miss"),
          l3m (type, EV_L3_MISS,      "L3-load-miss"),
          inst(type, EV_INSTRUCTIONS, "instructions"),
          cyc (type, EV_CYCLES,       "cycles"),
          brm (type, EV_BRANCH_MISS,  "branch-misses") {}

    bool available() const {
        return l1m.ok && l2m.ok && l3m.ok && inst.ok && cyc.ok && brm.ok;
    }

    void reset()   { l1m.reset();   l2m.reset();   l3m.reset();   inst.reset();   cyc.reset();   brm.reset(); }
    void enable()  { l1m.enable();  l2m.enable();  l3m.enable();  inst.enable();  cyc.enable();  brm.enable(); }
    void disable() { l1m.disable(); l2m.disable(); l3m.disable(); inst.disable(); cyc.disable(); brm.disable(); }

    CacheStats read() {
        CacheStats s;
        s.l1_miss       = l1m.read_value();
        s.l2_miss       = l2m.read_value();
        s.l3_miss       = l3m.read_value();
        s.instructions  = inst.read_value();
        s.cycles        = cyc.read_value();
        s.branch_misses = brm.read_value();
        return s;
    }
};

#endif // PERF_COUNTER_H
