
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "../density/runtime_type.h"
#include "../density/lifo.h"
#include <iostream>
#include <string>

namespace runtime_type_sample
{
    using namespace density;

    struct Widget
    {
        virtual void draw() { /* ... */ }
        ~Widget() { std::cout << "Goodbye!" << std::endl; }
    };
    struct TextWidget : Widget
    {
        std::string text = "some text";
        virtual void draw() override { std::cout << "Hi there! I'm a TextWidget: " << text << std::endl; }
    };
    struct ImageWidget : Widget
    {
        float x = 39.f, y = 6.f, z = -3.f;
        virtual void draw() override { std::cout << "Hi there! I'm a ImageWidget: " << x + y + z << std::endl; }
    };

    using Features = type_features::feature_list<type_features::default_construct, type_features::destroy,
		type_features::size, type_features::alignment, type_features::rtti>;

    runtime_type<Widget, Features> select_widget_type()
    {
        for (;;)
        {
            char selection = 0;
            std::cout << "Type 't' to create a TextWidget, 'i' to create an ImageWidget, or 'q' to quit the program" << std::endl;
            std::cin >> selection;
            switch (selection)
            {
                case 't': return runtime_type<Widget, Features>::template make<TextWidget>();
                case 'i': return runtime_type<Widget, Features>::template make<ImageWidget>();
                case 'q': std::exit(0);
            }
        }
    }

    void run()
    {
        using namespace density;

        lifo_buffer<> buffer;
        for (;;)
        {
            // make the user choose a type
            auto widget_type = select_widget_type();

            // reserve the required space in the lifo_buffer
            buffer.resize(widget_type.size(), widget_type.alignment());

            // create the widget of the specified type
			Widget * widget = widget_type.default_construct(buffer.data());

            // draw it
            widget->draw();

            // destroy the widget
            widget_type.destroy(widget);

            // draw a line
            std::cout << "-------------\n" << std::endl;
        }
    }
}

