//  Copyright (c) 2012 Bryce Adelstein-Lelbach
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#if !defined(ZEE731C57_770E_4889_86F4_01397AA0F38C)
#define ZEE731C57_770E_4889_86F4_01397AA0F38C

#include <boost/ref.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/serialization/access.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/type_traits/is_arithmetic.hpp>

#include <vector>

#include "container_device.hpp"

#include "portable_binary_iarchive.hpp"
#include "portable_binary_oarchive.hpp"

// NOTE: These classes don't provide the async_read/async_write functions that
// the zero_copy archives do, as they are not needed for the benchmark.

// NOTE: Not using the actual Boost.Endian code to avoid copying more stuff
// over to this git repository.
namespace boost { namespace integer { typedef boost::uint64_t ulittle64_t; }}

struct control_case_oarchive
  : boost::enable_shared_from_this<control_case_oarchive>
{
    typedef boost::mpl::false_ is_loading;
    typedef boost::mpl::true_ is_saving;

  private:
    boost::asio::ip::tcp::socket* socket_;

    std::vector<char> buffer_;
    boost::integer::ulittle64_t size_; // buffer_.size() 

  public:
    control_case_oarchive(
        boost::asio::ip::tcp::socket& socket
        )
      : socket_(&socket)
      , buffer_()
      , size_(0)
    {}

    ~control_case_oarchive()
    {
        // Gracefully and portably shutdown the socket.
        boost::system::error_code ec;
        socket_->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        socket_->close(ec);
    }

    // Synchronously write a data structure to the socket.
    template <typename Parcel>
    void write(Parcel const& p)
    {
        typedef container_device<std::vector<char> > io_device_type;
        boost::iostreams::stream<io_device_type> io(buffer_);

        {
            // Serialize the slow way.
            portable_binary_oarchive archive(io);
            archive & p;
        }

        size_ = buffer_.size();

        std::vector<boost::asio::const_buffer> message;
        message.push_back(boost::asio::buffer(&size_, sizeof(size_)));
        message.push_back(boost::asio::buffer(buffer_));

        boost::asio::write(*socket_, message);

        size_ = 0;
    }
};

// Note: We must "deserialize" the object BEFORE we read the data, but AFTER
// we have read the sizes. This allows us to do zero-copy, because we know the
// layout of the data structure before we call async_read.
struct control_case_iarchive : boost::enable_shared_from_this<control_case_oarchive>
{
    typedef boost::mpl::true_ is_loading;
    typedef boost::mpl::false_ is_saving;

  private:
    boost::asio::ip::tcp::socket* socket_;

    std::vector<char> buffer_;
    boost::integer::ulittle64_t size_; // buffer_.size() 

  public:
    control_case_iarchive(
        boost::asio::ip::tcp::socket& socket 
        )
      : socket_(&socket)
      , buffer_()
      , size_(0)
    {}

    ~control_case_iarchive()
    {
        // Gracefully and portably shutdown the socket.
        boost::system::error_code ec;
        socket_->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        socket_->close(ec);
    }

    // Synchronously read a data structure from the socket.
    template <typename Parcel>
    void read(Parcel& p)
    {
        // The first thing we need is the size of the incoming data. 
        boost::asio::read(*socket_, boost::asio::buffer(&size_, sizeof(size_)));

        buffer_.resize(size_);

        boost::asio::read(*socket_, boost::asio::buffer(buffer_));

        typedef container_device<std::vector<char> > io_device_type;
        boost::iostreams::stream<io_device_type> io(buffer_);

        {
            // Deserialize the slow way.
            portable_binary_oarchive archive(io);
            archive & p;
        }

        size_ = 0;
    }
};

#endif

