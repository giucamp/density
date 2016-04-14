
//          Copyright Giuseppe Campana (giu.campana@gmail.com) 2016 - 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <unordered_map>
#include <functional>
#include <memory>

namespace reflective
{
	namespace details
	{
		class TestAllocatorBase
		{
		public:

			static void push_level();
			static void pop_level();

			static void * alloc(size_t i_size);
			static void free(void * i_block);

		private:
			struct AllocationEntry
			{
				size_t m_size;
			};
			struct Levels
			{
				std::unordered_map<void*, AllocationEntry> m_allocations;
			};
			struct ThreadData
			{
				std::vector<Levels> m_levels;
			};
			static ThreadData & GetThreadData();

		private:
			#if defined(_MSC_VER) && _MSC_VER < 1900 // Visual Studio 2013 and below
				static _declspec(thread) ThreadData * st_thread_data;
			#else
				static thread_local ThreadData st_thread_data;
			#endif
		};

	} // namespace details

	template <class TYPE>
		class TestAllocator : private details::TestAllocatorBase
	{
	public:
		typedef TYPE value_type;

		TestAllocator() {}

		template <class OTHER_TYPE> TestAllocator(const TestAllocator<OTHER_TYPE>& /*i_other*/) { }

		TYPE * allocate(std::size_t i_count)
		{
			return static_cast<TYPE *>(details::TestAllocatorBase::alloc(i_count * sizeof(TYPE)) );
		}

		void deallocate(TYPE * i_block, std::size_t /*i_count*/)
		{
			details::TestAllocatorBase::free(i_block);
		}

		template <typename OTHER_TYPE>
			bool operator == (const TestAllocator<OTHER_TYPE> &) const
		{
			return true;
		}

		template <typename OTHER_TYPE>
			bool operator != (const TestAllocator<OTHER_TYPE> &) const
		{
			return false;
		}				

		#if defined(_MSC_VER) && _MSC_VER < 1900 // Visual Studio 2013 and below
					
			template<class Other> struct rebind { typedef TestAllocator<Other> other; };

			void construct(TYPE * i_pointer)
			{	
				new (i_pointer) TYPE();
			}

			void construct(TYPE * i_pointer, const TYPE & i_source)
			{
				new (i_pointer) TYPE(i_source);
			}

			template<class OTHER_TYPE, class... ARG_TYPES>
				void construct(OTHER_TYPE * i_pointer, ARG_TYPES &&... i_args)
			{
				new (i_pointer)OTHER_TYPE(std::forward<ARG_TYPES>(i_args)...);
			}

			void destroy(TYPE * i_pointer)
			{
				i_pointer->~TYPE();
				(void)i_pointer; // avoid warning C4100: 'i_pointer' : unreferenced formal parameter
			}

		#endif
	};

	class NoLeakScope
	{
	public:
		NoLeakScope();
		~NoLeakScope();
		NoLeakScope(const NoLeakScope &) = delete;
		NoLeakScope & operator = (const NoLeakScope &) = delete;
	};
	
	/** Runs an exception safeness test, calling the provided function many times.
		First the provided function is called without raising any exception. 
		- Then the function is called, an the first time exception_check_point is called, an exception is thrown
		- then the function is called, an the second time exception_check_point is called, an exception is thrown
		During the execution of the function the function exception_check_point should be called to 
		test the effect of throwing an exception.
	*/
	void run_exception_stress_test(std::function<void()> i_test);

	void exception_check_point();

	namespace details
	{
		class AllocatingTester
		{
		public:

			AllocatingTester();

			bool operator == (const AllocatingTester & i_other) const
			{
				return *m_rand_value == *i_other.m_rand_value;
			}

			bool operator != (const AllocatingTester & i_other) const
			{
				return *m_rand_value != *i_other.m_rand_value;
			}

		private:
			std::shared_ptr<int> m_rand_value;
		};

	} // namespace details

	class Copy_MoveExcept final : public details::AllocatingTester
	{
	public:

		using UnderlyingClass = details::AllocatingTester;

		Copy_MoveExcept() { exception_check_point(); }
		Copy_MoveExcept(const UnderlyingClass & i_source) : UnderlyingClass(i_source) {}

		// copy
		Copy_MoveExcept(const Copy_MoveExcept & i_source)
			: UnderlyingClass((exception_check_point(), i_source))
		{

		}
		Copy_MoveExcept & operator = (const Copy_MoveExcept & i_source)
		{
			exception_check_point();
			UnderlyingClass::operator = (i_source);
			exception_check_point();
			return *this;
		}

