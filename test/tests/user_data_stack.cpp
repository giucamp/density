
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "../test_framework/density_test_common.h"
#include "../test_framework/test_allocators.h"
#include "../test_framework/exception_tests.h"
#include "../test_framework/statistics.h"
#include <density/lifo.h>
#include <vector>
#include <iostream>
#include <algorithm>
#include <chrono>

namespace density_tests
{
    /** Decorator to lifo_allocator that adds debug checks to detect violations of the LIFO order, wrong size passed
        to deallocate or reallocate and leaks of lifo blocks. Functions that can throw call exception_checkpoint()
        to allow exception testing. */
    template <size_t ALIGNMENT = alignof(void*)>
        class DebugDataStack
    {
    public:

        using underlying_allocator = density_tests::DeepTestAllocator<>;

        /** same to lifo_allocator::alignment */
        static constexpr size_t alignment = ALIGNMENT;

        DebugDataStack()
        {
        }

        /** same to lifo_allocator::allocate */
        void * allocate(size_t i_size)
        {
            exception_checkpoint();
            auto const block = m_allocator.allocate(i_size);
            notify_alloc(block, i_size);
            return block;
        }

        /** same to lifo_allocator::allocate_empty */
        void * allocate_empty() noexcept
        {
            auto const block = m_allocator.allocate_empty();
            notify_alloc(block, 0);
            return block;
        }

        /** same to lifo_allocator::reallocate */
        void * reallocate(void * i_block, size_t i_old_size, size_t i_new_size)
        {
            exception_checkpoint();
            auto const new_block = m_allocator.reallocate(i_block, i_old_size, i_new_size);
            notify_dealloc(i_block, i_old_size);
            notify_alloc(new_block, i_new_size);
            return new_block;
        }

        /** same to lifo_allocator::deallocate */
        void deallocate(void * i_block, size_t i_size) noexcept
        {
            notify_dealloc(i_block, i_size);
            m_allocator.deallocate(i_block, i_size);
        }

        ~DebugDataStack()
        {
            DENSITY_TEST_ASSERT(m_lifo_blocks.empty());
        }

        struct Stats
        {
            Statistics m_lifo_blocks;

            Statistics m_page_count;
            Statistics m_page_block_count;
            Statistics m_page_used_space;

            Statistics m_external_block_count;
            Statistics m_external_block_size;
            size_t m_sample_count = 0;

            std::chrono::high_resolution_clock::time_point const m_start_time = std::chrono::high_resolution_clock::now();

            constexpr static size_t s_table_cell_width = 20;

            static void write_stats_header(std::ostream & i_stream)
            {
                std::string const separator_line(s_table_cell_width * 3 + 4, '-');
                
                i_stream << separator_line << '\n';
                i_stream << '|' << format_fixed("thread", s_table_cell_width);
                i_stream << '|' << format_fixed("page_count", s_table_cell_width);
                i_stream << '|' << format_fixed("ext_block_count", s_table_cell_width);
                i_stream << "|\n";

                i_stream << '|' << format_fixed("lifo_blocks", s_table_cell_width);
                i_stream << '|' << format_fixed("page_block_count", s_table_cell_width);
                i_stream << '|' << format_fixed("ext_block_size(%)", s_table_cell_width);
                i_stream << "|\n";

                i_stream << '|' << format_fixed("time (secs)", s_table_cell_width);
                i_stream << '|' << format_fixed("page_used_space(%)", s_table_cell_width);
                i_stream << '|' << format_fixed("stat sample count", s_table_cell_width);
                i_stream << "|\n" << separator_line << '\n';
            }

            void write_stats(std::ostream & i_stream, const char * i_thread_name) const
            {
                using FpSeconds = std::chrono::duration<double, std::chrono::seconds::period>;
                auto const elapsed = static_cast<FpSeconds>(std::chrono::high_resolution_clock::now() - m_start_time);

                std::string const separator_line(s_table_cell_width * 3 + 4, '-');

                i_stream << '|' << format_fixed(i_thread_name, s_table_cell_width);
                i_stream << '|' << format_fixed(m_page_count, s_table_cell_width);
                i_stream << '|' << format_fixed(m_external_block_count, s_table_cell_width);
                i_stream << "|\n";

                i_stream << '|' << format_fixed(m_lifo_blocks, s_table_cell_width);
                i_stream << '|' << format_fixed(m_page_block_count, s_table_cell_width);
                i_stream << '|' << format_fixed(m_external_block_size, s_table_cell_width);
                i_stream << "|\n";

                i_stream << '|' << format_fixed(elapsed.count(), s_table_cell_width);
                i_stream << '|' << format_fixed(m_page_used_space, s_table_cell_width);
                i_stream << '|' << format_fixed(m_sample_count, s_table_cell_width);
                i_stream << "|\n" << separator_line << '\n';
            }
        };

