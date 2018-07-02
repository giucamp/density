
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "../test_framework/density_test_common.h"
#include "../test_framework/test_objects.h"
#include <complex>
#include <cstdint>
#include <string>
#include <vector>

namespace density_tests
{
    struct NonPolymorphicBase
    {
        int m_int = 35;

        InstanceCounted m_inst_counted_1;

        void check() { DENSITY_TEST_ASSERT(m_int == 35); }

        ~NonPolymorphicBase() { check(); }
    };

    struct alignas(8) SingleDerivedNonPoly : public NonPolymorphicBase
    {
        std::string m_str1 = "Hello ";
        std::string m_str2 = "world!!";

        InstanceCounted m_inst_counted_2;

        void check()
        {
            NonPolymorphicBase::check();
            DENSITY_TEST_ASSERT(m_str1 + m_str2 == "Hello world!!");
        }

        ~SingleDerivedNonPoly() { void check(); }
    };

    struct PolymorphicBase : public NonPolymorphicBase
    {
        constexpr static int s_class_id = 10;

        virtual int class_id() const { return s_class_id; }

        double m_double = 22.;

        InstanceCounted m_inst_counted_3;

        void check()
        {
            NonPolymorphicBase::check();
            DENSITY_TEST_ASSERT(m_double == 22.);
        }

        virtual ~PolymorphicBase() { check(); }
    };

    struct SingleDerived : public PolymorphicBase
    {
        constexpr static int s_class_id = 15;

        virtual int class_id() const { return s_class_id; }

        std::string m_str = "Hi!!";

        InstanceCounted m_inst_counted_4;

        void check()
        {
            PolymorphicBase::check();
            DENSITY_TEST_ASSERT(m_str == "Hi!!");
        }

        ~SingleDerived() { check(); }
    };


    struct alignas(16) Derived1 : public virtual PolymorphicBase
    {
        constexpr static int s_class_id = 20;

        virtual int class_id() const { return s_class_id; }

        int64_t m_int64 = 999;

        InstanceCounted m_inst_counted_5;

        void check()
        {
            PolymorphicBase::check();
            DENSITY_TEST_ASSERT(m_int64 == 999);
        }

        ~Derived1() { check(); }
    };

    struct Derived2 : public virtual PolymorphicBase
    {
        constexpr static int s_class_id = 25;

        virtual int class_id() const { return s_class_id; }

        int8_t m_int8 = 22;

        InstanceCounted m_inst_counted_6;

        void check()
        {
            PolymorphicBase::check();
            DENSITY_TEST_ASSERT(m_int8 == 22);
        }

        ~Derived2() { check(); }
    };

    struct MultipleDerived : public Derived1, public Derived2
    {
        constexpr static int s_class_id = 30;

        virtual int class_id() const { return s_class_id; }

        using Complex = std::complex<double>;

        Complex m_complex{2., -4.};

        InstanceCounted m_inst_counted_7;

        void check()
        {
            Derived1::check();
            Derived2::check();
            DENSITY_TEST_ASSERT((m_complex == Complex{2., -4.}));
        }

        ~MultipleDerived() { check(); }
    };

    template <typename EXPECTED_TYPE, typename CONSUME_OPERATION>
    void polymorphic_consume(CONSUME_OPERATION i_consume)
    {
        DENSITY_TEST_ASSERT(i_consume.complete_type().template is<EXPECTED_TYPE>());

        //i_consume.element_ptr()->check();
        i_consume.template element<EXPECTED_TYPE>().check();

        //DENSITY_TEST_ASSERT(i_consume.element_ptr()->class_id() == EXPECTED_TYPE::s_class_id);
        DENSITY_TEST_ASSERT(
          i_consume.template element<EXPECTED_TYPE>().class_id() == EXPECTED_TYPE::s_class_id);

        auto const unaligned_element_ptr = i_consume.unaligned_element_ptr();
        auto const untyped_element_ptr   = density::address_upper_align(
          unaligned_element_ptr, i_consume.complete_type().alignment());
        auto const element_ptr = static_cast<EXPECTED_TYPE *>(untyped_element_ptr);
        element_ptr->check();
        DENSITY_TEST_ASSERT(element_ptr->class_id() == EXPECTED_TYPE::s_class_id);

        i_consume.commit();
    }

} // namespace density_tests
