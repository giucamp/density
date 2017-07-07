#pragma once

#define DENSITY_TEST_INSTANCE_PROGRESSIVE	0
#define DENSITY_TEST_INSTANCE_REGISTRY		0

#include <atomic>
#include <type_traits>
#include <limits>
#include <density/runtime_type.h>
#include "density_test_common.h"
#include "exception_tests.h"
#if DENSITY_TEST_INSTANCE_REGISTRY
	#include <mutex>
	#include <unordered_set>
	#include <ostream>
#endif

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable:4324) // structure was padded due to alignment specifier
#endif

namespace density_tests
{
	class InstanceCounted
	{
	public:

		class ScopedLeakCheck
		{
		public:

			ScopedLeakCheck();

			~ScopedLeakCheck();

			ScopedLeakCheck(const ScopedLeakCheck &) = delete;
			ScopedLeakCheck & operator = (const ScopedLeakCheck &) = delete;

		private:
			size_t m_instances;
		};

		InstanceCounted() noexcept
		{
			new_instance();
		}

		#if DENSITY_TEST_INSTANCE_REGISTRY

			virtual void write_leak(std::ostream & i_ostream) const;

		#endif

		InstanceCounted(const InstanceCounted &) noexcept
		{
			new_instance();
		}

		~InstanceCounted()
		{
			#if DENSITY_TEST_INSTANCE_REGISTRY
				unregister_instance(this);
			#endif

			auto const prev_count = s_instance_counter.fetch_sub(1, std::memory_order_relaxed);
			DENSITY_TEST_ASSERT(prev_count > 0);
		}

	private:

		void new_instance();

		#if DENSITY_TEST_INSTANCE_REGISTRY

			struct Registry;

			static Registry & get_registry();

			static void register_instance(InstanceCounted *);
			static void unregister_instance(InstanceCounted *);

		#endif

	private:
		#if DENSITY_TEST_INSTANCE_PROGRESSIVE
			uint64_t m_instance_progr;
			static std::atomic<uint64_t> s_next_instance_progr;
		#endif
		static std::atomic<size_t> s_instance_counter;
	};

	template <size_t SIZE, size_t ALIGNMENT>
		class alignas(ALIGNMENT) TestObject : InstanceCounted
	{
	public:

		// to do: find a better one
		constexpr static unsigned char s_fill_byte = static_cast<unsigned char>(SIZE & std::numeric_limits<unsigned char>::max());

		TestObject()
		{
			exception_checkpoint();
			memset(&m_storage, s_fill_byte, SIZE);
		}

		TestObject(const TestObject & i_source)
			: m_storage(i_source.m_storage)
		{
			exception_checkpoint();
		}

		TestObject(TestObject && i_source)
			: m_storage(i_source.m_storage)
		{
			exception_checkpoint();
		}

		TestObject & operator = (const TestObject & i_source)
		{
			exception_checkpoint();
			m_storage = i_source.m_storage;
			return *this;
		}

		TestObject & operator = (TestObject && i_source)
		{
			exception_checkpoint();
			m_storage = i_source.m_storage;
			return *this;
		}

		~TestObject()
		{
			auto storage = reinterpret_cast<unsigned char *>(&m_storage);
			for (size_t i = 0; i < SIZE; i++)
			{
				DENSITY_TEST_ASSERT(storage[i] == s_fill_byte);
				storage[i] = static_cast<unsigned char>(~s_fill_byte);
			}
		}

	private:
		typename std::aligned_storage<SIZE, ALIGNMENT>::type m_storage;
	};

	template <typename COMMON_TYPE = void>
		class TestRuntimeTime : private InstanceCounted
	{
		using UnderlyingType = typename density::runtime_type<COMMON_TYPE>;

	public:

		using common_type = COMMON_TYPE;

		template <typename TYPE>
			static TestRuntimeTime make() noexcept
		{
			return TestRuntimeTime(UnderlyingType::make<TYPE>());
		}

        TestRuntimeTime()
        {
			exception_checkpoint();
		}

		TestRuntimeTime(const TestRuntimeTime & i_source)
			: m_underlying_type(i_source.m_underlying_type)
        {
			exception_checkpoint();
		}

		TestRuntimeTime & operator = (const TestRuntimeTime & i_source)
        {
			exception_checkpoint();
			m_underlying_type = i_source.m_underlying_type;
			return *this;
		}

		bool empty() const noexcept 
		{ 
			return m_underlying_type.empty(); 
		}

		void clear() noexcept
		{
			m_underlying_type.clear();
		}

		size_t size() const noexcept
        {
            return m_underlying_type.size();
        }

		size_t alignment() const noexcept
        {
            return m_underlying_type.alignment();
        }

		common_type * default_construct(void * i_dest) const
		{
			exception_checkpoint();
			return m_underlying_type.default_construct(i_dest);
		}

		common_type * copy_construct(void * i_dest, const common_type * i_source) const
		{
			exception_checkpoint();
			return m_underlying_type.copy_construct(i_dest, i_source);
		}

		common_type * move_construct(void * i_dest, common_type * i_source) const
		{
			exception_checkpoint();
			return m_underlying_type.move_construct(i_dest, i_source);
		}

		void * destroy(common_type * i_dest) const noexcept
        {
            return m_underlying_type.destroy(i_dest);
        }
		
		const std::type_info & type_info() const noexcept
        {
            return m_underlying_type.type_info();
        }

		bool are_equal(const common_type * i_first, const common_type * i_second) const
        {
            return m_underlying_type.are_equal(i_first, i_second);
        }

		bool operator == (const TestRuntimeTime & i_other) const noexcept
		{
			return m_underlying_type == i_other.m_underlying_type;
		}

		bool operator != (const TestRuntimeTime & i_other) const noexcept
		{
			return m_underlying_type != i_other.m_underlying_type;
		}

		template <typename TYPE>
			bool is() const noexcept
		{
			return m_underlying_type.is<TYPE>();
		}

		size_t hash() const noexcept
		{
			return m_underlying_type.hash();
		}

	private:

		TestRuntimeTime(const UnderlyingType & i_underlying_type)
			: m_underlying_type(i_underlying_type)
		{
		}

	private:
		UnderlyingType m_underlying_type;
	};

} // namespace density_tests

namespace std
{
	/** Partial specialization of std::hash to allow the use of TestRuntimeTime as keys
		of unordered associative containers. */
	template <typename COMMON_TYPE>
		struct hash< density_tests::TestRuntimeTime<COMMON_TYPE> >
	{
		size_t operator()(const density_tests::TestRuntimeTime<COMMON_TYPE> & i_runtime_type) const noexcept
		{
			return i_runtime_type.hash();
		}
	};
}

#ifdef _MSC_VER
	#pragma warning(pop)
#endif
