#ifndef BOOST_PIMPLE_TEST_HPP
#define BOOST_PIMPLE_TEST_HPP

#include <boost/detail/lightweight_test.hpp>
#include <boost/pimpl/pimpl.hpp>
#include <string>

struct singleton_type {};
struct pass_value_type {};
struct Foo {};

struct Test1 : public pimpl<Test1>::pointer_semantics
{
    // Pure interface. The implementation is hidden in unittest_pimpl_implementation.cpp

    typedef std::string string;

    Test1 ();
    Test1 (int);
    Test1 (int, int);
    Test1 (Foo&);
    Test1 (Foo const&);
    Test1 (Foo const&, Foo const&);
    Test1 (Foo&, Foo const&);
    Test1 (Foo const&, Foo&);
    Test1 (Foo&, Foo&);
    Test1 (Foo*);
    Test1 (Foo const*);
    Test1 (singleton_type const&);
    Test1 (pass_value_type const&);

    string const& trace () const;
    int              id () const;

    private:

    friend class boost::serialization::access;
    template<class Archive> void serialize(Archive&, unsigned int);
};

struct Test2 : public pimpl<Test2>::value_semantics
{
    // Pure interface. The implementation is hidden in unittest_pimpl_implementation.cpp

    typedef Test2    this_type;
    typedef std::string string;

    Test2 ();
    Test2 (int);

    // Value-semantics Pimpl must explicitly define op==()
    // if it wants to be comparable. The same as normal classes do.
    bool operator==(Test2 const& o) const;
//  bool operator!=(Test2 const& o) const { return !this_type::operator==(o); } // Not needed

    string const& trace () const;
    int              id () const;
};

struct Base : public pimpl<Base>::pointer_semantics
{
    Base (int);

    std::string call_virtual(); // Non-virtual.
    std::string const& trace() const;
};

struct Derived1 : public Base
{
    Derived1 (int, int);
};

struct Derived2 : public Derived1
{
    Derived2 (int, int, int);
};


#endif // BOOST_PIMPLE_TEST_HPP
