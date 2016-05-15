namespace performance_test_viewer
{
    partial class TestGroupView
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

        #region Component Designer generated code

        /// <summary> 
        /// Required method for Designer support - do not modify 
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.splitContainer1 = new System.Windows.Forms.SplitContainer();
            this.label2 = new System.Windows.Forms.Label();
            this.label1 = new System.Windows.Forms.Label();
            this.plotPanel = new System.Windows.Forms.Panel();
            this.lblCardMax = new System.Windows.Forms.Label();
            this.lblCardMin = new System.Windows.Forms.Label();
            this.lblTimeMin = new System.Windows.Forms.Label();
            this.lblTimeMax = new System.Windows.Forms.Label();
            this.legendPanel = new System.Windows.Forms.FlowLayoutPanel();
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer1)).BeginInit();
            this.splitContainer1.Panel1.SuspendLayout();
            this.splitContainer1.Panel2.SuspendLayout();
            this.splitContainer1.SuspendLayout();
            this.SuspendLayout();
            // 
            // splitContainer1
            // 
            this.splitContainer1.Dock = System.Windows.Forms.DockStyle.Fill;
            this.splitContainer1.Location = new System.Drawing.Point(0, 0);
            this.splitContainer1.Name = "splitContainer1";
            // 
            // splitContainer1.Panel1
            // 
            this.splitContainer1.Panel1.Controls.Add(this.label2);
            this.splitContainer1.Panel1.Controls.Add(this.label1);
            this.splitContainer1.Panel1.Controls.Add(this.plotPanel);
            this.splitContainer1.Panel1.Controls.Add(this.lblCardMax);
            this.splitContainer1.Panel1.Controls.Add(this.lblCardMin);
            this.splitContainer1.Panel1.Controls.Add(this.lblTimeMin);
            this.splitContainer1.Panel1.Controls.Add(this.lblTimeMax);
            // 
            // splitContainer1.Panel2
            // 
            this.splitContainer1.Panel2.Controls.Add(this.legendPanel);
            this.splitContainer1.Size = new System.Drawing.Size(549, 443);
            this.splitContainer1.SplitterDistance = 452;
            this.splitContainer1.TabIndex = 0;
            // 
            // label2
            // 
            this.label2.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.label2.AutoSize = true;
            this.label2.Location = new System.Drawing.Point(224, 412);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(62, 13);
            this.label2.TabIndex = 6;
            this.label2.Text = "i_cardinality";
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(16, 183);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(50, 39);
            this.label1.TabIndex = 5;
            this.label1.Text = "Time\r\n(micro-\r\nseconds)";
            this.label1.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            // 
            // plotPanel
            // 
            this.plotPanel.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.plotPanel.Location = new System.Drawing.Point(72, 17);
            this.plotPanel.Name = "plotPanel";
            this.plotPanel.Size = new System.Drawing.Size(366, 392);
            this.plotPanel.TabIndex = 4;
            // 
            // lblCardMax
            // 
            this.lblCardMax.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.lblCardMax.Location = new System.Drawing.Point(348, 412);
            this.lblCardMax.Name = "lblCardMax";
            this.lblCardMax.Size = new System.Drawing.Size(91, 13);
            this.lblCardMax.TabIndex = 3;
            this.lblCardMax.Text = "CardTime";
            this.lblCardMax.TextAlign = System.Drawing.ContentAlignment.TopRight;
            // 
            // lblCardMin
            // 
            this.lblCardMin.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this.lblCardMin.Location = new System.Drawing.Point(74, 412);
            this.lblCardMin.Name = "lblCardMin";
            this.lblCardMin.Size = new System.Drawing.Size(77, 13);
            this.lblCardMin.TabIndex = 2;
            this.lblCardMin.Text = "CardTime";
            // 
            // lblTimeMin
            // 
            this.lblTimeMin.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this.lblTimeMin.Location = new System.Drawing.Point(17, 390);
            this.lblTimeMin.Name = "lblTimeMin";
            this.lblTimeMin.Size = new System.Drawing.Size(58, 19);
            this.lblTimeMin.TabIndex = 1;
            this.lblTimeMin.Text = "MinTime";
            this.lblTimeMin.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
            // 
            // lblTimeMax
            // 
            this.lblTimeMax.Location = new System.Drawing.Point(14, 16);
            this.lblTimeMax.Name = "lblTimeMax";
            this.lblTimeMax.Size = new System.Drawing.Size(61, 19);
            this.lblTimeMax.TabIndex = 0;
            this.lblTimeMax.Text = "MaxTime";
            this.lblTimeMax.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
            // 
            // legendPanel
            // 
            this.legendPanel.Dock = System.Windows.Forms.DockStyle.Fill;
            this.legendPanel.FlowDirection = System.Windows.Forms.FlowDirection.TopDown;
            this.legendPanel.Location = new System.Drawing.Point(0, 0);
            this.legendPanel.Name = "legendPanel";
            this.legendPanel.Size = new System.Drawing.Size(93, 443);
            this.legendPanel.TabIndex = 0;
            // 
            // TestGroupView
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.Controls.Add(this.splitContainer1);
            this.Name = "TestGroupView";
            this.Size = new System.Drawing.Size(549, 443);
            this.splitContainer1.Panel1.ResumeLayout(false);
            this.splitContainer1.Panel1.PerformLayout();
            this.splitContainer1.Panel2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer1)).EndInit();
            this.splitContainer1.ResumeLayout(false);
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.SplitContainer splitContainer1;
        private System.Windows.Forms.FlowLayoutPanel legendPanel;
        private System.Windows.Forms.Label lblCardMax;
        private System.Windows.Forms.Label lblCardMin;
        private System.Windows.Forms.Label lblTimeMin;
        private System.Windows.Forms.Label lblTimeMax;
        private System.Windows.Forms.Panel plotPanel;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.Label label1;
    }
}
