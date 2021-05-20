#ifdef COVERAGEBRIDGEMODULE_struct_guard

#include "coverage.h"
#include <iostream>
#include <cassert>
// Use (void) to silent unused warnings.
#define assertm(exp, msg) assert(((void)msg, exp))

// signal from serial module to coverage
extern bool coverage_start_scanning;
// signal from coverage to serial module
extern bool coverage_done_scanning;
// signals whether coverage bridge is installed
extern bool coverage_available;

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
    counter_index(0),
    first_tick(true)
{
  assertm(counter_width <= 64, "The maximum counter size supported by the C++ code is 64-bit!");

  // parse the arguments to find the name of the output file
  std::string filename = "";
  const std::string arg_name("+cover-json=");
  for(auto arg : args) {
    if(arg.find(arg_name) == 0) {
        filename = arg.erase(0, arg_name.length());
    }
  }
  if(filename.length() == 0) {
    std::cerr << "[COVERAGE] missing +cover-json argument, will be disabled!" << std::endl;
  } else {
    printfile.open(filename, std::ios_base::out | std::ios_base::binary);
    if(!printfile.is_open()) {
        std::cerr << "[COVERAGE] failed to open " << filename << " for writing, coverage will be disabled!" << std::endl;
    } else {
        coverage_available = true;

        // write start of JSON file
        printfile << "[" << std::endl << "{\"class\":\"chiseltest.coverage.TestCoverage\",\"counts\":[" << std::endl;
    }
  }
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

            uint64_t value = 0;

            // for counter sizes smaller than 8 (1, 2 or 4 bits) we need some special handling
            if(counter_width < 8) {
                assert(counter_width == 1 || counter_width == 2 || counter_width == 4);
                const auto counter_byte = (ii * counter_width) / 8;
                const auto mask = (1 << counter_width) - 1;
                const auto bit_offset = 7 - ((ii * counter_width) % 8);

                assert(counter_byte < bytes_received); // check for out of bounds access
                assert(bit_offset >= 0 && bit_offset < 8);

                value = (static_cast<uint64_t>(buf[counter_byte]) >> bit_offset) & mask;
            } else {
                // counter of size 8 or larger are byte aligned!
                assert((counter_width / 8) * 8 == counter_width);
                const auto counter_bytes = counter_width / 8;
                const auto counter_offset = ii * counter_bytes;

                for(size_t b = 0; b < counter_bytes; b++) {
                    value |= (static_cast<uint64_t>(buf[counter_offset + b]) << (8 * b));
                }
            }

            // debug print
            //if(counter_index <= 17) {
            //    std::cout << "[COVERAGE] " << covers[counter_index] << "=" << value << std::endl;
            //}

            // print to file
            const auto is_last = counter_index + 1 == cover_count;
            if(!is_last) {
                printfile << "{\"" << covers[counter_index] << "\":" << value << "}," << std::endl;
            } else {
                printfile << "{\"" << covers[counter_index] << "\":" << value << "}" << std::endl
                          << "]}" << std::endl << "}" << std::endl;
                printfile.close();
            }

            counter_index++;
        }
    }
}

void coverage_t::tick() {
    if(first_tick) {
        first_tick = false;
        sim_start_time = timestamp();
    }

    // check if coverage is disabled
    if(!coverage_available) return;

    if(coverage_start_scanning) {
      scan_start_time = timestamp();
      std::cout << "[COVERAGE] starting to scan" << std::endl;
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

            // report times
            const uint64_t end = timestamp();
            const auto sim_time = diff_secs(end, sim_start_time);
            const auto scan_time = diff_secs(end, scan_start_time);
            std::cout << "[COVERAGE] " << scan_time << "s (" << (scan_time / sim_time * 100.0)
                      << "% of total " << sim_time << "s simulation time) spent scanning out coverage"
                      << std::endl;
        }
    }
}

void coverage_t::finish() {
}

#endif // COVERAGEBRIDGEMODULE_struct_guard