
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "detail\queue_impl.h"
#include <memory>

namespace density
{

    template <typename ELEMENT = void, typename ALLOCATOR = std::allocator<ELEMENT>, typename RUNTIME_TYPE = RuntimeType<ELEMENT> >
        class DenseQueue;

} // namespace density

// definition of the primary class template DenseQueue
#include "detail\dense_queue_typed.h"

// definition of the speciaization of DenseQueue for void
#include "detail\dense_queue_void.h"
