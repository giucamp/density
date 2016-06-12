
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

namespace performance_test_viewer
{
    partial class btnSaveScreenshot
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.panel = new System.Windows.Forms.Panel();
            this.comboTestGroup = new System.Windows.Forms.ComboBox();
            this.txtFile = new System.Windows.Forms.TextBox();
            this.label1 = new System.Windows.Forms.Label();
            this.label2 = new System.Windows.Forms.Label();
            this.button1 = new System.Windows.Forms.Button();
            this.SuspendLayout();
            // 
            // panel
            // 
            this.panel.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.panel.Location = new System.Drawing.Point(12, 57);
            this.panel.Name = "panel";
            this.panel.Size = new System.Drawing.Size(992, 656);
            this.panel.TabIndex = 0;
            // 
            // comboTestGroup
            // 
            this.comboTestGroup.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.comboTestGroup.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.comboTestGroup.FormattingEnabled = true;
            this.comboTestGroup.Location = new System.Drawing.Point(435, 30);
            this.comboTestGroup.Name = "comboTestGroup";
            this.comboTestGroup.Size = new System.Drawing.Size(442, 21);
            this.comboTestGroup.TabIndex = 1;
            this.comboTestGroup.SelectedIndexChanged += new System.EventHandler(this.comboTestGroup_SelectedIndexChanged);
            // 
            // txtFile
            // 
            this.txtFile.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.txtFile.AutoCompleteMode = System.Windows.Forms.AutoCompleteMode.SuggestAppend;
            this.txtFile.AutoCompleteSource = System.Windows.Forms.AutoCompleteSource.FileSystem;
            this.txtFile.Location = new System.Drawing.Point(12, 31);
            this.txtFile.Name = "txtFile";
            this.txtFile.Size = new System.Drawing.Size(399, 20);
            this.txtFile.TabIndex = 2;
            this.txtFile.TextChanged += new System.EventHandler(this.txtFile_TextChanged);
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(15, 11);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(139, 13);
            this.label1.TabIndex = 3;
            this.label1.Text = "Performance test output file:";
            // 
            // label2
            // 
            this.label2.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.label2.AutoSize = true;
            this.label2.Location = new System.Drawing.Point(432, 11);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(79, 13);
            this.label2.TabIndex = 4;
            this.label2.Text = "Testity - Viewer";
            // 
            // button1
            // 
            this.button1.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.button1.Location = new System.Drawing.Point(883, 13);
            this.button1.Name = "button1";
            this.button1.Size = new System.Drawing.Size(110, 38);
            this.button1.TabIndex = 5;
            this.button1.Text = "Save &Screenshot";
            this.button1.UseVisualStyleBackColor = true;
            this.button1.Click += new System.EventHandler(this.button1_Click);
            // 
            // btnSaveScreenshot
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(1007, 718);
            this.Controls.Add(this.button1);
            this.Controls.Add(this.label2);
            this.Controls.Add(this.label1);
            this.Controls.Add(this.txtFile);
            this.Controls.Add(this.comboTestGroup);
            this.Controls.Add(this.panel);
            this.Name = "btnSaveScreenshot";
            this.Text = "Testity - Viewer";
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Panel panel;
        private System.Windows.Forms.ComboBox comboTestGroup;
        private System.Windows.Forms.TextBox txtFile;
        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.Button button1;
    }
}

