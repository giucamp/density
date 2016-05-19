
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Drawing;

namespace performance_test_viewer
{
    public class Test
    {
        public Test(Color i_color, string i_source_code, long i_cardinality, int i_multeplicity)
        {
            m_color = i_color;
            m_pen = new Pen(m_color);
            m_source_code = i_source_code;
            m_values = new long[i_cardinality][];
            for(long i = 0; i < i_cardinality; i++)
            {
                m_values[i] = new long[i_multeplicity];
            }
        }

        public string source_code
        {
            get { return m_source_code; }
        }

        public long[] get_values(int i_index)
        {
            return m_values[i_index];
        }

        private static char[] s_separators = new char[] { ',' };

        public void set_values(long i_index, string i_values)
        {
            int index = 0;
            foreach ( string val in i_values.Split(s_separators, StringSplitOptions.RemoveEmptyEntries) )
            {
                m_values[i_index][index] = long.Parse(val);
                index++;
            }
        }

        public Color color
        {
            get { return m_color; }
        }

        public Pen pen
        {
            get { return m_pen; }
        }

        public double TimeAvgPercentage
        {
            get { return m_time_avg_percentage; }
            set { m_time_avg_percentage = value; }
        }

        public string PercentageStr
        {
            get { return (m_time_avg_percentage * 100.0).ToString("f2") + "% avg time"; }
        }

        public double get_average()
        {
            // http://stackoverflow.com/questions/1930454/what-is-a-good-solution-for-calculating-an-average-where-the-sum-of-all-values-e
            double avg = 0;
            double total = 1;
            for(int i = 0; i < m_values.Length; i++)
            {
                for (int j = 0; j < m_values[i].Length; j++)
                {
                    avg += (m_values[i][j] - avg) / total;
                    total += 1.0;
                }
            }
            return avg;
        }

        private string m_source_code;
        private long[][] m_values;
        private Color m_color;
        private Pen m_pen;
        private double m_time_avg_percentage;
    }

    public class TestGroup
    {
        private static Color[] s_colors = new Color[] {
            Color.Red, Color.Green, Color.Blue, Color.Magenta, Color.Yellow};

        private string m_test_path;
        private string m_test_name;
        private string m_version_label;
        private string m_compiler;
        private string m_operating_sytem;
        private string m_sytem_info;
        private string m_date_time;
        private int m_multepicity;
        private int m_sizeof_pointer;
        private string m_random_shuffle;
        private bool m_deterministic;
        private long m_cardinality_start, m_cardinality_step, m_cardinality_end;
        private List<Test> m_tests = new List<Test>();
        private ReadOnlyCollection<Test> m_readony_tests;
        private long[] m_cardinality;

        public override string ToString()
        {
            return m_test_path + " - " + m_version_label + " - " + m_test_name + ", " + m_date_time;
        }

        public long get_cardinality(int i_index)
        {
            return m_cardinality[i_index];
        }

        public int get_cardinality_num()
        {
            return m_cardinality.Length;
        }

        public int multepicity
        {
            get { return m_multepicity; }
        }

        public int sizeof_pointer
        {
            get { return m_sizeof_pointer; }
        }

        public void set_cardinality(long i_index, long i_cardinality)
        {
            m_cardinality[i_index] = i_cardinality;
        }

        public ReadOnlyCollection<Test> Tests
        {
            get { return m_readony_tests; }
        }

        private long compute_cardinality()
        {
            long result = 0;
            for( var c = m_cardinality_start; c < m_cardinality_end; c += m_cardinality_step)
            {
                result++;
            }
            return result;
        }

        public string test_path { get { return m_test_path; } }
        public string version_label { get { return m_version_label; } }
        public string test_name { get { return m_test_name; } }
        public string compiler { get { return m_compiler; } }
        public string operating_sytem { get { return m_operating_sytem; } }
        public string sytem_info { get { return m_sytem_info; } }
        public string date_time { get { return m_date_time; } }
        public string random_shuffle { get { return m_random_shuffle; } }
        public bool deterministic { get { return m_deterministic; } }

