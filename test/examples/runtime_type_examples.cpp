
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "../test_framework/density_test_common.h"
//
#include <complex>
#include <density/io_runtimetype_features.h>
#include <density/runtime_type.h>
#include <iostream>
#include <mutex>
#include <string>
#include <type_traits>

namespace density_tests
{
    // clang-format off
    void feature_list_examples()
    {
        using namespace density;

        {
        //! [feature_list example 1]
    using FewFeatures = feature_list<f_size, f_alignment>;
    using MoreFeatures = feature_list<FewFeatures, f_default_construct, f_copy_construct, f_move_construct, f_destroy>;
    using ManyFeatures = feature_list<MoreFeatures, f_equal, f_less, f_hash>;
        //! [feature_list example 1]

        //! [feature_list example 2]
    static_assert(std::is_same<
        FewFeatures::tuple_type, 
        std::tuple<f_size, f_alignment>>::value, "");
        //! [feature_list example 2]

        //! [feature_list example 3]
    static_assert(std::is_same<
        ManyFeatures::tuple_type, std::tuple<
            f_size, f_alignment,
            f_default_construct, f_copy_construct, f_move_construct, f_destroy,
            f_equal, f_less, f_hash
        >>::value, "");
        //! [feature_list example 3]
        }
        {
        //! [feature_list example 4]
    // Features1, Features2 and Features3 are equivalent....
    using Features1 = feature_list<f_size, f_alignment, f_copy_construct>;
    using Features2 = feature_list< feature_list<f_size>, 
        feature_list<f_alignment, f_copy_construct>>;
    using Features3 = feature_list<
        feature_list<f_size, f_none>, feature_list<feature_list<f_none>>,
        feature_list<f_size, f_alignment, f_copy_construct, f_none, f_copy_construct, f_size>>;
        
    // ...because they produce the same tuple
    static_assert(std::is_same<Features1::tuple_type, Features2::tuple_type>::value, "");
    static_assert(std::is_same<Features2::tuple_type, Features3::tuple_type>::value, "");
        //! [feature_list example 4]
        //! [feature_list example 5]
    using MyFeatures    = feature_list<f_size, f_alignment, f_copy_construct>;
    using MyRuntimeType = runtime_type<MyFeatures>;

    // this is ok: int supports sizeof, alignof, and copy construction
    auto IntType = MyRuntimeType::make<int>();

    // this fails to compile: std::mutex doesen't allow copy construction
    // auto MutexType = MyRuntimeType::make<std::mutex>();

    // MyFeatures::tuple<void> = std::tuple<f_size::type<void>, f_alignment::type<void>, f_copy_construct::type<void>>
    static_assert(
        std::is_same<MyFeatures::tuple_type, std::tuple<f_size, f_alignment, f_copy_construct>>::
        value,
        "");
        //! [feature_list example 5]
        }
        {
        //! [has_features example 1]
    using MyFeatures = feature_list<f_size, f_alignment>;
    static_assert(has_features<MyFeatures>::value, "");
    static_assert(has_features<MyFeatures, f_size>::value, "");
    static_assert(has_features<MyFeatures, f_alignment>::value, "");
    static_assert(has_features<MyFeatures, f_size, f_alignment>::value, "");
    static_assert(!has_features<MyFeatures, f_copy_construct>::value, "");
    static_assert(!has_features<MyFeatures, f_size, f_copy_construct>::value, "");
    static_assert(!has_features<MyFeatures, f_copy_construct, f_size>::value, "");
        //! [has_features example 1]
        }
        {
        //! [has_features example 2]
    using MyFeatures = runtime_type<f_size, f_alignment>;
    static_assert(has_features<MyFeatures>::value, "");
    static_assert(has_features<MyFeatures, f_size>::value, "");
    static_assert(has_features<MyFeatures, f_alignment>::value, "");
    static_assert(has_features<MyFeatures, f_size, f_alignment>::value, "");
    static_assert(!has_features<MyFeatures, f_copy_construct>::value, "");
    static_assert(!has_features<MyFeatures, f_size, f_copy_construct>::value, "");
    static_assert(!has_features<MyFeatures, f_copy_construct, f_size>::value, "");
        //! [has_features example 2]
        }
    

        {
            std::aligned_storage<sizeof(std::string), alignof(std::string)>::type storage;
            void * storage_ptr = &storage;

            //! [f_default_construct example 1]
            auto const string_contruct = f_default_construct::make<std::string>();

            string_contruct(storage_ptr);
            //! [f_default_construct example 1]

            //! [f_destroy example 1]
            auto const string_destroy = f_default_construct::make<std::string>();

            string_destroy(storage_ptr);
            //! [f_destroy example 1]
        }
    }
    // clang-format on

    namespace
    {
        using namespace density;
        // clang-format off
        //! [feature_list example 6]
    template <bool CAN_COPY, bool CAN_MOVE>
    using ConditionalFeatures = feature_list<
        f_default_construct,
        typename std::conditional<CAN_COPY, f_copy_construct, f_none>::type,
        typename std::conditional<CAN_MOVE, f_move_construct, f_none>::type,
        f_destroy>;
        //! [feature_list example 6]
        // clang-format on
    } // namespace

