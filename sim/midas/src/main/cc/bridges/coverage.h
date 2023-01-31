#ifndef __COVERAGE_H
#define __COVERAGE_H

#include <fstream>
#include <cstdint>
#include "bridge_driver.h"

// TODO


#ifdef COVERAGEBRIDGEMODULE_struct_guard

class coverage_t: public bridge_driver_t
{

    public:
        coverage_t(simif_t* sim,
                 std::vector<std::string> &args,
                 COVERAGEBRIDGEMODULE_struct * mmio_addrs,
                 unsigned int stream_idx,
                 unsigned int stream_depth,
                 unsigned int counter_width,
                 unsigned int cover_count,
                 unsigned int counters_per_beat,
                 const char* const* covers);
        ~coverage_t();
        virtual void init();
        virtual void tick();
        virtual bool terminate() { return false; }
        virtual int exit_code() { return 0; }
        virtual void finish();
    private:
        void read_counts();
        COVERAGEBRIDGEMODULE_struct * mmio_addrs;
        const unsigned int stream_idx;
        const unsigned int stream_depth;
        const unsigned int counter_width;
        const unsigned int cover_count;
        const unsigned int counters_per_beat;
        const char* const* covers;
        bool scanning;
        size_t counter_index;
        // Stream batching parameters
        static constexpr size_t beat_bytes = BridgeConstants::STREAM_WIDTH_BYTES;
        // The number of stream beats to pull off the FPGA on each invocation of
        // tick() This will be set based on the ratio of token_size :
        // desired_batch_beats
        size_t batch_beats;
        // This will be modified to be a multiple of the token size
        const size_t desired_batch_beats = stream_depth / 2;        std::ofstream printfile;
        uint64_t sim_start_time;
        uint64_t scan_start_time;
        bool first_tick;

};

#endif // COVERAGEBRIDGEMODULE_struct_guard


#endif //__COVERAGE_H
