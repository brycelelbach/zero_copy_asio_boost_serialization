#ifndef TEST_PORTABLE_BINARY_IARCHIVE_HPP
#define TEST_PORTABLE_BINARY_IARCHIVE_HPP

#include <boost/version.hpp>

// MS compatible compilers support #pragma once
#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning( disable : 4244 )
#endif

/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
// portable_binary_iarchive.hpp

// (C) Copyright 2002-7 Robert Ramey - http://www.rrsd.com .
// Copyright (c) 2007-2012 Hartmut Kaiser
//
// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org for updates, documentation, and revision history.

#include <istream>
#include <boost/utility/enable_if.hpp>
#include <boost/type_traits/is_integral.hpp>
#include <boost/serialization/array.hpp>
#include <boost/serialization/string.hpp>
#include <boost/archive/archive_exception.hpp>

#if !defined(BOOST_WINDOWS)
  #pragma GCC visibility push(default)
#endif

#include <boost/archive/basic_binary_iprimitive.hpp>

#if !defined(BOOST_WINDOWS)
  #pragma GCC visibility pop
#endif

#include <boost/archive/detail/common_iarchive.hpp>
#include <boost/archive/shared_ptr_helper.hpp>
#include <boost/archive/detail/register_archive.hpp>
#if BOOST_VERSION >= 104400
#include <boost/serialization/item_version_type.hpp>
#endif

#include "portable_binary_archive.hpp"

/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
// exception to be thrown if integer read from archive doesn't fit
// variable being loaded
class portable_binary_iarchive_exception :
    public virtual boost::archive::archive_exception
{
public:
    enum exception_code {
        incompatible_integer_size
    };
    portable_binary_iarchive_exception(exception_code c = incompatible_integer_size )
      : boost::archive::archive_exception(static_cast<boost::archive::archive_exception::exception_code>(c))
    {}
    virtual const char *what() const throw()
    {
        const char *msg = "programmer error";
        switch(code){
        case incompatible_integer_size:
            msg = "integer cannot be represented";
        default:
            boost::archive::archive_exception::what();
        }
        return msg;
    }
};

/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
// "Portable" input binary archive.  It addresses integer size and endianness so
// that binary archives can be passed across systems. Note:floating point types
// are passed through as is.

