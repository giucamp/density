namespace performance_test_viewer
{
    partial class PerformanceTestViewer
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
            this.comboTestGroup.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.comboTestGroup.FormattingEnabled = true;
            this.comboTestGroup.Location = new System.Drawing.Point(472, 30);
            this.comboTestGroup.Name = "comboTestGroup";
            this.comboTestGroup.Size = new System.Drawing.Size(319, 21);
            this.comboTestGroup.TabIndex = 1;
            this.comboTestGroup.SelectedIndexChanged += new System.EventHandler(this.comboTestGroup_SelectedIndexChanged);
            // 
            // txtFile
            // 
            this.txtFile.Location = new System.Drawing.Point(12, 31);
            this.txtFile.Name = "txtFile";
            this.txtFile.Size = new System.Drawing.Size(399, 20);
            this.txtFile.TabIndex = 2;
            this.txtFile.TextChanged += new System.EventHandler(this.txtFile_TextChanged);
            // 
            // PerformanceTestViewer
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(1007, 718);
            this.Controls.Add(this.txtFile);
            this.Controls.Add(this.comboTestGroup);
            this.Controls.Add(this.panel);
            this.Name = "PerformanceTestViewer";
            this.Text = "PerformanceTestViewer";
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Panel panel;
        private System.Windows.Forms.ComboBox comboTestGroup;
        private System.Windows.Forms.TextBox txtFile;
    }
}

