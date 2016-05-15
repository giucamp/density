
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.IO;

namespace performance_test_viewer
{
    public partial class PerformanceTestViewer : Form
    {
        TestGroupView m_plot;
        List<TestGroup> test_groups = new List<TestGroup>();

        public PerformanceTestViewer()
        {
            InitializeComponent();

            m_plot = new TestGroupView();
            m_plot.Dock = DockStyle.Fill;
            panel.Controls.Add(m_plot);

            string backs = "/..";
            int tries = 0;
            string final_part = "/density_tests/vs2015/perf.txt";
            while (tries < 6 && !File.Exists(System.Environment.CurrentDirectory + backs + final_part))
            {
                backs += "/..";
                tries++;
            }

            string file_name = Path.GetFullPath(System.Environment.CurrentDirectory + backs + final_part);
            txtFile.Text = file_name;
        }

        private void comboTestGroup_SelectedIndexChanged(object sender, EventArgs e)
        {
            m_plot.Group = (TestGroup)comboTestGroup.SelectedItem;
        }

        private void txtFile_TextChanged(object sender, EventArgs e)
        {
            try
            {
                comboTestGroup.Items.Clear();
                panel.Visible = true;
                string[] rows = File.ReadAllLines(txtFile.Text);
                int row_index = 0;

                for (;;)
                {
                    try
                    {
                        TestGroup group = new TestGroup(rows, ref row_index);
                        test_groups.Add(group);
                    }
                    catch (Exception)
                    {
                        break;
                    }
                }

                comboTestGroup.Items.AddRange(test_groups.ToArray());

                comboTestGroup.SelectedItem = test_groups[test_groups.Count - 1];
            }
            catch (Exception)
            {
                panel.Visible = false;
            }
        }
    }
}
