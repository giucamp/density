#pragma once
#include "../test_framework/density_test_common.h"
#include "../test_framework/test_objects.h"
#include <string>
#include <vector>
#include <cstdint>
#include <complex>

namespace density_tests
{
	struct NonPolymorphicBase
	{
		int m_int = 35;

		InstanceCounted m_inst_counted_1;
		
		void check()
		{
			DENSITY_TEST_ASSERT(m_int == 35);
		}

		~NonPolymorphicBase()
		{
			check();
		}
	};

	struct SingleDerivedNonPoly : public NonPolymorphicBase
	{
		std::string m_str1 = "Hello ";
		std::string m_str2 = "world!!";

		InstanceCounted m_inst_counted_2;

		void check()
		{
			NonPolymorphicBase::check();
			DENSITY_TEST_ASSERT(m_str1 + m_str2 == "Hello world!!");
		}

		~SingleDerivedNonPoly()
		{
			void check();
		}
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
			DENSITY_TEST_ASSERT(m_double = 22.);
		}
		
		virtual ~PolymorphicBase()
		{
			check();
		}
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

		~SingleDerived()
		{
			check();
		}
	};


	struct Derived1 : public virtual PolymorphicBase
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

		~Derived1()
		{
			check();
		}
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

		~Derived2()
		{
			check();
		}
	};

	struct MultipleDerived : public Derived1, public Derived2
	{
		constexpr static int s_class_id = 30;

		virtual int class_id() const { return s_class_id; }

		using Complex = std::complex<double>;

		Complex m_complex{ 2., -4. };

		InstanceCounted m_inst_counted_7;

		void check()
		{
			Derived1::check();
			Derived2::check();
			DENSITY_TEST_ASSERT((m_complex == Complex{ 2., -4. }));
		}

		~MultipleDerived()
		{
			check();
		}
	};
	
	
} // namespace density_tests
