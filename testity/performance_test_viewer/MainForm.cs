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
        TestGroup test_group;

        public PerformanceTestViewer()
        {
            InitializeComponent();

            m_plot = new TestGroupView();
            m_plot.Dock = DockStyle.Fill;

            string[] rows = File.ReadAllLines("D:/SmartGit/density/density_tests/vs2015/perf.txt");
            int row_index = 0;
            test_group = new TestGroup(rows, ref row_index);

            Controls.Add(m_plot);
            m_plot.Group = test_group;


        }
    }
}
