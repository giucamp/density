namespace performance_test_viewer
{
    partial class LegendItem
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
            this.lblColorBox = new System.Windows.Forms.Label();
            this.txtSourceCode = new System.Windows.Forms.RichTextBox();
            this.SuspendLayout();
            // 
            // lblColorBox
            // 
            this.lblColorBox.BackColor = System.Drawing.Color.Red;
            this.lblColorBox.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.lblColorBox.Location = new System.Drawing.Point(8, 8);
            this.lblColorBox.Name = "lblColorBox";
            this.lblColorBox.Size = new System.Drawing.Size(31, 24);
            this.lblColorBox.TabIndex = 0;
            // 
            // txtSourceCode
            // 
            this.txtSourceCode.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.txtSourceCode.Location = new System.Drawing.Point(45, 8);
            this.txtSourceCode.Name = "txtSourceCode";
            this.txtSourceCode.Size = new System.Drawing.Size(225, 124);
            this.txtSourceCode.TabIndex = 1;
            this.txtSourceCode.Text = "";
            // 
            // LegendItem
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.Controls.Add(this.txtSourceCode);
            this.Controls.Add(this.lblColorBox);
            this.Name = "LegendItem";
            this.Size = new System.Drawing.Size(282, 145);
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.Label lblColorBox;
        private System.Windows.Forms.RichTextBox txtSourceCode;
    }
}
