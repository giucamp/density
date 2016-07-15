
#include "test_allocator.h"
#include "testity_common.h"
#include <unordered_map>
#include <mutex>
#include <iostream>

namespace testity
{
	struct SharedBlockRegistry::AllocationEntry
	{
		size_t m_progressive = 0;
		size_t m_size = 0;
		size_t m_alignment = 0;
		size_t m_alignment_offset = 0;
	};

	struct SharedBlockRegistry::Data
	{
		std::mutex m_mutex;
		std::unordered_map<void*, AllocationEntry> m_allocations;
		size_t m_last_progressive = 0;

		~Data()
		{
			TESTITY_ASSERT(m_allocations.size() == 0 );

			for (const auto & leaking_allocation : m_allocations)
			{
				std::cerr << "Leak of " << leaking_allocation.second.m_size << ", progressive: " << leaking_allocation.second.m_progressive << std::endl;
			}
		}
	};
	
	SharedBlockRegistry::SharedBlockRegistry() : m_data(std::make_shared<Data>()) {}

	SharedBlockRegistry::SharedBlockRegistry(const SharedBlockRegistry & i_source) = default;

	SharedBlockRegistry::~SharedBlockRegistry() = default;

	SharedBlockRegistry & SharedBlockRegistry::operator = (const SharedBlockRegistry & i_source) = default;

	SharedBlockRegistry::SharedBlockRegistry(SharedBlockRegistry && i_source) noexcept
		: m_data(i_source.m_data)
	{
		i_source.m_data = nullptr;
	}

	SharedBlockRegistry & SharedBlockRegistry::operator = (SharedBlockRegistry && i_source) noexcept
	{
		m_data = i_source.m_data;
		i_source.m_data = nullptr;
		return *this;
	}

	void SharedBlockRegistry::add_block(void * i_block, size_t i_size, size_t i_alignment, size_t i_alignment_offset)
	{
		TESTITY_ASSERT( m_data != nullptr ); // this registry can't be used to add blocks. Maybe it was used as source for a move.

		if (m_data != nullptr) // avoid a crash, but
		{
			auto & data = *m_data;
			
			AllocationEntry entry;
			entry.m_size = i_size;
			entry.m_alignment = i_alignment;
			entry.m_progressive = data.m_last_progressive++;
			entry.m_alignment_offset = i_alignment_offset;
			// TESTITY_ASSERT(entry.m_progressive != ...);

			std::lock_guard<std::mutex> lock(data.m_mutex);

			auto res = data.m_allocations.insert(std::make_pair(i_block, entry));
			TESTITY_ASSERT(res.second);
		}
	}

	void SharedBlockRegistry::remove_block(void * i_block, size_t i_size, size_t i_alignment, size_t i_alignment_offset)
	{
		TESTITY_ASSERT(m_data != nullptr); // this registry can't be used to remove blocks. Maybe it was used as source for a move.

		if (m_data != nullptr) // avoid a crash, but
		{
			auto & data = *m_data;
			std::lock_guard<std::mutex> lock(data.m_mutex);

			auto it = data.m_allocations.find(i_block);
			TESTITY_ASSERT(it != data.m_allocations.end());
			TESTITY_ASSERT(it->second.m_size == i_size);
			TESTITY_ASSERT(it->second.m_alignment == i_alignment);
			TESTITY_ASSERT(it->second.m_alignment_offset == i_alignment_offset);
			data.m_allocations.erase(it);
		}
	}
}