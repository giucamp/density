
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Data;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace performance_test_viewer
{
    public partial class TestGroupView : UserControl
    {
        private Plot m_plot;
        private TestGroup m_group;
        private bool m_sort_legend_by_avg = true;

        public TestGroupView()
        {
            InitializeComponent();
            m_plot = new Plot();
            m_plot.Dock = DockStyle.Fill;
            plotPanel.Controls.Add(m_plot);
        }

        protected override void OnControlAdded(ControlEventArgs e)
        {
            base.OnControlAdded(e);

            if (legendPanel.Controls.Count > 0)
            {
                splitContainer1.SplitterDistance = ClientSize.Width - legendPanel.Controls[0].Width;
            }
        }

        protected override void OnResize(EventArgs e)
        {
            base.OnResize(e);

            if (legendPanel.Controls.Count > 0)
            {
                splitContainer1.SplitterDistance = ClientSize.Width - legendPanel.Controls[0].Width;
            }
        }

        public TestGroup Group
        {
            get { return m_group; }
            set
            {
                legendPanel.Controls.Clear();

                m_group = value;
                m_plot.Group = m_group;

                lblTimeMin.Text = "0";
                lblTimeMax.Text = (m_group.MaxTime / 1000.0).ToString();

                lblCardMin.Text = m_group.MinCardinaity.ToString();
                lblCardMax.Text = m_group.MaxCardinaity.ToString();

                IEnumerable<Test> tests = m_group.Tests;
                if(m_sort_legend_by_avg)
                {
                    var sorted_tests = new List<Test>(tests);
                    sorted_tests.Sort((first, second) => { return -first.TimeAvgPercentage.CompareTo( second.TimeAvgPercentage); });
                    tests = sorted_tests;
                }
                foreach ( Test test in tests)
                {
                    LegendItem item = new LegendItem();
                    item.Color = test.color;
                    item.SourceCode = test.source_code;
                    item.Percentage = test.PercentageStr;
                    legendPanel.Controls.Add(item);
                }

                if (legendPanel.Controls.Count > 0)
                {
                    splitContainer1.SplitterDistance = ClientSize.Width - legendPanel.Controls[0].Width;
                }
            }
        }
    }
}
