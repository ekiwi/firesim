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
                 unsigned int dma_address,
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
        const unsigned int dma_address;
        const unsigned int counter_width;
        const unsigned int cover_count;
        const unsigned int counters_per_beat;
        const char* const* covers;
        bool scanning;
        uint64_t* counters;
        size_t counter_index;
        const size_t beat_bytes  = DMA_DATA_BITS / 8;

};

#endif // COVERAGEBRIDGEMODULE_struct_guard


#endif //__COVERAGE_H