class portable_binary_iarchive :
    public boost::archive::basic_binary_iprimitive<
        portable_binary_iarchive,
        std::istream::char_type,
        std::istream::traits_type
    >,
    public boost::archive::detail::common_iarchive<
        portable_binary_iarchive
    >,
    public boost::archive::detail::shared_ptr_helper
{
    typedef boost::archive::basic_binary_iprimitive<
        portable_binary_iarchive,
        std::istream::char_type,
        std::istream::traits_type
    > primitive_base_t;
    typedef boost::archive::detail::common_iarchive<
        portable_binary_iarchive
    > archive_base_t;
#ifndef BOOST_NO_MEMBER_TEMPLATE_FRIENDS
public:
#else
    friend archive_base_t;
    friend primitive_base_t; // since with override load below
    friend class boost::archive::detail::interface_iarchive<
        portable_binary_iarchive>;
    friend class boost::archive::load_access;
protected:
#endif
    unsigned int m_flags;
    void
    load_impl(boost::intmax_t& l, char maxsize);

    // default fall through for any types not specified here
#if defined(__GNUG__) && !defined(__INTEL_COMPILER)
#if defined(HPX_GCC_DIAGNOSTIC_PRAGMA_CONTEXTS)
#pragma GCC diagnostic push
#endif
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
    template <typename T>
    void load(T& t, typename boost::enable_if<boost::is_integral<T> >::type* = 0)
    {
        boost::intmax_t l = 0;
        load_impl(l, sizeof(T));
        // use cast to avoid compile time warning
        t = static_cast<T>(l);
    }
#if defined(__GNUG__) && !defined(__INTEL_COMPILER)
#if defined(HPX_GCC_DIAGNOSTIC_PRAGMA_CONTEXTS)
#pragma GCC diagnostic pop
#endif
#endif

    template <typename T>
    void load(T& t, typename boost::disable_if<boost::is_integral<T> >::type* = 0) 
    {
        this->primitive_base_t::load(t);
    }

    void load(std::string& t) {
        this->primitive_base_t::load(t);
    }
#if BOOST_VERSION >= 104400
    void load(boost::archive::class_id_reference_type& t) {
        boost::intmax_t l = 0;
        load_impl(l, sizeof(boost::int16_t));
        t = boost::archive::class_id_reference_type(
            boost::archive::class_id_type(std::size_t(l)));
    }
    void load(boost::archive::class_id_optional_type& t) {
        boost::intmax_t l = 0;
        load_impl(l, sizeof(boost::int16_t));
        t = boost::archive::class_id_optional_type(
            boost::archive::class_id_type(std::size_t(l)));
    }
    void load(boost::archive::class_id_type& t) {
        boost::intmax_t l = 0;
        load_impl(l, sizeof(boost::int16_t));
        t = boost::archive::class_id_type(std::size_t(l));
    }
    void load(boost::archive::object_id_type& t) {
        boost::intmax_t l = 0;
        load_impl(l, sizeof(boost::uint32_t));
        t = boost::archive::object_id_type(static_cast<unsigned int>(l));
    }
    void load(boost::archive::object_reference_type& t) {
        boost::intmax_t l = 0;
        load_impl(l, sizeof(boost::uint32_t));
        t = boost::archive::object_reference_type(
            boost::archive::object_id_type(std::size_t(l)));
    }
    void load(boost::archive::tracking_type& t) {
        bool l = false;
        this->primitive_base_t::load(l);
        t = boost::archive::tracking_type(l);
    }
    void load(boost::archive::version_type& t) {
        boost::intmax_t l = 0;
        load_impl(l, sizeof(boost::uint32_t));
        t = boost::archive::version_type(static_cast<unsigned int>(l));
    }
    void load(boost::archive::library_version_type& t) {
        boost::intmax_t l = 0;
        load_impl(l, sizeof(boost::uint16_t));
        t = boost::archive::library_version_type(static_cast<unsigned int>(l));
    }
    void load(boost::serialization::item_version_type& t) {
        boost::intmax_t l = 0;
        load_impl(l, sizeof(boost::intmax_t));
        t = boost::serialization::item_version_type(static_cast<unsigned int>(l));
    }
#endif
#ifndef BOOST_NO_STD_WSTRING
    void load(std::wstring& t) {
        this->primitive_base_t::load(t);
    }
#endif
    void load(float& t) {
        this->primitive_base_t::load(t);
    }
    void load(double& t) {
        this->primitive_base_t::load(t);
    }
    void load(char& t) {
        this->primitive_base_t::load(t);
    }
    void load(unsigned char& t) {
        this->primitive_base_t::load(t);
    }
    void load(signed char& t) {
        this->primitive_base_t::load(t);
    }

    // intermediate level to support override of operators
    // for templates in the absence of partial function
    // template ordering
    typedef boost::archive::detail::common_iarchive<portable_binary_iarchive>
        detail_common_iarchive;

    template <typename T>
    void load_override(T & t, BOOST_PFTO int) {
        this->detail_common_iarchive::load_override(t, 0);
    }

    void
    load_override(boost::archive::class_name_type & t, int);

    // binary files don't include the optional information
    void load_override(boost::archive::class_id_optional_type&, int) {}

    void
    init(unsigned int flags);

public:
    portable_binary_iarchive(std::istream & is, unsigned flags = 0)
      : primitive_base_t(
            *is.rdbuf(),
            0 != (flags & boost::archive::no_codecvt)
        ),
        archive_base_t(flags),
        m_flags(0)
    {
        init(flags);
    }

    portable_binary_iarchive(
            std::basic_streambuf<
                std::istream::char_type,
                std::istream::traits_type
            > & bsb,
            unsigned int flags)
      : primitive_base_t(
            bsb,
            0 != (flags & boost::archive::no_codecvt)
        ),
        archive_base_t(flags),
        m_flags(0)
    {
        init(flags);
    }

    // the optimized load_array dispatches to load_binary 
    template <typename T>
    void load_array(boost::serialization::array<T>& a, unsigned int)
    {
        // If we need to potentially flip bytes we serialize each element 
        // separately.
#ifdef BOOST_BIG_ENDIAN
        if (m_flags & endian_little) {
            for (std::size_t i = 0; i != a.count(); ++i)
                load(a.address()[i]);
        }
#else
        if (m_flags & endian_big) {
            for (std::size_t i = 0; i != a.count(); ++i)
                load(a.address()[i]);
        }
#endif
        else {
            this->primitive_base_t::load_binary(a.address(), a.count()*sizeof(T));
        }
    }

    void load_array(boost::serialization::array<float>& a, unsigned int)
    {
        this->primitive_base_t::load_binary(a.address(), a.count()*sizeof(float));
    }
    void load_array(boost::serialization::array<double>& a, unsigned int)
    {
        this->primitive_base_t::load_binary(a.address(), a.count()*sizeof(double));
    }
    void load_array(boost::serialization::array<char>& a, unsigned int)
    {
        this->primitive_base_t::load_binary(a.address(), a.count()*sizeof(char));
    }
    void load_array(boost::serialization::array<unsigned char>& a, unsigned int)
    {
        this->primitive_base_t::load_binary(a.address(), a.count()*sizeof(unsigned char));
    }
    void load_array(boost::serialization::array<signed char>& a, signed int)
    {
        this->primitive_base_t::load_binary(a.address(), a.count()*sizeof(signed char));
    }
};

// required by export in boost version > 1.34
BOOST_SERIALIZATION_REGISTER_ARCHIVE(portable_binary_iarchive)
BOOST_SERIALIZATION_USE_ARRAY_OPTIMIZATION(portable_binary_iarchive)

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif

