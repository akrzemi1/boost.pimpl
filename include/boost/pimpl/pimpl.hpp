// Copyright (c) 2006-2014 Vladimir Batov.
// Copyright (c) 2001 Peter Dimov and Multi Media Ltd.
// Use, modification and distribution are subject to the Boost Software License,
// Version 1.0. See http://www.boost.org/LICENSE_1_0.txt.

#ifndef BOOST_PIMPL_HPP
#define BOOST_PIMPL_HPP

#include <boost/config.hpp>
#include <boost/assert.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/static_assert.hpp>
#include <boost/utility.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/type_traits.hpp>

#if defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES) || defined(BOOST_NO_CXX11_RVALUE_REFERENCES)
#define BOOST_PIMPL_CXX03
#include <boost/pimpl/detail/cxx03_forwarding_constructors.hpp>
#include <boost/pimpl/detail/safebool.hpp>
#endif

// a) The BOOST_PIMPL_DEPLOY_IF_NOT_PIMPL_DERIVED macro makes sure that the one-arg
//    constructor is not called when the copy constructor is in order.
// b) The macro uses boost::is_base_of<pimpl_base_type, A> instead of is_pimpl<>::value
//    as we do not want the respective constructor disabled for any pimpl argument.
//    We only want it disabled for classes directly derived from this very
//    pimpl_base so that we deploy the pimpl_base copy-constructor instead.
// c) The macro uses the 'internal_type' type to uniquely distinguish the
//    respective constructor from ANY of 2-args constructors.
#define BOOST_PIMPL_DEPLOY_IF_NOT_PIMPL_DERIVED(A) \
    typename boost::disable_if<                    \
        boost::is_base_of<pimpl_base_type,         \
            typename boost::remove_reference<A>::type>, internal_type*>::type =0

/// @class pimpl
/// @brief Generalization of the Pimpl idiom
//
/// @typedef pimpl::value_semantics
/// @brief Convenience typedef to deploy pimpl with value semantics
/// @details The deployment technique is as follows:
/// @code
///     struct Foo : public pimpl<Foo>::value_semantics {...}
///     struct Bar : public pimpl<Bar>::pointer_semantics {...}
/// @endcode
//
/// @typedef pimpl::pointer_semantics
/// @brief Convenience typedef to deploy pimpl with pointer semantics
/// @details The deployment technique is as follows:
/// @code
///     struct Foo : public pimpl<Foo>::value_semantics {...}
///     struct Bar : public pimpl<Bar>::pointer_semantics {...}
/// @endcode
//template<class Interface, class Enable =void>
template<class Interface>
struct pimpl
{
    struct implementation;
    template<class> class value_ptr;
    template<template<class> class> class pimpl_base;
    typedef pimpl_base<value_ptr> value_semantics;
    typedef pimpl_base<boost::shared_ptr> pointer_semantics;

    static Interface null();
};

/// @brief Implementation of the type-traits functionality for the pimpl
/// @details is_pimpl<Foo>::value is 'true' if Foo directly or indirectly
/// inherits from pimpl<>. Otherwise, is_pimpl<Foo>::value is 'false'.
template<class T>
class is_pimpl
{
    typedef boost::type_traits::yes_type               yes_type;
    typedef boost::type_traits::no_type                 no_type;
    typedef typename boost::remove_reference<T>::type* ptr_type;

    template<class Y>
    static yes_type tester (Y const*, typename Y::pimpl_base_type const* =0);
    static no_type  tester (...);

    public:

    BOOST_STATIC_CONSTANT(bool, value = (1 == sizeof(tester(ptr_type(0)))));
};

/// @brief Smart-pointer with the value-semantics behavior
/// @details It complements boost::shared_ptr which takes care of the pointer
/// semantics behavior. The incomplete-type management technique is by Peter
/// Dimov (see http://tech.groups.yahoo.com/group/boost/files/impl_ptr).
template<class Interface>
template<class Impl/*ementation*/>
class pimpl<Interface>::value_ptr
{
    typedef value_ptr this_type;

    public:

   ~value_ptr () { traits_->destroy(impl_); }
    value_ptr () : traits_(null()), impl_(0) {}
    value_ptr (Impl* p) : traits_(deep_copy()), impl_(p) {}
    value_ptr (this_type const& that) : traits_(that.traits_), impl_(traits_->copy(that.impl_)) {}

    this_type& operator= (this_type const& that) { traits_ = that.traits_; traits_->assign(impl_, that.impl_); return *this; }
    bool       operator< (this_type const& that) const { return this->impl_ < that.impl_; }

    void      reset (Impl* p) { this_type(p).swap(*this); }
    void       swap (this_type& that) { std::swap(impl_, that.impl_), std::swap(traits_, that.traits_); }
    Impl*       get () { return impl_; }
    Impl const* get () const { return impl_; }
    long  use_count () const { return 1; }

    private:

    struct traits
    {
        virtual ~traits() {}

        virtual void destroy (Impl*&) const =0;
        virtual Impl*   copy (Impl const*) const =0;
        virtual void  assign (Impl*&, Impl const*) const =0;
    };
    struct null : public traits
    {
        virtual void destroy (Impl*&) const {}
        virtual Impl*   copy (Impl const*) const { return 0; }
        virtual void  assign (Impl*&, Impl const*) const {};

        operator traits const*() { static null impl; return &impl; }
    };
    struct deep_copy : public traits
    {
        virtual void destroy (Impl*& p) const { boost::checked_delete(p); p = 0; }
        virtual Impl*   copy (Impl const* p) const { return p ? new Impl(*p) : 0; }
        virtual void  assign (Impl*& a, Impl const* b) const
        {
            /**/ if ( a ==  b);
            else if ( a &&  b) *a = *b;
            else if (!a &&  b) a = copy(b);
            else if ( a && !b) destroy(a);
        }
        operator traits const*() { static deep_copy impl; return &impl; }
    };

    traits const* traits_;
    Impl*           impl_;
};

/// @details The base class behind pimpl::pointer_semantics and pimpl::value_semantics
/// convenience typedefs. All pimpl-based classes derive from the class by
/// @code
///      struct Foo : public pimpl<Foo>::pointer_semantics { ... };
///      struct Moo : public pimpl<Moo>::value_semantics { ... };
/// @endcode
template<class T>
template<template<class> class Manager>
class pimpl<T>::pimpl_base
{
    template<typename> friend class is_pimpl; // is_pimpl accesses pimpl_base_type.
    
    typedef typename pimpl<T>::template pimpl_base<Manager> pimpl_base_type;
    typedef typename pimpl<T>::implementation           implementation_type;
    typedef Manager<implementation_type>                       managed_type;
    struct                                                    internal_type {};

    public:

    typedef boost::detail::safebool<T>   safebool;
    typedef typename safebool::type safebool_type;
    typedef implementation_type    implementation;
    typedef pimpl_base_type             base_type;

    bool         operator! () const { return !pimpl_base_type::impl_.get(); }
#ifdef BOOST_PIMPL_CXX03
    operator safebool_type () const { return safebool(pimpl_base_type::impl_.get()); }
#else
    explicit operator bool () const { return pimpl_base_type::impl_.get(); }
#endif

    static T null() { return pimpl<T>::null(); }

    // pimpl_base::op==() simply transfers the comparison down to its implementation policy --
    // boost::shared_ptr or pimpl::value_ptr. Consequently, pointer-semantics (shared_ptr-based) pimpls are comparable
    // as there is shared_ptr::op==(). However, a value-semantics (value_ptr-based) pimpl is not comparable by default
    // (the standard value-semantics behavior) as there is no pimpl::value_ptr::op==().
    // If a value-semantics class T needs to be comparable, then it has to *explicitly* provide T::op==(T const&)
    // as part of its public interface. Trying to call this pimpl_base::op==() for value_ptr-based pimpl will fail
    // to compile (no value_ptr::op==()) and will indicate that the user forgot to declare T::operator==(T const&).

    bool operator==(pimpl_base_type const& that) const { return impl_ == that.impl_; }
    bool operator!=(T const& that) const { return !((T*) this)->T::operator==(that); }

    // For pimpl to be used in std associative containers.
    bool operator<(pimpl_base_type const& that) const { return pimpl_base_type::impl_ < that.pimpl_base_type::impl_; }

    void swap (pimpl_base_type& that) { pimpl_base_type::impl_.swap(that.pimpl_base_type::impl_); }
    void swap (managed_type& that) { pimpl_base_type::impl_.swap(that); }

    /// @name Explicit Management of Interface-Implementation Associations
    /// @details Usually internal interface-implementation associations are managed automatically by the base class:
    /// @code
    ///      Foo::Foo(params) : base_type(params) {...}
    /// @endcode
    /// Sometimes, however, it is not what we want. Like for lazy-instantiation optimization or pimpl<> deployment as
    /// the base of run-time polymorphic hierarchies. Then use reset() to explicitly set/reset the internal
    /// interface-implementation association to point to the supplied implementation. The behavior is similar to
    /// std::auto_ptr::reset() or boost::shared_ptr::reset(). For example, the following technique could be
    /// used for the Non-Virtual Interface idiom:
    /// @code
    ///     struct impl1 : public pimpl<Foo>::implementation { ... };
    ///     struct impl2 : public pimpl<Foo>::implementation { ... };
    ///
    ///     Foo::Foo(parameters) : base_type(null())
    ///     {
    ///         reset(condition ? new impl1 : new impl2);
    ///     }
    /// @endcode
    //@{
    void reset(implementation_type* p) { pimpl_base_type::impl_.reset(p); }
    template<class Y, class D> void reset(Y* p, D d) { pimpl_base_type::impl_.reset(p, d); }
    //@}

