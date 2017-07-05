
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <density/void_allocator.h>
#include "shared_block_registry.h"
#include "exception_tests.h"
#include <atomic>
#include <iostream>

namespace density_tests
{
    /* Allocators that meets the requirements of both \ref UntypedAllocator_concept "UntypedAllocator"
        and \ref PagedAllocator_concept "PagedAllocator".
		This class is based on density::basic_void_allocator, and uses a shared registry to detect bugs
		in the memory management of containers (and pseudo-containers).
		This class uses a mutex to be thread-safe. */
    template <size_t PAGE_CAPACITY_AND_ALIGNMENT = density::default_page_capacity>
		class DeepTestAllocator : public density::basic_void_allocator<PAGE_CAPACITY_AND_ALIGNMENT>
    {
		using Base = typename density::basic_void_allocator<PAGE_CAPACITY_AND_ALIGNMENT>;

    public:

		static constexpr size_t page_size = Base::page_size;

        static constexpr size_t page_alignment = Base::page_alignment;

        void * allocate(size_t i_size, size_t i_alignment = alignof(std::max_align_t), size_t i_alignment_offset = 0)
        {
			exception_checkpoint();

            auto block = Base::allocate(i_size, i_alignment, i_alignment_offset);
            m_registry.register_block(s_default_category, block, i_size, i_alignment, i_alignment_offset);
            return block;
        }

        void deallocate(void * i_block, size_t i_size, size_t i_alignment = alignof(std::max_align_t), size_t i_alignment_offset = 0) noexcept
        {
            m_registry.unregister_block(s_default_category, i_block, i_size, i_alignment, i_alignment_offset);
            Base::deallocate(i_block, i_size, i_alignment, i_alignment_offset);
        }

        void * allocate_page()
        {
			exception_checkpoint();

            auto page = Base::allocate_page();
            m_registry.register_block(s_page_category, page, page_size, page_alignment, 0);
            return page;
        }

        void deallocate_page(void * i_page) noexcept
        {
            m_registry.unregister_block(s_page_category, 
				density::address_lower_align(i_page, page_alignment), page_size, page_alignment, 0);
            Base::deallocate_page(i_page);
        }

		void * allocate_page_zeroed()
        {
			exception_checkpoint();

            auto page = Base::allocate_page_zeroed();
            m_registry.register_block(s_page_category, page, page_size, page_alignment, 0);
            return page;
        }

        void deallocate_page_zeroed(void * i_page) noexcept
        {
            m_registry.unregister_block(s_page_category, 
				address_lower_align(i_page, page_alignment), page_size, page_alignment, 0);
            Base::deallocate_page_zeroed(i_page);
        }

        bool operator == (const DeepTestAllocator & i_other) const
        {
            return m_registry == i_other.m_registry;
        }

        bool operator != (const DeepTestAllocator & i_other) const
        {
            return m_registry != i_other.m_registry;
        }

		void pin_page(void * i_address) noexcept
		{
			Base::pin_page(i_address);
		}

		void unpin_page(void * i_address) noexcept
		{
			Base::unpin_page(i_address);
		}

    private:
		constexpr static int s_default_category = 2;
		constexpr static int s_page_category = 4;
        SharedBlockRegistry m_registry;
    };

	/* Allocators that meets the requirements of both \ref UntypedAllocator_concept "UntypedAllocator"
        and \ref PagedAllocator_concept "PagedAllocator".
		This class is based on density::basic_void_allocator, and uses atomic counters to detect bugs
		in the memory management of containers (and pseudo-containers).
		This class is lock-free if std::atomic<uintptr_t> is lock-free. */
	template <size_t PAGE_CAPACITY_AND_ALIGNMENT = density::default_page_capacity>
		class UnmovableFastTestAllocator : private density::basic_void_allocator<PAGE_CAPACITY_AND_ALIGNMENT>
	{
		using Base = typename density::basic_void_allocator<PAGE_CAPACITY_AND_ALIGNMENT>;
	
	public:

		
		static constexpr size_t page_size = Base::page_size;

		static constexpr size_t page_alignment = Base::page_alignment;

		UnmovableFastTestAllocator() noexcept = default;        
		
