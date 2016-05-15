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

                foreach( Test test in m_group.Tests)
                {
                    LegendItem item = new LegendItem();
                    item.Color = test.color;
                    item.SourceCode = test.source_code;
                    item.Percentage = test.Percentage;
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
