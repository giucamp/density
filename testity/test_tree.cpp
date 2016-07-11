
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "test_tree.h"
#include <string.h>
#include <random>
#include <algorithm>

namespace testity
{
    namespace detail
    {
        template <typename PREEDICATE>
            static void for_each_token(const char * i_path, PREEDICATE && i_predicate)
        {
            const char * remaining_path = i_path;
            for(;;)
            {
                const size_t token_length = strcspn(remaining_path, "/\\");
                i_predicate(remaining_path, token_length);
                remaining_path += token_length;
                if (*remaining_path == 0)
                    break;
                remaining_path++;
            }
        }
    }

	void TestTree::add_functionality_test(std::function< void(FunctionalityContext & i_context) > i_function)
	{
		m_functionality_tests.emplace_back(std::unique_ptr<detail::IFunctionalityTest>(
			new detail::NoTargetFunctionalityTest(i_function)));
	}

	void TestTree::add_child(TestTree i_child)
	{
		if (find(i_child.name().c_str()) != nullptr)
		{
			throw std::invalid_argument("duplicate child in TestTree");
		}

		m_children.push_back(std::move(i_child));
	}

    TestTree & TestTree::operator [] (const char * i_path)
    {
        using namespace std;

        auto node = this;
		detail::for_each_token(i_path, [&node](const char * i_token, size_t i_token_length) {

            auto entry_it = find_if(node->m_children.begin(), node->m_children.end(),
                [i_token, i_token_length](const TestTree & i_entry) { return i_entry.name().compare(0, string::npos, i_token, i_token_length) == 0; });

            if (entry_it != node->m_children.end())
            {
                node = &*entry_it;
            }
            else
            {
                node->m_children.emplace_back(TestTree(string(i_token, i_token_length)));
                node = &node->m_children.back();
            }
        });

        return *node;
    }

    const TestTree * TestTree::find(const char * i_path) const
    {
        using namespace std;

        auto node = this;
		detail::for_each_token(i_path, [&node](const char * i_token, size_t i_token_length) {

            if (node != nullptr)
            {
                auto entry_it = find_if(node->m_children.begin(), node->m_children.end(),
                    [i_token, i_token_length](const TestTree & i_entry) { return i_entry.name().compare(0, string::npos, i_token, i_token_length); });

                if (entry_it != node->m_children.end())
                {
                    node = &*entry_it;
                }
                else
                {
                    node = nullptr;
                }
            }
        });

        return node;
    }

    TestTree * TestTree::find(const char * i_path)
    {
        using namespace std;

        auto node = this;
        detail::for_each_token(i_path, [&node](const char * i_token, size_t i_token_length) {

            if (node != nullptr)
            {
                auto entry_it = find_if(node->m_children.begin(), node->m_children.end(),
                    [i_token, i_token_length](const TestTree & i_entry) { return i_entry.name().compare(0, string::npos, i_token, i_token_length); });

                if (entry_it != node->m_children.end())
                {
                    node = &*entry_it;
                }
                else
                {
                    node = nullptr;
                }
            }
        });

        return node;
    }

 

} // namespace testity
