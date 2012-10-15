// Copyright (c) 2012      Bryce Adelstein-Lelbach
// Copyright (c) 2003-2012 Christopher M. Kohlhoff (chris at kohlhoff dot com)
// Copyright (c) 2012      Mehmet Balman
// Copyright (c) 2012      Aydin Buluc
// Copyright (c) 2012      Hartmut Kaiser
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "control_case_archive.hpp"
#include "high_resolution_timer.hpp"

#include <boost/lexical_cast.hpp>
#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <boost/random.hpp>
#include <boost/format.hpp>

#include <future>
#include <iostream>
#include <vector>
#include <iterator>

#include <sched.h>
#include <time.h>

using boost::program_options::variables_map;
using boost::program_options::options_description;
using boost::program_options::value;
using boost::program_options::store;
using boost::program_options::command_line_parser;
using boost::program_options::notify;

using boost::asio::ip::tcp;

#if defined(CHECK_DATA)
    std::vector<double> correct_data;
    
    template <typename T>
    inline bool compare_floating(T const& x, T const& y)
    {
        T const epsilon = 1e-8;
     
        if ((x + epsilon >= y) && (x - epsilon <= y))
            return true;
        else
            return false;
    }
#endif

// Receive, then send.
void receive(
    control_case_oarchive& sender
  , control_case_iarchive& receiver
  , std::vector<double>& data
  , boost::uint64_t iteration
    )
{
    receiver.read(data);

#if defined(CHECK_DATA)
    if (data.size() != correct_data.size())
    {
        std::cout << "ERROR (receive): got vector of size "
                  << data.size()
                  << ", expected vector of size " << correct_data.size()
                  << " (iteration " << iteration << ")\n";
    }

    for (std::size_t i = 0; i < data.size(); ++i)
    {
        if (!compare_floating(data[i], correct_data[i]))
        {
            std::cout << "ERROR (receive): got " << data[i]
                      << " as the value for element " << i
                      << ", expected " << correct_data[i]
                      << " (iteration " << iteration << ")\n";
        }
    }
#endif
}

// Send, then receive. 
void send(
    control_case_oarchive& sender
  , control_case_iarchive& receiver
  , std::vector<double>& data
  , boost::uint64_t iteration
    )
{
#if defined(CHECK_DATA)
    if (data.size() != correct_data.size())
    {
        std::cout << "ERROR (send): got vector of size "
                  << data.size()
                  << ", expected vector of size " << correct_data.size()
                  << " (iteration " << iteration << ")\n";
    }

    for (std::size_t i = 0; i < data.size(); ++i)
    {
        if (!compare_floating(data[i], correct_data[i]))
        {
            std::cout << "ERROR (send): got " << data[i]
                      << " as the value for element " << i
                      << ", expected " << correct_data[i]
                      << " (iteration " << iteration << ")\n";
        }
    }
#endif

    sender.write(data);
}

// Generate a vector of doubles filled with random data.
void generate_data(
    std::vector<double>& data
  , boost::uint64_t vector_size
  , boost::uint64_t seed
    )
{
    boost::random::mt19937_64 prng(seed);
    boost::random::uniform_01<> dst;
    std::back_insert_iterator<std::vector<double> > it(data);
    std::generate_n(it, vector_size, boost::bind(dst, boost::ref(prng))); 
}

std::string server_main(variables_map& vm)
{
    std::string host = vm["host"].as<std::string>();
    boost::uint16_t port = boost::lexical_cast<boost::uint16_t>
        (vm["port"].as<std::string>());
 
    boost::uint64_t vector_size = vm["vector-size"].as<boost::uint64_t>();
    boost::uint64_t iterations = vm["iterations"].as<boost::uint64_t>();
    boost::uint64_t seed = vm["seed"].as<boost::uint64_t>();
 
    boost::asio::io_service io_service;

    tcp::acceptor acceptor(io_service, tcp::endpoint(tcp::v4(), port));
    acceptor.set_option(tcp::acceptor::reuse_address(true));
    acceptor.set_option(tcp::acceptor::linger(true, 0));

    tcp::socket s(io_service);
    control_case_oarchive sender(s);
    control_case_iarchive receiver(s);

    // Start accepting connections.
    acceptor.accept(s);

    // Generate a vector of doubles filled with random data.
    std::vector<double> data;

#if defined(CHECK_DATA)
    generate_data(correct_data, vector_size, seed);
#endif

    // Start timing.
    high_resolution_timer clock;

    for (std::size_t i = 0; i < iterations; ++i)
    {
        if (i % 2)
            receive(sender, receiver, data, iterations);  
        else            
            send(sender, receiver, data, iterations);  
    }

    double elapsed = clock.elapsed();

    return boost::str(boost::format(
        "server seed=%1% vector-size=%2%[double] iterations=%3% walltime=%4%[s]"
        ) % seed % vector_size % iterations % elapsed);
}

