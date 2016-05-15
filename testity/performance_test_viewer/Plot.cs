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
    public partial class Plot : UserControl
    {
        public Plot()
        {
            InitializeComponent();
            DoubleBuffered = true;
        }

        private TestGroup m_group;
        private double m_min_x, m_max_x;
        private double m_min_y, m_max_y;

        public TestGroup Group
        {
            get { return m_group; }
            set
            {
                m_group = value;
                Invalidate();

                m_min_x = 0;
                m_max_x = m_group.MaxCardinaity;
                m_min_y = m_group.MinTime;
                m_max_y = m_group.MaxTime;
            }
        }
        protected override void OnResize(EventArgs e)
        {
            base.OnResize(e);
            Invalidate();
        }

        private PointF GetPoint( long x, long y)
        {
            double rx = (x - m_min_x) / (m_max_x - m_min_x);
            double ry = 1.0 - (y - m_min_y) / (m_max_y - m_min_y);
            return new PointF( (float)(rx * ClientSize.Width), (float)(ry * ClientSize.Height) );
        }

        float m_text_pos = 0.0f;

        private void PrintString(PaintEventArgs e, string i_string)
        {
            e.Graphics.DrawString("\t" + i_string, Font, Brushes.White, 0.0f, m_text_pos);
            m_text_pos += e.Graphics.MeasureString(i_string, Font).Height;
        }
        
        protected override void OnPaint(PaintEventArgs e)
        {
            base.OnPaint(e);

            m_text_pos = 10.0f;

            if (m_group == null)
                return;

            float clientHeight = ClientSize.Height;
            Pen pen = new Pen(Color.FromArgb(20, 20, 20));
            pen.DashStyle = System.Drawing.Drawing2D.DashStyle.Dash;
            for (int row = 0; row < m_group.get_cardinality_num(); row++)
            {
                long cardinality = m_group.get_cardinality(row);
                PointF point = GetPoint(cardinality, 0);
                e.Graphics.DrawLine(pen, point.X, 0, point.X, clientHeight);
            }


            PrintString(e, "compiler: " + m_group.compiler);
            PrintString(e, "os:" + m_group.operating_sytem);
            PrintString(e, "sys:" + m_group.sytem_info);
            PrintString(e, "sizeof pointer: " + m_group.sizeof_pointer);
            PrintString(e, "i_cardinality = " + m_group.get_cardinality(0) + ".." + m_group.get_cardinality(m_group.get_cardinality_num() - 1));
            PrintString(e, "sampling: " + m_group.multepicity.ToString() + "x" );
            PrintString(e, "date-time: " + m_group.date_time);

            for (int mult = 0; mult < m_group.multepicity; mult++)
            {
                foreach (Test test in m_group.Tests)
                {
                    PointF prev_point = new PointF(0, 0);
                    bool is_first_point = true;
                    for (int row = 0; row < m_group.get_cardinality_num(); row++)
                    {
                        long x = m_group.get_cardinality(row);
                        long y = test.get_values(row)[mult];

                        PointF new_point = GetPoint(x,y);

                        if(is_first_point)
                        {
                            is_first_point = false;
                        }
                        else
                        {
                            e.Graphics.DrawLine(test.pen, prev_point, new_point);
                        }
                        prev_point = new_point; 
                    }
                }
            }
        }
    }
}
