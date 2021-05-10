#ifdef COVERAGEBRIDGEMODULE_struct_guard

#include "coverage.h"
#include <iostream>

// signal from serial module to coverage
bool coverage_start_scanning = false;
// signal from coverage to serial module
bool coverage_done_scanning = false;


coverage_t::coverage_t(
    simif_t* sim,
    std::vector<std::string> &args,
    COVERAGEBRIDGEMODULE_struct * mmio_addrs,
    unsigned int counter_width,
    unsigned int cover_count,
    const char* const* covers) :
    bridge_driver_t(sim),
    mmio_addrs(mmio_addrs),
    counter_width(counter_width),
    cover_count(cover_count),
    covers(covers),
    scanning(false)
{
}

coverage_t::~coverage_t() {
    free(this->mmio_addrs);
}

void coverage_t::init() {
}

void coverage_t::tick() {
    if(coverage_start_scanning) {
      std::cout << "[COVERAGE] starting to scan" << std::endl;
      coverage_start_scanning = false;
      write(mmio_addrs->start_scanning, 1);
      scanning = true;
    }

    if(scanning) {
        // TODO: read out values and set "done_scanning"
        const auto done = read(mmio_addrs->scanning) == 0;
        if(done) {
            scanning = false;
            coverage_done_scanning = true;
            std::cout << "[COVERAGE] done scanning" << std::endl;
        }
    }
}

void coverage_t::finish() {
}

#endif // COVERAGEBRIDGEMODULE_struct_guard