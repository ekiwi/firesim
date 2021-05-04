#ifdef COVERAGEBRIDGEMODULE_struct_guard

#include "coverage.h"

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
    covers(covers)
{
}

coverage_t::~coverage_t() {
    free(this->mmio_addrs);
}

void coverage_t::init() {
}

void coverage_t::tick() {
}

void coverage_t::finish() {
}

#endif // COVERAGEBRIDGEMODULE_struct_guard