    void runtime_type_examples()
    {
        using namespace density;
        {
            // clang-format off
            //! [runtime_type example 1]
    using RuntimeType_1 = runtime_type<f_size, f_alignment>;
    using RuntimeType_2 = runtime_type<f_size, f_none, f_size, f_alignment>;
    using RuntimeType_3 = runtime_type<feature_list<f_size, feature_list<f_none>>, f_alignment, f_alignment>;
    RuntimeType_1 a;
    RuntimeType_2 b = a;
    RuntimeType_3 c = b;
            //! [runtime_type example 1]
            // clang-format on
        }
        {
            // clang-format off
            //! [runtime_type example 3]
    // we just want to create, print and destroy objects
    using RuntimeType =
        runtime_type<f_size, f_alignment, f_ostream, f_default_construct, f_destroy>;

    // create a runtime type bound to std::complex<float> (the target type)
    auto const type = RuntimeType::make<std::complex<float>>();

    /* From now on, we can manage instances of the target type just using the runtime_type.
        Note that this is a kind of generic code different from C++ templates, because the 
        type is bound at runtime. It's also very low-level code, and it's not exception-safe. */

    // allocate and default construct an object
    void * const buff = aligned_allocate(type.size(), type.alignment());
    type.default_construct(buff); /* equivalent to get_feature<f_default_construct>()(buff). 
        default_construct is just a convenience function. */

    // now print the object std::cout
    type.get_feature<f_ostream>()(std::cout, buff); /* There is no convenience function to 
        do that, use get_feature. */

    /* destroy and deallocate. */
    type.destroy(buff);
    aligned_deallocate(buff, type.size(), type.alignment());
            //! [runtime_type example 3]
            // clang-format on
        }
        {
            // clang-format off
            //! [runtime_type tuple_type example 1]
    using RuntimeRype = runtime_type<f_size, feature_list<f_none, f_alignment> >;
    static_assert( std::is_same<
        RuntimeRype::tuple_type, 
        std::tuple<f_size, f_alignment>
    >::value, "");
            //! [runtime_type tuple_type example 1]
            // clang-format on
        }
#if !defined(__GNUC__) || defined(__clang__) /* gcc has some bugs in constexpr
			evaluation (m_feature_table == nullptr is not a constant expression) */
        {
            // clang-format off
            using T = int;
            using R = runtime_type<>;
            //! [runtime_type construct example 1]
    constexpr R r;
    static_assert(r.empty(), "");
    static_assert(!r.is<T>(), "");
            //! [runtime_type construct example 1]
            // clang-format on
        }
        {
            // clang-format off
            using T = int;
            using R = runtime_type<>;
            //! [runtime_type make example 1]
    constexpr auto r = R::make<T>();
    static_assert(!r.empty(), "");
    static_assert(r != R(), "");
    static_assert(r.is<T>(), "");
            //! [runtime_type make example 1]
            // clang-format on
        }
#endif
        {
            // clang-format off
            //! [runtime_type copy example 1]
    using Rt1 = runtime_type<f_size, f_alignment>;
    using Rt2 = runtime_type<feature_list<f_size>, f_none, f_alignment>;
    Rt1 t1    = Rt1::make<int>();
    Rt1 t2    = t1; // valid because Rt1 and Rt2 are equivalent
    assert(t1 == t2);

    using Rt3 =
        runtime_type<feature_list<f_size, f_default_construct>, f_none, f_alignment>;
    // Rt3 includes f_default_construct, so it's not equivalent to Rt1 and Rt2
    static_assert(!std::is_constructible<Rt1, Rt3>::value, "");
            //! [runtime_type copy example 1]
            // clang-format on
        }
        {
            // clang-format off
            //! [runtime_type assign example 1]
    using Rt1 = runtime_type<f_size, f_alignment>;
    using Rt2 = runtime_type<feature_list<f_size>, f_none, f_alignment>;
    Rt1 t1    = Rt1::make<int>();
    Rt1 t2;
    t2 = t1; // valid because Rt1 and Rt2 are equivalent
    assert(t1 == t2);

    using Rt3 =
        runtime_type<feature_list<f_size, f_default_construct>, f_none, f_alignment>;
    // Rt3 includes f_default_construct, so it's not equivalent to Rt1 and Rt2
    static_assert(!std::is_assignable<Rt1, Rt3>::value, "");
            //! [runtime_type assign example 1]
            // clang-format on
        }
    }

    //! [runtime_type example 2]

    /* This feature calls an update function on any object. The update has not to be virtual, as
        type erasure already is a kind of virtualization. */
    struct feature_call_update
    {
        void (*m_func)(void * i_object, float i_elapsed_time);

        void operator()(void * i_object, float i_elapsed_time) const
        {
            m_func(i_object, i_elapsed_time);
        }

        /** Creates an instance of this feature bound to the specified target type */
        template <typename TARGET_TYPE> constexpr static feature_call_update make() noexcept
        {
            return feature_call_update{&invoke<TARGET_TYPE>};
        }

      private:
        template <typename TARGET_TYPE>
        static void invoke(void * i_object, float i_elapsed_time) noexcept
        {
            static_cast<TARGET_TYPE *>(i_object)->update(i_elapsed_time);
        }
    };
    //! [runtime_type example 2]

} // namespace density_tests