std::string client_main(variables_map& vm)
{
    std::string host = vm["host"].as<std::string>();
    std::string port = vm["port"].as<std::string>();
 
    boost::uint64_t vector_size = vm["vector-size"].as<boost::uint64_t>();
    boost::uint64_t iterations = vm["iterations"].as<boost::uint64_t>();
    boost::uint64_t seed = vm["seed"].as<boost::uint64_t>();

    boost::asio::io_service io_service;

    // Resolve the target's address.
    tcp::resolver resolver(io_service);
    tcp::resolver::query query(tcp::v4(), host, port);
    tcp::resolver::iterator iterator = resolver.resolve(query);

    tcp::socket s(io_service);
    control_case_oarchive sender(s);
    control_case_iarchive receiver(s);

    // Connect to the target.
    boost::asio::connect(s, iterator);
    s.set_option(tcp::socket::reuse_address(true));
    s.set_option(tcp::socket::linger(true, 0));

    // Generate a vector of doubles filled with random data.
    std::vector<double> data;
    generate_data(data, vector_size, seed);

#if defined(CHECK_DATA)
    generate_data(correct_data, vector_size, seed);
#endif

    // Start timing.
    high_resolution_timer clock;

    for (std::size_t i = 0; i < iterations; ++i)
    {
        if (i % 2)
            send(sender, receiver, data, iterations);  
        else            
            receive(sender, receiver, data, iterations);  
    }

    double elapsed = clock.elapsed();

    return boost::str(boost::format(
        "server seed=%1% vector-size=%2%[double] iterations=%3% walltime=%4%[s]"
        ) % seed % vector_size % iterations % elapsed);
}

int main(int argc, char** argv)
{
    // Parse command line.
    variables_map vm;

    options_description
        cmdline("Usage: control_case_test <-s|-c|-b> [options]");

    cmdline.add_options()
        ( "help,h"
        , "print out program usage (this message)")

        ( "server,s", "run as the server")
        
        ( "client,c", "run as the client")

        ( "both,b", "run both the server and client")

        ( "host"
        , value<std::string>()->default_value("localhost")
        , "hostname or IP to send to")

        ( "port"
        , value<std::string>()->default_value("9000")
        , "TCP port to connect to")

        ( "vector-size", value<boost::uint64_t>()->default_value(128),
          "number of elements (doubles) to send/receive")

        ( "iterations", value<boost::uint64_t>()->default_value(4096),
          "number of iterations")

        ( "seed"
        , value<boost::uint64_t>()->default_value(1337)
        , "seed for the pseudo random number generator")
    ;

    store(command_line_parser(argc, argv).options(cmdline).run(), vm);

    notify(vm);

    // Print help screen.
    if (vm.count("help"))
    {
        std::cout << cmdline;
        return 1;
    }

    if (!vm.count("server") && !vm.count("client") && !vm.count("both"))
    {
        std::cout << "ERROR: must specify either --server, --client "
                  << "or --both\n"
                  << cmdline;
        return 1;
    }

    if (1 != (vm.count("server") + vm.count("client") + vm.count("both")))
    {
        std::cout << "ERROR: only one of--server, --client and --both may "
                  << "be specified\n"
                  << cmdline;
        return 1;
    }

    if      (vm.count("server"))
        std::cout << server_main(vm) << "\n";
    else if (vm.count("client"))
        std::cout << client_main(vm) << "\n";
    else
    {
        auto server = std::async(std::launch::async, server_main, vm);

        // Sleep for 1000 nanoseconds.
        timespec rqtp = { 0, 1000 };
        nanosleep(&rqtp, 0);

        auto client = std::async(std::launch::async, client_main, vm);

        std::cout << server.get() << "\n"
                  << client.get() << "\n";
    }

    return 0;
}