		~UnmovableFastTestAllocator()
		{
			auto const living_pages = m_living_pages.load();
			auto const total_allocated_pages = m_total_allocated_pages.load();
			auto const living_pins = m_living_pins.load();
			auto const living_allocations = m_living_allocations.load();
			auto const living_bytes = m_living_bytes.load();
			auto const total_allocations = m_total_allocations.load();

			DENSITY_TEST_ASSERT(living_pages == 0);
			DENSITY_TEST_ASSERT(living_pins == 0);
			DENSITY_TEST_ASSERT(living_bytes == 0);

			std::cout << "Destroying UnmovableFastTestAllocator."
				<< " page_size: " << page_size
				<< ", page_alignment: " << page_alignment
				<< ", total_allocated_pages: " << total_allocated_pages
				<< ", total_allocations: " << total_allocations << std::endl;
		}
		
		UnmovableFastTestAllocator(const UnmovableFastTestAllocator&) noexcept = delete;
        UnmovableFastTestAllocator & operator = (const UnmovableFastTestAllocator&) noexcept = delete;

		void * allocate(size_t i_size,
			size_t i_alignment = alignof(std::max_align_t),
			size_t i_alignment_offset = 0) noexcept
		{
			m_living_allocations.fetch_add(1, std::memory_order_relaxed);
			m_living_bytes.fetch_add(i_size, std::memory_order_relaxed);
			m_total_allocations.fetch_add(1, std::memory_order_relaxed);

			return Base::allocate(i_size, i_alignment, i_alignment_offset);
		}

		void deallocate(void * i_block,
			size_t i_size,
			size_t i_alignment = alignof(std::max_align_t),
			size_t i_alignment_offset = 0) noexcept
		{
			Base::deallocate(i_block, i_size, i_alignment, i_alignment_offset);

			auto const prev_living_allocations = m_living_allocations.fetch_sub(1, std::memory_order_relaxed);
			auto const prev_living_bytes = m_living_bytes.fetch_sub(i_size, std::memory_order_relaxed);
			DENSITY_TEST_ASSERT(prev_living_allocations >= 1 && prev_living_bytes >= i_size);
		}

		void * allocate_page()
        {
			m_living_pages.fetch_add(1, std::memory_order_relaxed);
			m_total_allocated_pages.fetch_add(1, std::memory_order_relaxed);
            auto const result = Base::allocate_page();
			DENSITY_TEST_ASSERT(result != nullptr && density::address_is_aligned(result, page_alignment));
			return result;
        }

		void * allocate_page_zeroed()
		{
			m_living_pages.fetch_add(1, std::memory_order_relaxed);
			m_total_allocated_pages.fetch_add(1, std::memory_order_relaxed);
			auto const result = Base::allocate_page_zeroed();
			DENSITY_TEST_ASSERT(result != nullptr && address_is_aligned(result, page_alignment));

			DENSITY_TEST_ASSERT(density::detail::mem_equal(result, page_size, 0));
			return result;
		}

		void deallocate_page(void * i_page) noexcept
        {
			Base::deallocate_page(i_page);

			auto const prev_living_pages = m_living_pages.fetch_sub(1, std::memory_order_relaxed);
			DENSITY_TEST_ASSERT(prev_living_pages >= 1);
        }

		void deallocate_page_zeroed(void * i_page) noexcept
		{
			Base::deallocate_page_zeroed(i_page);

			auto const prev_living_pages = m_living_pages.fetch_sub(1, std::memory_order_relaxed);
			DENSITY_TEST_ASSERT(prev_living_pages >= 1);
		}

		void pin_page(void * i_address) noexcept
		{
			m_living_pins.fetch_add(1, std::memory_order_relaxed);
			Base::pin_page(i_address);
		}

		void unpin_page(void * i_address) noexcept
		{
			Base::unpin_page(i_address);

			auto const prev_living_pins = m_living_pins.fetch_sub(1, std::memory_order_relaxed);
			DENSITY_TEST_ASSERT(prev_living_pins >= 1);
		}

		uintptr_t get_pin_count(const void * i_address) noexcept
		{
			return Base::get_pin_count(i_address);
		}

		bool operator == (const UnmovableFastTestAllocator & i_other) const noexcept
        {
			return this == &i_other;
		}

		bool operator != (const UnmovableFastTestAllocator & i_other) const noexcept
        {
			return this == &i_other;
		}

	private:
		std::atomic<uintptr_t> m_living_pages{ 0 };
		std::atomic<uintptr_t> m_total_allocated_pages{ 0 };
		std::atomic<uintptr_t> m_living_pins{ 0 };
		std::atomic<uintptr_t> m_living_allocations{ 0 };
		std::atomic<uintptr_t> m_living_bytes{ 0 };
		std::atomic<uintptr_t> m_total_allocations{ 0 };
	};

} // density_tests

