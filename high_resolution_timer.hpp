//  Copyright (c) 2005-2012 Hartmut Kaiser
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#if !defined(HIGH_RESOLUTION_TIMER_MAR_24_2008_1222PM)
#define HIGH_RESOLUTION_TIMER_MAR_24_2008_1222PM

#include <boost/cstdint.hpp>
#include <boost/chrono/chrono.hpp>

struct high_resolution_clock
{
    // This function returns a tick count with a resolution (not
    // precision!) of 1 ns.
    static boost::uint64_t now()
    {
        boost::chrono::nanoseconds ns =
            boost::chrono::high_resolution_clock::now().time_since_epoch();
        BOOST_ASSERT(ns.count() >= 0);
        return static_cast<boost::uint64_t>(ns.count());
    }

    // This function returns the smallest representable time unit as 
    // returned by this clock.
    static boost::uint64_t (min)()
    {
        typedef boost::chrono::duration_values<boost::chrono::nanoseconds>
            duration_values;
        return (duration_values::min)().count();
    }

    // This function returns the largest representable time unit as 
    // returned by this clock.
    static boost::uint64_t (max)()
    {
        typedef boost::chrono::duration_values<boost::chrono::nanoseconds>
            duration_values;
        return (duration_values::max)().count();
    }
};

///////////////////////////////////////////////////////////////////////////
//
//  high_resolution_timer
//      A timer object measures elapsed time.
//      
///////////////////////////////////////////////////////////////////////////
class high_resolution_timer
{
public:
    high_resolution_timer()
      : start_time_(take_time_stamp())
    {
    }

    high_resolution_timer(double t)
      : start_time_(static_cast<boost::uint64_t>(t * 1e9))
    {}

    high_resolution_timer(high_resolution_timer const& rhs)
      : start_time_(rhs.start_time_)
    {}

    static double now()
    {
        return take_time_stamp() * 1e-9;
    }

    void restart()
    {
        start_time_ = take_time_stamp();
    }
    double elapsed() const                  // return elapsed time in seconds
    {
        return double(take_time_stamp() - start_time_) * 1e-9;
    }

    boost::int64_t elapsed_microseconds() const
    {
        return boost::int64_t((take_time_stamp() - start_time_) * 1e-3);
    }

    boost::int64_t elapsed_nanoseconds() const
    {
        return boost::int64_t(take_time_stamp() - start_time_);
    }

    double elapsed_max() const   // return estimated maximum value for elapsed()
    {
        return (high_resolution_clock::max)() * 1e-9;
    }

    double elapsed_min() const   // return minimum value for elapsed()
    {
        return (high_resolution_clock::min)() * 1e-9;
    }

protected:
    static boost::uint64_t take_time_stamp()
    {
        return high_resolution_clock::now();
    }

private:
    boost::uint64_t start_time_;
};

#endif

