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

// NOTE: Not using the actual Boost.Endian code to avoid copying more stuff
// over to this git repository.
namespace boost { namespace integer { typedef boost::uint64_t ulittle64_t; }}

template <typename T, typename enable = void>
struct is_bitwise_serializable : boost::is_arithmetic<T>::type { };

template <typename T>
struct is_bitwise_serializable<const T> : is_bitwise_serializable<T> { };

template <typename T>
struct is_bitwise_serializable<T&> : is_bitwise_serializable<T> { };

template <typename T>
struct is_bitwise_serializable<T&&> : is_bitwise_serializable<T> { };

// This specialization has to be done not just for std::vector, but for other
// special cases of boost::asio::buffer, such as boost::array, std::array,
// perhaps std::valarray too.
template <typename T>
struct is_bitwise_serializable<std::vector<T> > : is_bitwise_serializable<T> { };

// We never directly serialize an std::vector; we actually only serialize one
// type (a parcel). Parcels contain a polymorphic object (an action) that has
// all our data in it. Because of this, I believe we can safely do zero-copy
// for std::vector and other none polymorphic types. On the receiving end, we
// will know how to read the data because the polymorphic type was serialized
// normally through Boost.Serialization.
struct zero_copy_oarchive : boost::enable_shared_from_this<zero_copy_oarchive>
{
    typedef std::function<void()> handler_type;

    typedef boost::mpl::false_ is_loading;
    typedef boost::mpl::true_ is_saving;

  private:
    boost::asio::ip::tcp::socket* socket_;

    handler_type handler_;

    bool homogeneity_; ///< Is it safe to do bitwise serialization? E.g. does
                       ///  the target have the endianness as us, etc?

    std::vector<boost::asio::const_buffer> message_;
    std::vector<boost::integer::ulittle64_t> chunk_sizes_;
    boost::integer::ulittle64_t chunks_; // chunk_sizes_.size()

    std::vector<std::vector<char> > slow_buffers_;

  public:
    zero_copy_oarchive(
        boost::asio::ip::tcp::socket& socket
      , bool homogeneity = true
        )
      : socket_(&socket)
      , handler_()
      , homogeneity_(homogeneity)
      , message_()
      , chunk_sizes_()
      , chunks_()
      , slow_buffers_()
    {}

    ~zero_copy_oarchive()
    {
        // Gracefully and portably shutdown the socket.
        boost::system::error_code ec;
        socket_->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        socket_->close(ec);
    }

    template <typename T>
    void operator& (T const& t) { dispatch(t); }

    template <typename T>
    void operator<< (T const& t) { dispatch(t); }

    // NOTE: The lifetime of the data we're serializing is controlled, so t
    // going out of scope isn't an issue.
    template <typename T>
    void dispatch(T const& t)
    {
        typedef typename is_bitwise_serializable<T>::type predicate_type;

        if (homogeneity_ && predicate_type::value)
            save<T>::call(this, t);
        else
            slow_save(t);
    }

    template <typename T>
    struct save
    {
        static void call(zero_copy_oarchive* self, T const& t)
        {
            self->message_.push_back(boost::asio::buffer(&t, sizeof(t)));
        }
    };

    // This specialization has to be done not just for std::vector, but for
    // other special cases of boost::asio::buffer, such as boost::array,
    // std::array, perhaps std::valarray too.
    template <typename T>
    struct save<std::vector<T> >
    {
        static void call(zero_copy_oarchive* self, std::vector<T> const& t)
        {
            // Save the size, so we can know how much to read on the other end.
            // This allows us to do zero copy when reading.
            self->chunk_sizes_.push_back(t.size());

            self->message_.push_back(boost::asio::buffer(t));
        } 
    };

    template <typename T>
    void slow_save(T&& t)
    {
        slow_buffers_.push_back(std::vector<char>());
        std::vector<char>& slow_buffer_ = slow_buffers_.back();

        typedef container_device<std::vector<char> > io_device_type;
        boost::iostreams::stream<io_device_type> io(slow_buffer_);

        {
            // Serialize t the slow way.
            portable_binary_oarchive archive(io);
            archive & t;
        }

        // Save the size, so we can know how much to read on the other end.
        // This allows us to do zero copy when reading.
        chunk_sizes_.push_back(slow_buffer_.size());

        message_.push_back(boost::asio::buffer(slow_buffer_));
    }

