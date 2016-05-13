
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "..\dense_function_queue.h"
#include "..\paged_function_queue.h"
#include <functional>
#include <deque>
#include <queue>

namespace density
{
    namespace benchmarks
    {
        void paged_function_queue_1(size_t i_cardinality)
        {
            paged_function_queue< void() > queue;
            for (size_t index = 0; index < i_cardinality; index++)
            {
                queue.push([]() { volatile int dummy = 1; (void)dummy; });
            }
            for (size_t index = 0; index < i_cardinality; index++)
            {
                queue.consume_front();
            }
        }

        void dense_function_queue_1(size_t i_cardinality)
        {
            dense_function_queue< void() > queue;
            for (size_t index = 0; index < i_cardinality; index++)
            {
                queue.push([]() { volatile int dummy = 1; (void)dummy; });
            }
            for (size_t index = 0; index < i_cardinality; index++)
            {
                queue.consume_front();
            }
        }

        void function_queue_1(size_t i_cardinality)
        {
            using namespace std;;
            std::queue< function<void()> > queue;
            for (size_t index = 0; index < i_cardinality; index++)
            {
                queue.push([]() { volatile int dummy = 1; (void)dummy; });
            }
            for (size_t index = 0; index < i_cardinality; index++)
            {
                queue.front()();
                queue.pop();
            }
        }


    } // namespace benchmarks

} // namespace density