        public long MinCardinaity
        {
            get
            {
                return m_cardinality[0];
            }
        }
        public long MaxCardinaity
        {
            get
            {
                return m_cardinality[m_cardinality.Length - 1];
            }
        }

        public long MinTime
        {
            get
            {
                long time = long.MaxValue;
                foreach(Test test in m_tests)
                {
                    for(int row = 0; row < m_cardinality.Length; row++)
                    {
                        var min = test.get_values(row).Min();
                        time = Math.Min(time, min);
                    }
                }
                return time;
            }
        }

        public long MaxTime
        {
            get
            {
                long time = long.MinValue;
                foreach (Test test in m_tests)
                {
                    for (int row = 0; row < m_cardinality.Length; row++)
                    {
                        var max = test.get_values(row).Max();
                        time = Math.Max(time, max);
                    }
                }
                return time;
            }
        }

        public TestGroup(string[] i_lines, ref int io_curr_line)
        {
            m_readony_tests = m_tests.AsReadOnly();
            char[] separators = new char[] { '\t' };

            while(i_lines[io_curr_line] == "-------------------------------------" || i_lines[io_curr_line].Trim() == "")
            {
                io_curr_line++;
            }
            long curr_row_index = 0;
            bool finished = false;
            while(!finished)
            {
                var curr_line = i_lines[io_curr_line];
                io_curr_line++;

                var col_pos = curr_line.IndexOf(':');
                string type = curr_line.Substring(0, col_pos);
                string value = curr_line.Substring(col_pos + 1);
                switch (type)
                {
                    case "PERFORMANCE_TEST_GROUP": m_test_path = value; break;
                    case "NAME": m_test_name = value; break;
                    case "VERSION_LABEL": m_version_label = value; break;
                    case "COMPILER": m_compiler = value; break;
                    case "OS": m_operating_sytem = value; break;
                    case "SYSTEM": m_sytem_info = value; break;
                    case "DATE_TIME": m_date_time = value; break;
                    case "CARDINALITY_START": m_cardinality_start = long.Parse(value); break;
                    case "CARDINALITY_STEP": m_cardinality_step = long.Parse(value); break;
                    case "CARDINALITY_END":
                        m_cardinality_end = long.Parse(value);
                        m_cardinality = new long[compute_cardinality()];
                        break;
                    case "MULTEPLICITY": m_multepicity = int.Parse(value); break;
                    case "SIZEOF_POINTER": m_sizeof_pointer = int.Parse(value); break;
                    case "DETERMINISTIC": m_deterministic = value.Trim().ToLowerInvariant() == "yes"; break;
                    case "RANDOM_SHUFFLE": m_random_shuffle = value; break;
                    case "TEST":
                        int color_index = m_tests.Count % s_colors.Length;
                        m_tests.Add( new Test(s_colors[color_index], value, compute_cardinality(), m_multepicity) );
                        break;
                    case "ROW":
                    {
                        var values = value.Split(separators, StringSplitOptions.RemoveEmptyEntries);
                        m_cardinality[curr_row_index] = long.Parse(values[0]);
                        for (int i = 0; i < m_tests.Count; i++ )
                        {
                           m_tests[i].set_values(curr_row_index, values[i+1]);
                        }
                        curr_row_index++;
                        break;
                    }
                    case "LEGEND_START": break;
                    case "LEGEND_END": break;
                    case "TABLE_START": break;
                    case "TABLE_END": break;
                    case "PERFORMANCE_TEST_GROUP_END":
                        finished = true;
                        break;
                }
            }

            double [] averages = new double[m_tests.Count];
            for (int i = 0; i < m_tests.Count; i++)
            {
                averages[i] = m_tests[i].get_average();
            }

            double max_avg = averages.Max();

            for (int i = 0; i < m_tests.Count; i++)
            {
                m_tests[i].TimeAvgPercentage = averages[i] / max_avg;
            }
        }

    }
}