        const Stats & stats() const noexcept
        {
            return m_stats;
        }

        void stat_sample()
        {
            // construct pages, a vector of pages sorted by age
            struct Page
            {
                void * m_page = nullptr;
                size_t m_progressive = 0;
                size_t m_used_size = 0;
                size_t m_blocks = 0;
            };
            std::vector<Page> pages;
            m_allocator.underlying_allocator_ref().for_each_page([&pages](void * i_page, size_t i_progressive) {
                Page page;
                page.m_page = i_page;
                page.m_progressive = i_progressive;
                pages.push_back(page);
            });
            std::sort(pages.begin(), pages.end(), [](const Page & i_first, const Page & i_second) {
                return i_first.m_progressive < i_second.m_progressive;
            });

            // construct external_blocks, a vector of external blocks sorted by age
            struct ExternalBlock
            {
                void * m_block = nullptr;
                underlying_allocator::BlockInfo m_block_info;
            };
            std::vector<ExternalBlock> external_blocks;
            m_allocator.underlying_allocator_ref().for_each_block([&external_blocks](void * i_block, const underlying_allocator::BlockInfo & i_info) {
                ExternalBlock block;
                block.m_block = i_block;
                block.m_block_info = i_info;
                external_blocks.push_back(block);
            });
            std::sort(external_blocks.begin(), external_blocks.end(), [](const ExternalBlock & i_first, const ExternalBlock & i_second) {
                return i_first.m_block_info.m_progressive < i_second.m_block_info.m_progressive;
            });

            // update stats about counts
            m_stats.m_sample_count++;
            m_stats.m_lifo_blocks.sample(static_cast<double>(m_lifo_blocks.size()));
            m_stats.m_page_count.sample(static_cast<double>(pages.size()));
            m_stats.m_external_block_count.sample(static_cast<double>(external_blocks.size()));

            // check the consistency
            void * const current_top = get_top_pointer();
            void * const virgin_top = get_virgin_top();
            if (current_top == virgin_top)
            {
                /* virgin data-stack: only external blocks must exist. external_blocks and m_lifo_blocks must 
                    match exactly (after stripping virgin blocks from m_lifo_blocks */
                auto non_virgin_lifo_blocks = m_lifo_blocks;
                non_virgin_lifo_blocks.erase(
                    std::remove_if(non_virgin_lifo_blocks.begin(), non_virgin_lifo_blocks.end(), 
                        [virgin_top](const Block & i_block){ return i_block.m_block == virgin_top; }),
                    non_virgin_lifo_blocks.end());

                DENSITY_TEST_ASSERT(pages.size() == 0);
                DENSITY_TEST_ASSERT(external_blocks.size() == non_virgin_lifo_blocks.size());
                for (size_t index = 0; index < external_blocks.size(); index++)
                {
                    auto const & lifo_block = non_virgin_lifo_blocks[index];
                    auto const & external_block = external_blocks[index];
                    DENSITY_TEST_ASSERT(external_block.m_block == lifo_block.m_block);
                    DENSITY_TEST_ASSERT(external_block.m_block_info.m_size == lifo_block.m_size);

                    m_stats.m_external_block_size.sample(size_percentage(external_block.m_block_info.m_size));
                }
            }
            else
            {
                DENSITY_TEST_ASSERT(!pages.empty()); // at least one page must exist

                // iterate m_lifo_blocks
                Block prev_inpage_block;
                size_t page_index = 0, external_block_index = 0;
                for(size_t lifo_block_index = 0; lifo_block_index < m_lifo_blocks.size(); lifo_block_index++)
                {
                    const auto & block = m_lifo_blocks[lifo_block_index];
                    if (block.m_block == virgin_top)
                    {
                        // empty virgin blocks can appear only before the first in-page block
                        DENSITY_TEST_ASSERT(prev_inpage_block.m_block == nullptr);
                    }
                    else if (external_block_index < external_blocks.size() && external_blocks[external_block_index].m_block == block.m_block)
                    {
                        // consume an external block
                        DENSITY_TEST_ASSERT(external_blocks[external_block_index].m_block_info.m_size == block.m_size);
                        m_stats.m_external_block_size.sample(size_percentage(external_blocks[external_block_index].m_block_info.m_size));
                        external_block_index++;
                    }
                    else
                    {
                        if (!same_page(block.m_block, prev_inpage_block.m_block))
                        {
                            // page switch
                            if (prev_inpage_block.m_block != nullptr)
                            {
                                // not first page
                                m_stats.m_page_block_count.sample(static_cast<double>(pages[page_index].m_blocks));
                                m_stats.m_page_used_space.sample(size_percentage(pages[page_index].m_used_size));
                                page_index++;
                                DENSITY_TEST_ASSERT(page_index < pages.size());
                            }
                        }
                        else
                        {
                            // no page switch, this block must begin at the end of the previous in-page block
                            DENSITY_TEST_ASSERT(block.m_block == density::address_add(prev_inpage_block.m_block, prev_inpage_block.m_size));
                        }
                        DENSITY_TEST_ASSERT(same_page(block.m_block, pages[page_index].m_page));                        

                        // update the page
                        pages[page_index].m_blocks++;
                        pages[page_index].m_used_size += block.m_size;

                        prev_inpage_block = block;
                    }
                }

                // we must have consumed all the elements of pages and external_blocks
                DENSITY_TEST_ASSERT(page_index + 1 == pages.size());
                DENSITY_TEST_ASSERT(external_block_index == external_blocks.size());
            }
        }

