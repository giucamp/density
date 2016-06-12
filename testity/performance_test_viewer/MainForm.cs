
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
    public partial class btnSaveScreenshot : Form
    {
        TestGroupView m_plot;
        List<TestGroup> test_groups = new List<TestGroup>();

        public btnSaveScreenshot()
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
                test_groups.Clear();
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

                m_lastPath = Path.GetDirectoryName(txtFile.Text);
            }
            catch (Exception)
            {
                panel.Visible = false;
            }
        }

        SaveFileDialog m_fileDiag = new SaveFileDialog();
        string m_lastPath = "";

        private void button1_Click(object sender, EventArgs e)
        {
            m_fileDiag.Filter = "JPeg Image|*.jpg|Bitmap Image|*.bmp|Gif Image|*.gif";
            m_fileDiag.FilterIndex = 2;

            try
            {
                char[] name = m_plot.Group.test_name.ToCharArray();
                for (int i = 0; i < name.Length; i++)
                {
                    if( !char.IsLetterOrDigit(name[i]) )
                        name[i] = '_';
                }
                m_fileDiag.FileName = new string(name);
                m_fileDiag.InitialDirectory = m_lastPath;
            }
            catch(Exception)
            {

            }

            var result = m_fileDiag.ShowDialog(this);            
            if (result == DialogResult.OK)
            {
                m_lastPath = Path.GetDirectoryName(m_fileDiag.FileName);
                System.Threading.Thread.Sleep(800);
                System.Drawing.Imaging.ImageFormat format = System.Drawing.Imaging.ImageFormat.Gif;
                switch (m_fileDiag.FilterIndex)
                {
                    case 1:
                        format = System.Drawing.Imaging.ImageFormat.Jpeg;
                        break;

                    case 2:
                        format = System.Drawing.Imaging.ImageFormat.Bmp;
                        break;

                    case 3:
                        format = System.Drawing.Imaging.ImageFormat.Gif;
                        break;
                }

                m_plot.SaveScreenshot(m_fileDiag.FileName, format);
            }            
        }
    }
}
