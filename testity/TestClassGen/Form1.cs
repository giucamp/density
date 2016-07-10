using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace TestClassGen
{
    public partial class Form1 : Form
    {
        public Form1()
        {
            InitializeComponent();

            StringBuilder code = new StringBuilder();
            StringBuilder test = new StringBuilder();

            for( int i1 = 0; i1 < 3; i1++ )
            {
                for (int i2 = 0; i2 < 3; i2++)
                {
                    for (int i3 = 0; i3 < 3; i3++)
                    {
                        BuilClass(code, test, KindFromInt(i1), KindFromInt(i2), KindFromInt(i3));
                    }
                }
            }

            //BuilClass(code, test, Kind.Supported, Kind.Supported, Kind.Supported);

            textBox1.Text = code.ToString();
            textBox2.Text = test.ToString();
        }

        enum Kind
        {
            Deleted,
            Supported,
            SupportedNoExcept
        }

        Kind KindFromInt(int i)
        {
            switch(i)
            {
                case 0: return Kind.Deleted;
                case 1: return Kind.Supported;
                case 2: return Kind.SupportedNoExcept;
                default: throw new Exception();
            }
        }

        string KindToStr(Kind i_kind)
        {
            return "FeatureKind::" + i_kind.ToString();
        }

        private void BuildTest(StringBuilder i_test, string i_className, string i_trait, bool i_value)
        {
            i_test.AppendLine("static_assert( std::" + i_trait + "<" + i_className + ">::value == " + i_value.ToString().ToLowerInvariant() + ", \""
                + i_trait + " should be " + i_value.ToString().ToLowerInvariant() + "\");");
        }

        private void BuilClass(StringBuilder i_out, StringBuilder i_test, Kind i_defaultConstructor, Kind i_copy, Kind i_move)
        {
            i_out.AppendLine("template <size_t SIZE, size_t ALIGNMENT>");
            i_out.AppendLine("\tclass alignas(ALIGNMENT) TestClass<" +
                KindToStr(i_defaultConstructor) + ", " +
                KindToStr(i_copy) + ", " +
                KindToStr(i_move) +
                ", SIZE, ALIGNMENT> : public detail::RandomStorage<SIZE>");
            i_out.AppendLine("{");
            i_out.AppendLine("public:");

            i_out.AppendLine("\t// default constructor");
            switch (i_defaultConstructor)
            {
                case Kind.Deleted:
                    i_out.AppendLine("\tTestClass() = delete;");
                    break;
                case Kind.Supported:
                    i_out.AppendLine("\tTestClass() { exception_check_point(); }");
                    break;

                case Kind.SupportedNoExcept:
                    i_out.AppendLine("\tTestClass() noexcept = default;");
                    break;
            }

            i_out.AppendLine("");
            i_out.AppendLine("\t// constructor with int seed");
            i_out.AppendLine("\tTestClass(int i_seed) : RandomStorage<SIZE>((exception_check_point(), i_seed)) { }");

            i_out.AppendLine("");
            i_out.AppendLine("\t// copy");
            switch (i_copy)
            {
                case Kind.Deleted:
                    i_out.AppendLine("\tTestClass(const TestClass &) = delete;");
                    i_out.AppendLine("\tTestClass & operator = (const TestClass &) = delete;");
                    break;
                case Kind.Supported:
                    i_out.AppendLine("\tTestClass(const TestClass & i_source)");
                    i_out.AppendLine("\t\t: RandomStorage<SIZE>((exception_check_point(), i_source)) { }");
                    i_out.AppendLine("\tconst TestClass & operator = (const TestClass & i_source)");
                    i_out.AppendLine("\t\t{ exception_check_point(); RandomStorage<SIZE>::operator = (i_source); return *this; }");
                    break;

                case Kind.SupportedNoExcept:
                    i_out.AppendLine("\tTestClass(const TestClass &) noexcept = default;");
                    i_out.AppendLine("\tTestClass & operator = (const TestClass &) noexcept = default;");
                    break;
            }

            i_out.AppendLine("");
            i_out.AppendLine("\t// move");
            switch (i_move)
            {
                case Kind.Deleted:
                    i_out.AppendLine("\tTestClass(TestClass &&) = delete;");
                    i_out.AppendLine("\tTestClass & operator = (TestClass &&) = delete;");
                    break;
                case Kind.Supported:
                    i_out.AppendLine("\tTestClass(TestClass && i_source)");
                    i_out.AppendLine("\t\t: RandomStorage<SIZE>((exception_check_point(), std::move(i_source))) { exception_check_point(); }");
                    i_out.AppendLine("\tconst TestClass & operator = (TestClass && i_source)");
                    i_out.AppendLine("\t\t{ exception_check_point(); RandomStorage<SIZE>::operator = (std::move(i_source)); return *this; }");
                    break;

                case Kind.SupportedNoExcept:
                    i_out.AppendLine("\tTestClass(TestClass &&) noexcept = default;");
                    i_out.AppendLine("\tTestClass & operator = (TestClass &&) noexcept = default;");
                    break;
            }

            i_out.AppendLine("};");
            i_out.AppendLine("");


            string className = "TestClass<" + KindToStr(i_defaultConstructor) + ", " +
                KindToStr(i_copy) + ", " + KindToStr(i_move) + ">";
            i_test.AppendLine("");
            i_test.AppendLine("// test " + className);
            BuildTest(i_test, className, "is_default_constructible", i_defaultConstructor != Kind.Deleted);
            BuildTest(i_test, className, "is_nothrow_default_constructible", i_defaultConstructor == Kind.SupportedNoExcept);

            BuildTest(i_test, className, "is_copy_constructible", i_copy != Kind.Deleted);
            BuildTest(i_test, className, "is_nothrow_copy_constructible", i_copy == Kind.SupportedNoExcept);
            BuildTest(i_test, className, "is_copy_assignable", i_copy != Kind.Deleted);
            BuildTest(i_test, className, "is_nothrow_copy_assignable", i_copy == Kind.SupportedNoExcept);

            BuildTest(i_test, className, "is_move_constructible", i_move != Kind.Deleted);
            BuildTest(i_test, className, "is_nothrow_move_constructible", i_move == Kind.SupportedNoExcept);
            BuildTest(i_test, className, "is_move_assignable", i_move != Kind.Deleted);
            BuildTest(i_test, className, "is_nothrow_move_assignable", i_move == Kind.SupportedNoExcept);
        }
    }
}
