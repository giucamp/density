
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <density/conc_heter_queue.h>
#include <density/detail/function_queue_impl.h>

namespace density
{
    template < typename CALLABLE, typename ALLOCATOR_TYPE = void_allocator >
        using concurrent_function_queue = detail::FunctionQueueImpl< conc_heter_queue<void, detail::FunctionRuntimeType<CALLABLE>, ALLOCATOR_TYPE>, CALLABLE >;
}
