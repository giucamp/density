
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)


#include "../density/runtime_type.h"
#include "../density/heterogeneous_array.h"
#include <string>
#include <iostream>

namespace misc_samples
{
    //! [runtime_type example 2]

    /* This feature calls an update function on any object. The update has not to be virtual, as
        type erasure is a kind of virtualization. */
    struct feature_call_update
    {
        using type = void(*)(void * i_dest, float i_elapsed_time);

        template <typename BASE, typename TYPE> struct Impl
        {
            static void invoke(void * i_base_dest, float i_elapsed_time) noexcept
            {
                const auto base_dest = static_cast<BASE*>(i_base_dest);
                static_cast<TYPE*>(base_dest)->update(i_elapsed_time);
            }
            static const uintptr_t value;
        };
    };
    template <typename TYPE, typename BASE>
        const uintptr_t feature_call_update::Impl<TYPE, BASE>::value = reinterpret_cast<uintptr_t>(invoke);

    //! [runtime_type example 2]

    void run()
    {
        {
            //! [feature_list example 1]
    using namespace density;
    using namespace type_features;
    using MyFeatures = feature_list<default_construct, size, alignment>;
            //! [feature_list example 1]
            MyFeatures unused; (void)unused;
        }

        {
            //! [feature_concat example 1]
    using namespace density;
    using namespace type_features;
    using MyPartialFeatures = feature_list<default_construct, size>;
    using MyFeatures = feature_concat_t<MyPartialFeatures, alignment>;
    using MyFeatures1 = feature_concat_t<MyPartialFeatures, feature_list<alignment> >;
    static_assert(std::is_same<MyFeatures, MyFeatures1>::value, "");
            //! [feature_concat example 1]
        }

        {
            //! [type_features::invoke example 1]

    using namespace density;
    using namespace type_features;

    // This feature allows to call a function object passing a std::string
    using MyInvoke = invoke<void(std::string)>;

    // since we are going to declare a type-erased container, we must include the default features (size, alignment, ...)
    using MyFeatures = feature_concat_t<default_type_features_t<void>, MyInvoke >;

    // alias for a specific heterogeneous_array. Until now we have just declared types
    using ArrayOfInvokables = heterogeneous_array<void, void_page_allocator, runtime_type<void, MyFeatures> >;

    // instances an array with a lambda expression as only element
    auto my_array = ArrayOfInvokables::make([](std::string s) { std::cout << s << std::endl; });

    // invoke my_array.begin()
    my_array.begin().complete_type().get_feature<MyInvoke>()(my_array.begin().element(), "hello!");
            //! [type_features::invoke example 1]
        }

        {
            //! [runtime_type example 1]

    using namespace density;
    using namespace type_features;

    using MyRTType = runtime_type<void, feature_list<default_construct, destroy, size> >;

    MyRTType type = MyRTType::make<std::string>();

    void * buff = malloc(type.size());

    type.default_construct(buff);

    // now buff points to a valid std::string
    *static_cast<std::string*>(buff) = "hello world!";

    type.destroy(buff);

    free(buff);

            //! [runtime_type example 1]
        }


        {
            //! [runtime_type example 3]

    struct ObjectA
    {
        void update(float i_elapsed_time)
            { std::cout << "ObjectA::update(" << i_elapsed_time << ")" << std::endl; }
    };

    struct ObjectB
    {
        void update(float i_elapsed_time)
            { std::cout << "ObjectB::update(" << i_elapsed_time << ")" << std::endl; }
    };

    using namespace density;
    using namespace type_features;

    // concatenates feature_call_update to the default features
    using MyFeatures = feature_concat_t<default_type_features_t<void>, feature_call_update>;

    // create an array with 4 objects
    auto my_array = heterogeneous_array<void, void_page_allocator, runtime_type<void, MyFeatures> >::make(
        ObjectA(), ObjectB(), ObjectA(), ObjectB() );

    // call update on all the objects
    auto const end_it = my_array.end();
    for (auto it = my_array.begin(); it != end_it; ++it)
    {
        auto const update_func = it.complete_type().get_feature<feature_call_update>();
        update_func(it.element(), 1.f / 60.f );
    }

            //! [runtime_type example 3]
        }
    }
} // namespace misc_samples

