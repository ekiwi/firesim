#ifndef __COVERAGE_H
#define __COVERAGE_H

#include <fstream>
#include "bridge_driver.h"

// TODO


#ifdef COVERAGEBRIDGEMODULE_struct_guard

class coverage_t: public bridge_driver_t
{

    public:
        coverage_t(simif_t* sim,
                 std::vector<std::string> &args,
                 COVERAGEBRIDGEMODULE_struct * mmio_addrs,
                 unsigned int counter_width,
                 unsigned int cover_count,
                 const char* const* covers);
        ~coverage_t();
        virtual void init();
        virtual void tick();
        virtual bool terminate() { return false; }
        virtual int exit_code() { return 0; }
        virtual void finish();
    private:
        COVERAGEBRIDGEMODULE_struct * mmio_addrs;
        const unsigned int counter_width;
        const unsigned int cover_count;
        const char* const* covers;
        bool scanning;

};

#endif // COVERAGEBRIDGEMODULE_struct_guard


#endif //__COVERAGE_H
