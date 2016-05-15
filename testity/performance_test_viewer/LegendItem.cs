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
    public partial class LegendItem : UserControl
    {
        public LegendItem()
        {
            InitializeComponent();
        }        

        public Color Color
        {
            get { return lblColorBox.BackColor; }
            set { lblColorBox.BackColor = value; }
        }

        public string SourceCode
        {
            get { return txtSourceCode.Text; }
            set { txtSourceCode.Text = value; }
        }

        public string Percentage
        {
            get { return lblPercentage.Text; }
            set { lblPercentage.Text = value; }
        }
    }
}
