
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)


#pragma once
#include "dense_queue.h"

namespace density
{
    template < typename FUNCTION > class dense_function_queue;
	
	/** \brief Queue of callable objects (or function object).  */
    template < typename RET_VAL, typename... PARAMS >
        class dense_function_queue<RET_VAL (PARAMS...)>
    {
    public:

        using value_type = RET_VAL(PARAMS...);

        template <typename ELEMENT_COMPLETE_TYPE>
            void push(ELEMENT_COMPLETE_TYPE && i_source)
        {
            m_queue.push(std::forward<ELEMENT_COMPLETE_TYPE>(i_source));
        }

        RET_VAL invoke_front(PARAMS... i_params)
        {
            auto first = m_queue.begin();
            return first.complete_type().template get_feature<typename detail::FeatureInvoke<value_type>>()(first.element(), i_params...);
        }

        RET_VAL consume_front(PARAMS... i_params)
        {
            return m_queue.manual_consume([&i_params...](const runtime_type<void, Features > & i_complete_type, void * i_element) {
                return i_complete_type.template get_feature<typename detail::FeatureInvokeDestroy<value_type>>()(i_element, i_params...);
            } );
        }

        bool empty() DENSITY_NOEXCEPT
        {
            return m_queue.empty();
        }

        void clear() DENSITY_NOEXCEPT
        {
            m_queue.clear();
        }

    private:
        using Features = detail::FeatureList<
            density::detail::FeatureSize, density::detail::FeatureAlignment,
            density::detail::FeatureCopyConstruct, density::detail::FeatureMoveConstruct,
            density::detail::FeatureDestroy, typename detail::FeatureInvoke<value_type>, typename detail::FeatureInvokeDestroy<value_type> >;
        dense_queue<void, std::allocator<char>, runtime_type<void, Features > > m_queue;
    };

} // namespace density
