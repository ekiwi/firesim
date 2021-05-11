#ifdef COVERAGEBRIDGEMODULE_struct_guard

#include "coverage.h"
#include <iostream>
#include <cassert>
// Use (void) to silent unused warnings.
#define assertm(exp, msg) assert(((void)msg, exp))

// signal from serial module to coverage
bool coverage_start_scanning = false;
// signal from coverage to serial module
bool coverage_done_scanning = false;


coverage_t::coverage_t(
    simif_t* sim,
    std::vector<std::string> &args,
    COVERAGEBRIDGEMODULE_struct * mmio_addrs,
    unsigned int dma_address,
    unsigned int counter_width,
    unsigned int cover_count,
    unsigned int counters_per_beat,
    const char* const* covers) :
    bridge_driver_t(sim),
    mmio_addrs(mmio_addrs),
    dma_address(dma_address),
    counter_width(counter_width),
    cover_count(cover_count),
    counters_per_beat(counters_per_beat),
    covers(covers),
    scanning(false),
    counters(nullptr),
    counter_index(0)
{
  assertm(counter_width <= 64, "The maximum counter size supported by the C++ code is 64-bit!");
}

coverage_t::~coverage_t() {
    free(this->mmio_addrs);
}

void coverage_t::init() {
}

void coverage_t::read_counts() {
    const auto available = read(mmio_addrs->outgoing_count);

    // std::cout << "[COVERAGE] available=" << available << std::endl;

    if(available > 0) {
        // dynamic stack allocation .... messy, but it seems to work
        const auto batch_bytes = available * beat_bytes;
        alignas(4096) unsigned char buf[batch_bytes];
        uint32_t bytes_received = pull(dma_address, (char*)buf, batch_bytes);
        if (bytes_received != batch_bytes) {
            std::cerr << "[COVERAGE] ERR MISMATCH! on reading print tokens. Read " << bytes_received
                      << " bytes, wanted " << batch_bytes << " bytes." << std::endl;
            std::cerr << "[COVERAGE] errno: " << strerror(errno) << std::endl;
            exit(1);
        }

        // convert bytes read into cover counts
        const auto received_covers = available * counters_per_beat;

        // std::cout << "[COVERAGE] received " << received_covers << " cover counts" << std::endl;

        for(size_t ii = 0; ii < received_covers; ii++) {
            // leave the loop if we have already updated all counters
            if(counter_index >= cover_count) break;

            // for counter sizes smaller than 8 (1, 2 or 4 bits) we need some special handling
            if(counter_width < 8) {
                assertm(false, "TODO: support counters with smaller bit widths!");
            } else {
                // counter of size 8 or larger are byte aligned!
                assert((counter_width / 8) * 8 == counter_width);
                const auto counter_bytes = counter_width / 8;
                const auto counter_offset = ii * counter_bytes;

                uint64_t value = 0;
                for(size_t b = 0; b < counter_bytes; b++) {
                    value |= (static_cast<uint64_t>(buf[counter_offset + b]) << (8 * b));
                }


                // debug print
                if(counter_index <= 16) {
                    std::cout << "[COVERAGE] " << covers[counter_index] << "=" << value
                              << "; counter_offset=" << counter_offset
                              << "; counter_bytes=" << counter_bytes
                              << std::endl;
                    for(size_t b = 0; b < counter_bytes; b++) {
                        std::cout << "value |= (buf[" << (counter_offset + b) << "]) << (" << (8 * b) << "));"
                                  << "; buf[...]=" << static_cast<uint64_t>(buf[counter_offset + b])
                                  << std::endl;
                    }
                }

                counters[counter_index] = value;
                counter_index++;
            }
        }
    }
}

void coverage_t::tick() {
    if(coverage_start_scanning) {
      std::cout << "[COVERAGE] starting to scan" << std::endl;
      // allocate space for the results
      counters = new uint64_t[cover_count];
      counter_index = 0;

      // reset the flag
      coverage_start_scanning = false;

      // signal the HW that we want to start scanning now
      write(mmio_addrs->start_scanning, 1);
      scanning = true;
    }

    if(scanning) {
        read_counts();
        // we are done when the hardware is no longer in scanning mode and when we have read all counters
        const auto hw_scanning = read(mmio_addrs->scanning) == 1;
        const auto done = !hw_scanning && (counter_index >= cover_count);
        if(done) {
            scanning = false;
            // signal the serial bridge that it can stop the simulation now
            coverage_done_scanning = true;
            std::cout << "[COVERAGE] done scanning" << std::endl;
        }
    }
}

void coverage_t::finish() {
}

#endif // COVERAGEBRIDGEMODULE_struct_guard