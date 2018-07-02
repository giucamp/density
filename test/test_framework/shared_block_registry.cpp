
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "../test_framework/density_test_common.h"
//

#include "shared_block_registry.h"
#include <atomic>
#include <iostream>
#include <mutex>
#include <unordered_map>

namespace density_tests
{
    struct SharedBlockRegistry::Data
    {
        std::mutex                            m_mutex;
        std::unordered_map<void *, BlockInfo> m_allocations;
        static std::atomic<size_t>            s_last_progressive;

        ~Data()
        {
            DENSITY_TEST_ASSERT(m_allocations.size() == 0);

            for (const auto & leaking_allocation : m_allocations)
            {
                std::cerr << "Leak of " << leaking_allocation.second.m_size
                          << ", progressive: " << leaking_allocation.second.m_progressive
                          << std::endl;
            }
        }
    };

    std::atomic<size_t> SharedBlockRegistry::Data::s_last_progressive(0);

    SharedBlockRegistry::SharedBlockRegistry() : m_data(std::make_shared<Data>()) {}

    SharedBlockRegistry::SharedBlockRegistry(const SharedBlockRegistry & i_source) = default;

    SharedBlockRegistry::~SharedBlockRegistry() = default;

    SharedBlockRegistry & SharedBlockRegistry::
                          operator=(const SharedBlockRegistry & i_source) = default;

    SharedBlockRegistry::SharedBlockRegistry(SharedBlockRegistry && i_source) noexcept
        : m_data(i_source.m_data)
    {
        i_source.m_data = nullptr;
    }

    SharedBlockRegistry & SharedBlockRegistry::operator=(SharedBlockRegistry && i_source) noexcept
    {
        m_data          = i_source.m_data;
        i_source.m_data = nullptr;
        return *this;
    }

    void SharedBlockRegistry::register_block(
      int i_category, void * i_block, size_t i_size, size_t i_alignment, size_t i_alignment_offset)
    {
        DENSITY_TEST_ASSERT(i_alignment == 0 || ((i_alignment & (i_alignment - 1))) == 0);
        DENSITY_TEST_ASSERT(
          m_data !=
          nullptr); // this registry can't be used to add blocks. Maybe it was used as source for a move.

        if (m_data != nullptr) // avoid a crash, but
        {
            BlockInfo entry;
            entry.m_category         = i_category;
            entry.m_size             = i_size;
            entry.m_alignment        = i_alignment;
            entry.m_progressive      = Data::s_last_progressive++;
            entry.m_alignment_offset = i_alignment_offset;
            // DENSITY_TEST_ASSERT(entry.m_progressive != ...);

            auto &                      data = *m_data;
            std::lock_guard<std::mutex> lock(data.m_mutex);
            auto res = data.m_allocations.insert(std::make_pair(i_block, entry));
            DENSITY_TEST_ASSERT(res.second);
        }
    }

    void SharedBlockRegistry::unregister_block(
      int i_category, void * i_block, size_t i_size, size_t i_alignment, size_t i_alignment_offset)
    {
        if (i_block != nullptr)
        {
            DENSITY_TEST_ASSERT(
              m_data !=
              nullptr); // this registry can't be used to remove blocks. Maybe it was used as source for a move.

            if (m_data != nullptr) // avoid a crash, but
            {
                auto &                      data = *m_data;
                std::lock_guard<std::mutex> lock(data.m_mutex);

                auto it = data.m_allocations.find(i_block);
                DENSITY_TEST_ASSERT(it != data.m_allocations.end());
                DENSITY_TEST_ASSERT(it->second.m_category == i_category);
                DENSITY_TEST_ASSERT(it->second.m_size == i_size);
                DENSITY_TEST_ASSERT(it->second.m_alignment == i_alignment);
                DENSITY_TEST_ASSERT(it->second.m_alignment_offset == i_alignment_offset);
                data.m_allocations.erase(it);
            }
        }
    }

    void SharedBlockRegistry::for_each_block(
      const std::function<void(void * i_block, const BlockInfo &)> & i_callback) const
    {
        auto &                      data = *m_data;
        std::lock_guard<std::mutex> lock(data.m_mutex);
        for (auto & entry : data.m_allocations)
        {
            i_callback(entry.first, entry.second);
        }
    }

    void SharedBlockRegistry::for_each_block(
      int                                                            i_category,
      const std::function<void(void * i_block, const BlockInfo &)> & i_callback) const
    {
        auto &                      data = *m_data;
        std::lock_guard<std::mutex> lock(data.m_mutex);
        for (auto & entry : data.m_allocations)
        {
            if (entry.second.m_category == i_category)
            {
                i_callback(entry.first, entry.second);
            }
        }
    }

} // namespace density_tests
