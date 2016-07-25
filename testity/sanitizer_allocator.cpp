
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "sanitizer_allocator.h"
#include <atomic>
#ifdef _WIN32 // currently supported only on windows
	#include <assert.h>
	#include <windows.h>
#endif

namespace testity
{
	#ifdef _WIN32 // currently supported only on windows

	class SanitizerAllocator::Impl
	{
	public:

		struct AllocationHeader
		{
			void * m_block;
			uintptr_t m_progressive;
			size_t m_size, m_alignment, m_alignment_offset;
			size_t m_whole_size;
		};

		static bool is_aligned(void * i_address, size_t i_alignment, size_t i_alignment_offset)
		{
			auto address = reinterpret_cast<uintptr_t>(i_address);
			address += static_cast<uintptr_t>(i_alignment_offset);
			return (address & (static_cast<uintptr_t>(i_alignment) - 1)) == 0;
		}

		static void * lower_align(void * i_address, size_t i_alignment)
		{
			auto address = reinterpret_cast<uintptr_t>(i_address);
			address &= ~(static_cast<uintptr_t>(i_alignment) - 1);
			return reinterpret_cast<void*>(address);
		}

		Impl()
		{
			SYSTEM_INFO system_info;
			ZeroMemory(&system_info, sizeof(system_info));
			GetSystemInfo(&system_info);
			m_page_size = system_info.dwPageSize;
			if (m_page_size == 0 || ((m_page_size & (m_page_size - 1))) != 0)
			{
				throw std::exception();
			}
			m_page_mask = m_page_size - 1;
		}

		AllocationHeader * get_header(void * i_block)
		{
			auto address = reinterpret_cast<uintptr_t>(i_block);
			if ((address & static_cast<uintptr_t>(m_page_size - 1)) < sizeof(AllocationHeader))
			{
				address -= m_page_size;
			}

			address &= ~static_cast<uintptr_t>(m_page_size - 1);
			return reinterpret_cast<AllocationHeader*>(address);
		}

		void * allocate(size_t i_size, size_t i_alignment, size_t i_alignment_offset)
		{
			if (i_alignment == 0 || ((i_alignment & (i_alignment - 1))) != 0)
			{
				throw std::exception();
			}

			size_t total_size = ((i_size + m_page_mask) / m_page_size) * m_page_size;

			for(;;)
			{
				uintptr_t remaining_size = total_size;
				if (remaining_size < i_size)
				{
					total_size += m_page_size;
					continue;
				}
				remaining_size -= i_size;

				remaining_size += i_alignment_offset;
				remaining_size &= ~static_cast<uintptr_t>(i_alignment - 1);

				if (remaining_size < i_alignment_offset)
				{
					total_size += m_page_size;
					continue;
				}

				remaining_size -= i_alignment_offset;

				if (remaining_size < sizeof(AllocationHeader))
				{
					total_size += m_page_size;
					continue;
				}

				break;
			}

			void * pages = VirtualAlloc(NULL, total_size + m_page_size, MEM_RESERVE, PAGE_NOACCESS );
			if (pages == nullptr)
			{
				throw std::bad_alloc();
			}

			pages = VirtualAlloc(pages, total_size, MEM_COMMIT, PAGE_READWRITE);
			if (pages == nullptr)
			{
				throw std::bad_alloc();
			}

			uintptr_t address = reinterpret_cast<uintptr_t>(pages) + total_size;
			address -= i_size;
			address += i_alignment_offset;
			address &= ~static_cast<uintptr_t>(i_alignment - 1);
			address -= i_alignment_offset;
			
			void * block = reinterpret_cast<void*>(address);
			assert(is_aligned(block, i_alignment, i_alignment_offset));

			auto header = static_cast<AllocationHeader*>(pages);
			assert(header == get_header(block));
			header->m_block = block;
			header->m_size = i_size;
			header->m_alignment = i_alignment;
			header->m_alignment_offset = i_alignment_offset;
			header->m_whole_size = total_size + m_page_size;
			header->m_progressive = s_next_proressive++;
			
			assert(!IsBadWritePtr(block, i_size));

			return block;
		}

		void deallocate(void * i_block, const size_t * i_size, const size_t * i_alignment, const size_t * i_alignment_offset)
		{
			if (i_block != nullptr)
			{
				AllocationHeader * header = get_header(i_block);
				assert(header->m_block == i_block);
				assert(i_size == nullptr || header->m_size == *i_size);
				assert(i_alignment == nullptr || header->m_alignment == *i_alignment);
				assert(i_alignment_offset == nullptr || header->m_alignment_offset == *i_alignment_offset);

				BOOL result = VirtualFree(header, header->m_whole_size, MEM_DECOMMIT);
				assert(result);
				(void)result;
			}
		}

	private:
		size_t m_page_size, m_page_mask;
		static uintptr_t s_next_proressive;
	};

	uintptr_t SanitizerAllocator::Impl::s_next_proressive(1);

	SanitizerAllocator::SanitizerAllocator()
		: m_impl(new Impl)
	{

	}

	SanitizerAllocator::SanitizerAllocator(const SanitizerAllocator &)
		: m_impl(new Impl)
	{
	}

	SanitizerAllocator::SanitizerAllocator(SanitizerAllocator &&) noexcept
		: m_impl(new Impl)
	{

	}

	SanitizerAllocator & SanitizerAllocator::operator = (const SanitizerAllocator &)
	{
		return *this;
	}

	SanitizerAllocator & SanitizerAllocator::operator = (SanitizerAllocator &&) noexcept
	{
		return *this;
	}

	SanitizerAllocator::~SanitizerAllocator() = default;

	void * SanitizerAllocator::allocate(size_t i_size, size_t i_alignment, size_t i_alignment_offset)
	{
		return m_impl->allocate(i_size, i_alignment, i_alignment_offset);
	}

	void SanitizerAllocator::deallocate(void * i_block)
	{
		m_impl->deallocate(i_block, nullptr, nullptr, nullptr);
	}

	void SanitizerAllocator::deallocate(void * i_block, size_t i_size)
	{
		m_impl->deallocate(i_block, &i_size, nullptr, nullptr);
	}

	void SanitizerAllocator::deallocate(void * i_block, size_t i_size, size_t i_alignment, size_t i_alignment_offset)
	{
		m_impl->deallocate(i_block, &i_size, &i_alignment, &i_alignment_offset);
	}

	#endif // #ifdef _WIN32
}
