
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

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
            this.lblPercentage = new System.Windows.Forms.Label();
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
            this.txtSourceCode.BorderStyle = System.Windows.Forms.BorderStyle.None;
            this.txtSourceCode.Font = new System.Drawing.Font("Courier New", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.txtSourceCode.Location = new System.Drawing.Point(11, 38);
            this.txtSourceCode.Name = "txtSourceCode";
            this.txtSourceCode.ReadOnly = true;
            this.txtSourceCode.Size = new System.Drawing.Size(467, 111);
            this.txtSourceCode.TabIndex = 1;
            this.txtSourceCode.Text = "";
            // 
            // lblPercentage
            // 
            this.lblPercentage.Font = new System.Drawing.Font("Microsoft Sans Serif", 12F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.lblPercentage.Location = new System.Drawing.Point(42, 12);
            this.lblPercentage.Name = "lblPercentage";
            this.lblPercentage.Size = new System.Drawing.Size(190, 27);
            this.lblPercentage.TabIndex = 2;
            this.lblPercentage.Text = "label1";
            // 
            // LegendItem
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.Controls.Add(this.lblPercentage);
            this.Controls.Add(this.txtSourceCode);
            this.Controls.Add(this.lblColorBox);
            this.Name = "LegendItem";
            this.Size = new System.Drawing.Size(490, 152);
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.Label lblColorBox;
        private System.Windows.Forms.RichTextBox txtSourceCode;
        private System.Windows.Forms.Label lblPercentage;
    }
}