		// move (except)
		Copy_MoveExcept(Copy_MoveExcept && i_source)
			: UnderlyingClass((exception_check_point(), std::move(i_source)))
		{
			exception_check_point();
		}
		Copy_MoveExcept & operator = (Copy_MoveExcept && i_source)
		{
			exception_check_point();
			UnderlyingClass::operator = (std::move(i_source));
			exception_check_point();
			return *this;
		}

		bool operator == (const Copy_MoveExcept & i_other) const
			{ return UnderlyingClass::operator == (i_other); }

		bool operator != (const Copy_MoveExcept & i_other) const
			{ return UnderlyingClass::operator != (i_other); }
	};

	class NoCopy_MoveExcept final : public details::AllocatingTester
	{
	public:

		using UnderlyingClass = details::AllocatingTester;

		NoCopy_MoveExcept() { exception_check_point(); }
		NoCopy_MoveExcept(const UnderlyingClass & i_source) : UnderlyingClass(i_source) {}

		// copy
		NoCopy_MoveExcept(const NoCopy_MoveExcept & i_source) = delete;
		NoCopy_MoveExcept & operator = (const NoCopy_MoveExcept & i_source) = delete;

		// move (except)
		NoCopy_MoveExcept(NoCopy_MoveExcept && i_source)
			: UnderlyingClass((exception_check_point(), std::move(i_source)) )
				{ exception_check_point(); }
		NoCopy_MoveExcept & operator = (NoCopy_MoveExcept && i_source)
		{
			exception_check_point();
			UnderlyingClass::operator = (std::move(i_source));
			exception_check_point();
			return *this;
		}

		bool operator == (const NoCopy_MoveExcept & i_other) const
			{ return UnderlyingClass::operator == (i_other); }

		bool operator != (const NoCopy_MoveExcept & i_other) const
			{ return UnderlyingClass::operator != (i_other); }
	};

	class Copy_MoveNoExcept final : public details::AllocatingTester
	{
	public:

		using UnderlyingClass = details::AllocatingTester;

		Copy_MoveNoExcept() { exception_check_point(); }
		Copy_MoveNoExcept(const UnderlyingClass & i_source) : UnderlyingClass(i_source) {}

		// copy
		Copy_MoveNoExcept(const Copy_MoveNoExcept & i_source)
			: UnderlyingClass((exception_check_point(), i_source))
		{

		}
		Copy_MoveNoExcept & operator = (const Copy_MoveNoExcept & i_source)
		{
			exception_check_point();
			UnderlyingClass::operator = (i_source);
			exception_check_point();
			return *this;
		}

		// move (no except)
		Copy_MoveNoExcept(Copy_MoveNoExcept && i_source) noexcept = default;
		Copy_MoveNoExcept & operator = (Copy_MoveNoExcept && i_source) noexcept = default;

		bool operator == (const Copy_MoveNoExcept & i_other) const
		{
			return UnderlyingClass::operator == (i_other);
		}

		bool operator != (const Copy_MoveNoExcept & i_other) const
		{
			return UnderlyingClass::operator != (i_other);
		}
	};

	class NoCopy_MoveNoExcept final : public details::AllocatingTester
	{
	public:

		using UnderlyingClass = details::AllocatingTester;

		NoCopy_MoveNoExcept() { exception_check_point(); }
		NoCopy_MoveNoExcept(const UnderlyingClass & i_source) : UnderlyingClass(i_source) {}

		// copy
		NoCopy_MoveNoExcept(const NoCopy_MoveNoExcept & i_source) = delete;
		NoCopy_MoveNoExcept & operator = (const NoCopy_MoveNoExcept & i_source) = delete;

		// move (no except)
		NoCopy_MoveNoExcept(NoCopy_MoveNoExcept && i_source) noexcept = default;
		NoCopy_MoveNoExcept & operator = (NoCopy_MoveNoExcept && i_source) noexcept = default;

		bool operator == (const NoCopy_MoveNoExcept & i_other) const
		{
			return UnderlyingClass::operator == (i_other);
		}

		bool operator != (const NoCopy_MoveNoExcept & i_other) const
		{
			return UnderlyingClass::operator != (i_other);
		}
	};

} // namespace reflective