    /// @name Access To Implementation
    /// @details Functions allow access to the underlying implementation. These methods are only meaningful to the code
    /// for which pimpl<>::implementation has been made visible. For example, the derived classes which extend the base
    /// implementation or the code inside *implementation* files. Methods are to be used as follows:
    /// @code
    ///      implementation& impl = **this;
    ///      implementation const& impl = **this; // inside 'const' methods
    ///
    ///      impl.data = ...;
    /// @endcode
    /// Pimpl does *not* behave like a pointer, where, say, 'const shared_ptr' still allows modifications of
    /// the underlying data. Instead, Pimpl behaves like a proxy. Therefore, 'const' instances return 'const'
    /// pointers and references.
    //@{
    implementation_type const* operator->() const { BOOST_ASSERT(impl_.get()); return  impl_.get(); }
    implementation_type const& operator *() const { BOOST_ASSERT(impl_.get()); return *impl_.get(); }
    implementation_type*       operator->()       { BOOST_ASSERT(impl_.get()); return  impl_.get(); }
    implementation_type&       operator *()       { BOOST_ASSERT(impl_.get()); return *impl_.get(); }
    //@}

    long use_count() const { return impl_.use_count(); }

    protected:

    // The default auto-generated copy constructor and the default assignment
    // operator are just fine (do member-wise copy and assignment respectively).

    // Forwarding Constructors. Forward arguments to the corresponding constructors of the internal 'implementation' class.
    // That is done to encapsulate the construction of the 'implementation' instance inside this class and, consequently,
    // to *fully* automate memory management (rather than just deletion).

    pimpl_base() : impl_(new implementation_type()) {}

    // The BOOST_PIMPL_DEPLOY_IF_NOT_PIMPL_DERIVED macro makes sure that these one-arg constructors
    // are not called when the copy constructor is in order.
    template<class A> explicit pimpl_base(A&       a, BOOST_PIMPL_DEPLOY_IF_NOT_PIMPL_DERIVED(A)) : impl_(new implementation(a)) {}                                           
    template<class A> explicit pimpl_base(A const& a, BOOST_PIMPL_DEPLOY_IF_NOT_PIMPL_DERIVED(A)) : impl_(new implementation(a)) {}                                           
                                                                            
#ifdef BOOST_PIMPL_CXX03

    BOOST_PIMPL_CXX03_FORWARDING_CONSTRUCTORS;

#else

    pimpl_base(pimpl_base&& other)
        : impl_(std::move(other.impl_))
    {
    }

    template<class... Args>
    pimpl_base(Args&&... args)
        : impl_(new implementation_type(std::forward<Args>(args)...))
    {
    }
#endif

    private:

    // Creates an invalid (with no implementation) instance. Used internally by pimpl::null().
    pimpl_base (internal_type) {}

    template<class Y> friend struct pimpl;
    friend class boost::serialization::access;
    template<class Archive> void serialize(Archive& a, unsigned int)
    {
        a & BOOST_SERIALIZATION_NVP(pimpl_base_type::impl_);
    }

    managed_type impl_;
};

template<class T>
inline
T
pimpl<T>::null()
{
    BOOST_STATIC_ASSERT(is_pimpl<T>::value);
    BOOST_STATIC_ASSERT(sizeof(T) == sizeof(typename T::pimpl_base_type));

    typename T::internal_type null_arg;
    typename T::pimpl_base_type null_pimpl(null_arg);

    return *(T*) &null_pimpl;
}