    private:

        static int size_percentage(size_t i_size)
        {
            auto const factor = 100. / static_cast<double>(underlying_allocator::page_size);
            return static_cast<int>(i_size * factor + .5);
        }

        static bool same_page(const void * i_first, const void * i_second) noexcept
        {
            auto const page_mask = underlying_allocator::page_alignment - 1;
            return ((reinterpret_cast<uintptr_t>(i_first) ^ reinterpret_cast<uintptr_t>(i_second)) & ~page_mask) == 0;
        }

        void * get_top_pointer() const
        {
            /* this function makes the assumption that allocate_empty just returns the top pointer
                without altering the state of the allocator. */
            return const_cast<DebugDataStack*>(this)->m_allocator.allocate_empty();
        }

        void * get_virgin_top() const
        {
            /* this function makes the assumption that allocate_empty just returns the top pointer. Even the
                notion of virgin allocator is an implementation detail of density::lifo_allocator. */
            return density::lifo_allocator<underlying_allocator, ALIGNMENT>().allocate_empty();
        }

        void notify_alloc(void * i_block, size_t i_size)
        {
            Block new_block;
            new_block.m_block = i_block;
            new_block.m_size = i_size;
            m_lifo_blocks.push_back(new_block);
        }

        void notify_dealloc(void * i_block, size_t i_size)
        {
            DENSITY_TEST_ASSERT(!m_lifo_blocks.empty() &&
                m_lifo_blocks.back().m_block == i_block &&
                m_lifo_blocks.back().m_size == i_size);
            m_lifo_blocks.pop_back();
        }

    private:

        struct Block
        {
            void * m_block = nullptr;
            size_t m_size = 0;

            bool belongs_to_page(void * i_page) const noexcept
            {
                auto const page_start = density::address_lower_align(i_page, underlying_allocator::page_alignment);
                auto const page_end = density::address_add(page_start, underlying_allocator::page_size);
                auto const block_end = density::address_add(m_block, m_size);
                return m_block >= page_start && block_end < page_end;
            }
        };
        std::vector<Block> m_lifo_blocks; /**< used to check the LIFO order */        
        density::lifo_allocator<underlying_allocator, ALIGNMENT> m_allocator;
        Stats m_stats;
    };

} // namespace density_tests

#ifdef DENSITY_USER_DATA_STACK
    /** Override the data stack with a debug user defined data stack that:
            - detects violations of the LIFO constraint and leaks of lifo blocks
            - detects wrong sizes passed to deallocate or reallocate
            - detects leaks of pages or external blocks
            - allows testing the exceptional paths */
    namespace density
    {
        namespace user_data_stack
        {
            namespace
            {
                thread_local density_tests::DebugDataStack<alignment> user_data_stack;
                    // alignment is defined in density_config.h (density::user_data_stack::alignment)
            }

            void * allocate(size_t i_size)
            {
                return user_data_stack.allocate(i_size);
            }

            void * allocate_empty() noexcept
            {
                return user_data_stack.allocate_empty();
            }

            void * reallocate(void * i_block, size_t i_old_size, size_t i_new_size)
            {
                return user_data_stack.reallocate(i_block, i_old_size, i_new_size);
            }

            void deallocate(void * i_block, size_t i_size) noexcept
            {
                user_data_stack.deallocate(i_block, i_size);
            }


            /* If DENSITY_USER_DATA_STACK is defined, this test program implements this function
                that make a complete check of the data-stack, and updates the internal statistics. */
            void stat_sample()
            {
                user_data_stack.stat_sample();
            }

            void stats_header(std::ostream & i_dest)
            {
                user_data_stack.stats().write_stats_header(i_dest);
            }

            /* Prints the internal statistics. */
            void stats_print(std::ostream & i_dest, const char * i_thread_name)
            {
                user_data_stack.stats().write_stats(i_dest, i_thread_name);
            }

        } // namespace user_data_stack

    } // namespace density

#endif // #ifdef DENSITY_USER_DATA_STACK