    // Synchronously write a data structure to the socket.
    template <typename Parcel>
    void write(Parcel const& p)
    {
        // The first buffer is the number of elements in the list. The second
        // buffer is our list of sizes. We'll fill these in later.
        message_.push_back(boost::asio::buffer(&chunks_, sizeof(chunks_)));
        message_.push_back(boost::asio::const_buffer());

        *this & p;

        // NOTE: Non-container chunks (e.g. single elements) are not in the size
        // list.
        chunks_ = chunk_sizes_.size();
        message_.at(1) = boost::asio::buffer(chunk_sizes_);

        boost::asio::write(*socket_, message_);

        message_.clear();
        chunk_sizes_.clear();
        chunks_ = 0;
        slow_buffers_.clear();
    }

    // Asynchronously write a data structure to the socket.
    template <typename Parcel>
    void async_write(Parcel const& p, handler_type const& h = handler_type())
    {
        handler_ = h;

        // The first buffer is the number of elements in the list. The second
        // buffer is our list of sizes. We'll fill these in later.
        message_.push_back(boost::asio::buffer(&chunks_, sizeof(chunks_)));
        message_.push_back(boost::asio::const_buffer());

        // FIXME: Not sure if this is the correct way to kick off the
        // serialization call chain.
        *this & p;

        // NOTE: Non-container chunks (e.g. single elements) are not in the size
        // list.
        chunks_ = chunk_sizes_.size();
        message_.at(1) = boost::asio::buffer(chunk_sizes_);

        boost::asio::async_write(*socket_, message_,
            boost::bind(&zero_copy_oarchive::handle_write<Parcel>,
                shared_from_this(),
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred,
                p));
    }

    template <typename Parcel>
    void handle_write(
        boost::system::error_code const& e
      , std::size_t bytes
      , Parcel& p
        )
    {
        if (handler_)
            handler_();

        message_.clear();
        chunk_sizes_.clear();
        chunks_ = 0;
        slow_buffers_.clear();
    }
};

// Note: We must "deserialize" the object BEFORE we read the data, but AFTER
// we have read the sizes. This allows us to do zero-copy, because we know the
// layout of the data structure before we call async_read.
struct zero_copy_iarchive : boost::enable_shared_from_this<zero_copy_oarchive>
{
    typedef std::function<void()> handler_type;

    typedef boost::mpl::true_ is_loading;
    typedef boost::mpl::false_ is_saving;

  private:
    boost::asio::ip::tcp::socket* socket_;

    handler_type handler_;

    bool homogeneity_; ///< Is it safe to do bitwise serialization? E.g. does
                       ///  the target have the endianness as us, etc?

    std::size_t pass_; 

    std::vector<boost::asio::mutable_buffer> message_;
    std::vector<boost::integer::ulittle64_t> chunk_sizes_;
    boost::integer::ulittle64_t chunks_; // chunk_sizes_.size()
    std::size_t current_chunk_;

    std::vector<std::vector<char> > slow_buffers_;
    std::size_t current_slow_buffer_;

  public:
    zero_copy_iarchive(
        boost::asio::ip::tcp::socket& socket 
      , bool homogeneity = true
        )
      : socket_(&socket)
      , handler_()
      , homogeneity_(homogeneity)
      , pass_(0)
      , message_()
      , chunk_sizes_()
      , current_chunk_(0)
      , slow_buffers_()
      , current_slow_buffer_(0)
    {}

    ~zero_copy_iarchive()
    {
        // Gracefully and portably shutdown the socket.
        boost::system::error_code ec;
        socket_->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        socket_->close(ec);
    }

    template <typename T>
    void operator& (T& t) { dispatch(t); }

    template <typename T>
    void operator>> (T& t) { dispatch(t); }

    template <typename T>
    void dispatch(T& t)
    {
        typedef typename is_bitwise_serializable<T>::type predicate_type;

        // Pass 1 builds the structure of message_. It is done right before
        // message_ is read.
        if (1 == pass_)
        {
            if (homogeneity_ && predicate_type::value)
                load_pass1<T>::call(this, t);
            else
                slow_load_pass1(t);
        }

        // Pass 2 decodes message_ after it has been read.
        else if (2 == pass_)
        {
            if (homogeneity_ && predicate_type::value)
                load_pass2<T>::call(this, t);
            else
                slow_load_pass2(t);
        }

        else
            BOOST_ASSERT(false);
    }

    template <typename T>
    struct load_pass1
    {
        static void call(zero_copy_iarchive* self, T& t)
        {
            self->message_.push_back(boost::asio::buffer(&t, sizeof(T)));
        } 
    };