// To deploy boost::serialization for a Pimpl-based class the following steps
// are needed:
// C0. Include the relevant archives. Something like
//          #include <boost/archive/binary_iarchive.hpp>
//          #include <boost/archive/binary_oarchive.hpp>
//          #include <boost/archive/text_iarchive.hpp>
//          #include <boost/archive/text_oarchive.hpp>
//          #include <boost/archive/xml_iarchive.hpp>
//          #include <boost/archive/xml_oarchive.hpp>
// C1. Add the following serialization DECLARATION to the interface class
//     THE_CLASS:
//          private:
//          friend class boost::serialization::access;
//          template<class Archive> void serialize(Archive&, unsigned int);
// C2. Add the following serialization DEFINITION to the
//     pimpl<THE_CLASS>::implementation class
//          template<class Archive>
//          void
//          serialize(Archive& a, unsigned int file_version)
//          { THE SERIALIZATION OF YOUR PIMPL IMPLEMENTATION GOES HERE }
// C3. Add one of the BOOST_SERIALIZATION_PIMPL...(THE_CLASS) macros to the same
//     file where pimpl<THE_CLASS>::implementation is declared and implemented.
// C4. For more info see http://www.boost.org/libs/serialization/doc/index.html
//     and then go to "Case Studies/PIMPL"
// C5. BOOST_SERIALIZATION_PIMPL_REPLACE has the standard boost::serialization
//     behavior, i.e. it RESETS the supplied pimpl as the macro delegates
//     serialization to the underlying boost::shared_ptr which in turn REPLACES
//     the underlying implementation data with newly created and serialized
//     data. See boost/shared_ptr.hpp where load() looks like
//
//     void load(Archive& ar, boost::shared_ptr<T>& t, const unsigned int)
//     {
//         T* r;
//         ar >> boost::serialization::make_nvp("px", r);
//         get_helper<Archive, detail::shared_ptr_helper >(ar).reset(t,r);
//     }
//     All that is done for a reason. That allows boost::serialization to track
//     shared pointers and, therefore, to avoid duplicates.
// C6. Given the supplied pimpl content is replaced, there is no reason to
//     create that temporary pimpl (that by default requires the default
//     constructor. For that reason we initialize that replaceable pimpl with
//     null().
// C7. BOOST_SERIALIZATION_PIMPL_INPLACE works differently as it does not
//     serialize the underlying boost::shared_ptr but instead populates/loads
//     into the user-provided implementation data. That certainly lacks
//     boost::serialization standard object-tracking properties of
//     BOOST_SERIALIZATION_PIMPL_REPLACE as the serialization step of
//     boost::shared_ptr is by-passed. Consequently, it is not suitable when a
//     pimpl is restored from scratch or many pimpls pointing to the same data
//     are stored/restored. However, it is essential when an application
//     controls the creation of a pimpl. Say, a pimpl object is a singleton and
//     by the time we try restoring from the persistent state there are many
//     pimpls holding references to that same instance. In such setting we need
//     to replace the content of the already existing implementation rather than
//     replace the implementation instance as a whole.
//     Another application might be when an instance is initially constructed
//     manually (not restored from a persistent state) and, say, only partially
//     initialized from, say, (static) configuration data. Then more (dynamic)
//     data can be loaded via serialization into that EXISTING instance.
#define BOOST_SERIALIZATION_PIMPL_EXPLICIT_INSTANTIATIONS(THE_CLASS)                    \
    /* Explicit instantiation of the serialization code. Add more when needed. */       \
    template void THE_CLASS::serialize(boost::archive::binary_iarchive&, unsigned int); \
    template void THE_CLASS::serialize(boost::archive::binary_oarchive&, unsigned int); \
    template void THE_CLASS::serialize(boost::archive::  text_iarchive&, unsigned int); \
    template void THE_CLASS::serialize(boost::archive::  text_oarchive&, unsigned int); \
    template void THE_CLASS::serialize(boost::archive::   xml_iarchive&, unsigned int); \
    template void THE_CLASS::serialize(boost::archive::   xml_oarchive&, unsigned int); \

#define BOOST_SERIALIZATION_PIMPL_REPLACE(THE_CLASS) /* See C5 */                       \
                                                                                        \
    namespace boost                                                                     \
    {                                                                                   \
        namespace serialization                                                         \
        {                                                                               \
            template<class Archive>                                                     \
            inline                                                                      \
            typename boost::enable_if<is_pimpl<THE_CLASS>, void>::type                  \
            load_construct_data(                                                        \
                Archive& ar,                                                            \
                THE_CLASS* t,                                                           \
                const unsigned int file_version)                                        \
            {                                                                           \
                ::new(t) THE_CLASS(pimpl<THE_CLASS>::null()); /* See C6 */              \
            }                                                                           \
        }                                                                               \
    }                                                                                   \
    /* Serialization definition for THE_CLASS */                                        \
    template<class Archive>                                                             \
    void                                                                                \
    THE_CLASS::serialize(Archive& a, unsigned int)                                      \
    {                                                                                   \
        a & BOOST_SERIALIZATION_BASE_OBJECT_NVP(pimpl_base_type);                       \
    }                                                                                   \
    BOOST_SERIALIZATION_PIMPL_EXPLICIT_INSTANTIATIONS(THE_CLASS)                        \

#define BOOST_SERIALIZATION_PIMPL_INPLACE(THE_CLASS) /* See C7 */                       \
                                                                                        \
    /* Serialization definition for THE_CLASS */                                        \
    template<class Archive>                                                             \
    void                                                                                \
    THE_CLASS::serialize(Archive& a, unsigned int)                                      \
    {                                                                                   \
        a & BOOST_SERIALIZATION_NVP(**this);                                            \
    }                                                                                   \
    BOOST_SERIALIZATION_PIMPL_EXPLICIT_INSTANTIATIONS(THE_CLASS)                        \

#endif // BOOST_PIMPL_HPP