    // This specialization has to be done not just for std::vector, but for
    // other special cases of boost::asio::buffer, such as boost::array,
    // std::array, perhaps std::valarray too.
    template <typename T>
    struct load_pass1<std::vector<T> >
    {
        static void call(zero_copy_iarchive* self, std::vector<T>& t)
        {
            // Use the size list to figure out how large this vector needs to be.
            t.resize(self->chunk_sizes_.at(self->current_chunk_++));

            self->message_.push_back(boost::asio::buffer(t));
        } 
    };

    // This specialization has to be done not just for std::vector, but for
    // other special cases of boost::asio::buffer, such as boost::array,
    // std::array, perhaps std::valarray too. For this pass, the general case
    // (no-op) also works for std::vector, etc.
    template <typename T>
    struct load_pass2
    {
        static void call(zero_copy_iarchive*, T&)
        {
            // No-op.
        }
    };

    template <typename T>
    void slow_load_pass1(T& t)
    {
        // Use the size list to figure out how large this vector has to be.
        slow_buffers_.push_back(std::vector<char>
            (chunk_sizes_.at(current_chunk_++)));
        std::vector<char>& slow_buffer_ = slow_buffers_.back();

        message_.push_back(boost::asio::buffer(slow_buffer_));
    }

    template <typename T>
    void slow_load_pass2(T& t)
    {
        std::vector<char>& slow_buffer_
            = slow_buffers_.at(current_slow_buffer_++);

        typedef container_device<std::vector<char> > io_device_type;
        boost::iostreams::stream<io_device_type> io(slow_buffer_);

        {
            // Deserialize t the slow way.
            portable_binary_iarchive archive(io);
            archive & t;
        }
    }

    // Synchronously read a data structure from the socket.
    template <typename Parcel>
    void read(Parcel& p)
    {
        // The first thing we need is the number of elements in the list of
        // chunk sizes.
        boost::asio::read(*socket_,
            boost::asio::buffer(&chunks_, sizeof(chunks_)));

        // Now we know how large chunk_sizes_ needs to be.
        chunk_sizes_.resize(chunks_);

        // The second thing we need is the list of chunk sizes. 
        boost::asio::read(*socket_, boost::asio::buffer(chunk_sizes_));

        // First pass. Create the message structure. Note that this doesn't
        // actually read in anything.
        pass_ = 1;
        *this & p;

        boost::asio::read(*socket_, message_);

        // Second pass. Do any required deserialization. 
        pass_ = 2;
        *this & p;

        message_.clear();
        chunk_sizes_.clear();
        chunks_ = 0;
        current_chunk_ = 0;
        slow_buffers_.clear();
        current_slow_buffer_ = 0;
    }
 
    // Asynchronously read a data structure from the socket.
    template <typename Parcel>
    void async_read(Parcel& p, handler_type const& h = handler_type())
    {
        handler_ = h;

        // The first thing we need is the number of elements in the list of
        // chunk sizes.
        boost::asio::async_read(*socket_,
            boost::asio::buffer(&chunks_, sizeof(chunks_)),
            boost::bind(&zero_copy_iarchive::handle_read_chunks<Parcel>,
                shared_from_this(),
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred,
                boost::ref(p)));
    }

    template <typename Parcel>
    void handle_read_chunks(
        boost::system::error_code const& e,
        std::size_t bytes,
        Parcel& p
        )
    {
        // Now we know how large chunk_sizes_ needs to be.
        chunk_sizes_.resize(chunks_);

        // The second thing we need is the list of chunk sizes. 
        boost::asio::async_read(*socket_,
            boost::asio::buffer(chunk_sizes_),
            boost::bind(&zero_copy_iarchive::handle_read_chunk_sizes<Parcel>,
                shared_from_this(),
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred,
                boost::ref(p)));
    }

    template <typename Parcel>
    void handle_read_chunk_sizes(
        boost::system::error_code const& e
      , std::size_t bytes
      , Parcel& p
        )
    {
        // First pass. Create the message structure. Note that this doesn't
        // actually read in anything.
        pass_ = 1;
        *this & p;

        boost::asio::async_read(*socket_, message_,
            boost::bind(&zero_copy_iarchive::handle_read_message<Parcel>,
                shared_from_this(),
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred,
                boost::ref(p)));
    }

    template <typename Parcel>
    void handle_read_message(
        boost::system::error_code const& e
      , std::size_t bytes
      , Parcel& p
        )
    {
        // Second pass. Do any required deserialization. 
        pass_ = 2;
        *this & p;

        if (handler_)
            handler_();

        message_.clear();
        chunk_sizes_.clear();
        chunks_ = 0;
        current_chunk_ = 0;
        slow_buffers_.clear();
        current_slow_buffer_ = 0;
    }
};

#endif

