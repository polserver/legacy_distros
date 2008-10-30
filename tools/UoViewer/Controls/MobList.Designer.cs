/***************************************************************************
 *
 * $Author: Turley
 * 
 * "THE BEER-WARE LICENSE"
 * As long as you retain this notice you can do whatever you want with 
 * this stuff. If we meet some day, and you think this stuff is worth it,
 * you can buy me a beer in return.
 *
 ***************************************************************************/

namespace Controls
{
    partial class MobList
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
            this.components = new System.ComponentModel.Container();
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(MobList));
            this.splitContainer1 = new System.Windows.Forms.SplitContainer();
            this.TreeViewMobs = new System.Windows.Forms.TreeView();
            this.FacingBar = new System.Windows.Forms.TrackBar();
            this.tabControl1 = new System.Windows.Forms.TabControl();
            this.tabPage1 = new System.Windows.Forms.TabPage();
            this.MainPictureBox = new System.Windows.Forms.PictureBox();
            this.contextMenuStrip1 = new System.Windows.Forms.ContextMenuStrip(this.components);
            this.extractImageToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.tabPage2 = new System.Windows.Forms.TabPage();
            this.listView = new System.Windows.Forms.ListView();
            this.tabPage3 = new System.Windows.Forms.TabPage();
            this.listView1 = new System.Windows.Forms.ListView();
            this.statusStrip1 = new System.Windows.Forms.StatusStrip();
            this.SettingsButton = new System.Windows.Forms.ToolStripSplitButton();
            this.sortAlphaToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.hueToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.animateToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.GraphicLabel = new System.Windows.Forms.ToolStripStatusLabel();
            this.BaseGraphicLabel = new System.Windows.Forms.ToolStripStatusLabel();
            this.HueLabel = new System.Windows.Forms.ToolStripStatusLabel();
            this.splitContainer1.Panel1.SuspendLayout();
            this.splitContainer1.Panel2.SuspendLayout();
            this.splitContainer1.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.FacingBar)).BeginInit();
            this.tabControl1.SuspendLayout();
            this.tabPage1.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.MainPictureBox)).BeginInit();
            this.contextMenuStrip1.SuspendLayout();
            this.tabPage2.SuspendLayout();
            this.tabPage3.SuspendLayout();
            this.statusStrip1.SuspendLayout();
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
            this.splitContainer1.Panel1.Controls.Add(this.TreeViewMobs);
            // 
            // splitContainer1.Panel2
            // 
            this.splitContainer1.Panel2.Controls.Add(this.FacingBar);
            this.splitContainer1.Panel2.Controls.Add(this.tabControl1);
            this.splitContainer1.Panel2.Controls.Add(this.statusStrip1);
            this.splitContainer1.Size = new System.Drawing.Size(619, 324);
            this.splitContainer1.SplitterDistance = 203;
            this.splitContainer1.TabIndex = 1;
            // 
            // TreeViewMobs
            // 
            this.TreeViewMobs.Dock = System.Windows.Forms.DockStyle.Fill;
            this.TreeViewMobs.HideSelection = false;
            this.TreeViewMobs.Location = new System.Drawing.Point(0, 0);
            this.TreeViewMobs.Name = "TreeViewMobs";
            this.TreeViewMobs.ShowNodeToolTips = true;
            this.TreeViewMobs.Size = new System.Drawing.Size(203, 324);
            this.TreeViewMobs.TabIndex = 0;
            this.TreeViewMobs.AfterSelect += new System.Windows.Forms.TreeViewEventHandler(this.TreeViewMobs_AfterSelect);
            // 
            // FacingBar
            // 
            this.FacingBar.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.FacingBar.AutoSize = false;
            this.FacingBar.LargeChange = 1;
            this.FacingBar.Location = new System.Drawing.Point(324, 304);
            this.FacingBar.Maximum = 7;
            this.FacingBar.Name = "FacingBar";
            this.FacingBar.Size = new System.Drawing.Size(88, 20);
            this.FacingBar.TabIndex = 2;
            this.FacingBar.Scroll += new System.EventHandler(this.OnScrollFacing);
            // 
            // tabControl1
            // 
            this.tabControl1.Controls.Add(this.tabPage1);
            this.tabControl1.Controls.Add(this.tabPage2);
            this.tabControl1.Controls.Add(this.tabPage3);
            this.tabControl1.Dock = System.Windows.Forms.DockStyle.Fill;
            this.tabControl1.Location = new System.Drawing.Point(0, 0);
            this.tabControl1.Name = "tabControl1";
            this.tabControl1.SelectedIndex = 0;
            this.tabControl1.Size = new System.Drawing.Size(412, 302);
            this.tabControl1.TabIndex = 1;
            // 
            // tabPage1
            // 
            this.tabPage1.Controls.Add(this.MainPictureBox);
            this.tabPage1.Location = new System.Drawing.Point(4, 22);
            this.tabPage1.Name = "tabPage1";
            this.tabPage1.Padding = new System.Windows.Forms.Padding(3);
            this.tabPage1.Size = new System.Drawing.Size(404, 276);
            this.tabPage1.TabIndex = 0;
            this.tabPage1.Text = "Animation";
            this.tabPage1.UseVisualStyleBackColor = true;
            // 
            // MainPictureBox
            // 
            this.MainPictureBox.BackColor = System.Drawing.Color.White;
            this.MainPictureBox.ContextMenuStrip = this.contextMenuStrip1;
            this.MainPictureBox.Dock = System.Windows.Forms.DockStyle.Fill;
            this.MainPictureBox.Location = new System.Drawing.Point(3, 3);
            this.MainPictureBox.Name = "MainPictureBox";
            this.MainPictureBox.Size = new System.Drawing.Size(398, 270);
            this.MainPictureBox.TabIndex = 0;
            this.MainPictureBox.TabStop = false;
            this.MainPictureBox.Paint += new System.Windows.Forms.PaintEventHandler(this.OnPaint_MainPicture);
            // 
            // contextMenuStrip1
            // 
            this.contextMenuStrip1.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.extractImageToolStripMenuItem});
            this.contextMenuStrip1.Name = "contextMenuStrip1";
            this.contextMenuStrip1.Size = new System.Drawing.Size(149, 26);
            // 
            // extractImageToolStripMenuItem
            // 
            this.extractImageToolStripMenuItem.Name = "extractImageToolStripMenuItem";
            this.extractImageToolStripMenuItem.Size = new System.Drawing.Size(148, 22);
            this.extractImageToolStripMenuItem.Text = "extract Image";
            this.extractImageToolStripMenuItem.Click += new System.EventHandler(this.extract_Image_Click);
            // 
            // tabPage2
            // 
            this.tabPage2.Controls.Add(this.listView);
            this.tabPage2.Location = new System.Drawing.Point(4, 22);
            this.tabPage2.Name = "tabPage2";
            this.tabPage2.Padding = new System.Windows.Forms.Padding(3);
            this.tabPage2.Size = new System.Drawing.Size(404, 276);
            this.tabPage2.TabIndex = 1;
            this.tabPage2.Text = "Thumbnail List";
            this.tabPage2.UseVisualStyleBackColor = true;
            // 
            // listView
            // 
            this.listView.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
            this.listView.Dock = System.Windows.Forms.DockStyle.Fill;
            this.listView.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.listView.HideSelection = false;
            this.listView.Location = new System.Drawing.Point(3, 3);
            this.listView.Margin = new System.Windows.Forms.Padding(0);
            this.listView.MultiSelect = false;
            this.listView.Name = "listView";
            this.listView.OwnerDraw = true;
            this.listView.Size = new System.Drawing.Size(398, 270);
            this.listView.TabIndex = 0;
            this.listView.TileSize = new System.Drawing.Size(81, 110);
            this.listView.UseCompatibleStateImageBehavior = false;
            this.listView.View = System.Windows.Forms.View.Tile;
            this.listView.MouseDoubleClick += new System.Windows.Forms.MouseEventHandler(this.listView_DoubleClick);
            this.listView.DrawItem += new System.Windows.Forms.DrawListViewItemEventHandler(this.listViewdrawItem);
            this.listView.SelectedIndexChanged += new System.EventHandler(this.selectChanged_listView);
            // 
            // tabPage3
            // 
            this.tabPage3.Controls.Add(this.listView1);
            this.tabPage3.Location = new System.Drawing.Point(4, 22);
            this.tabPage3.Name = "tabPage3";
            this.tabPage3.Padding = new System.Windows.Forms.Padding(3);
            this.tabPage3.Size = new System.Drawing.Size(404, 276);
            this.tabPage3.TabIndex = 2;
            this.tabPage3.Text = "Frames";
            this.tabPage3.UseVisualStyleBackColor = true;
            // 
            // listView1
            // 
            this.listView1.Dock = System.Windows.Forms.DockStyle.Fill;
            this.listView1.Location = new System.Drawing.Point(3, 3);
            this.listView1.MultiSelect = false;
            this.listView1.Name = "listView1";
            this.listView1.OwnerDraw = true;
            this.listView1.Size = new System.Drawing.Size(398, 270);
            this.listView1.TabIndex = 0;
            this.listView1.TileSize = new System.Drawing.Size(81, 110);
            this.listView1.UseCompatibleStateImageBehavior = false;
            this.listView1.View = System.Windows.Forms.View.Tile;
            this.listView1.DrawItem += new System.Windows.Forms.DrawListViewItemEventHandler(this.Frames_ListView_DrawItem);
            // 
            // statusStrip1
            // 
            this.statusStrip1.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.SettingsButton,
            this.GraphicLabel,
            this.BaseGraphicLabel,
            this.HueLabel});
            this.statusStrip1.Location = new System.Drawing.Point(0, 302);
            this.statusStrip1.Name = "statusStrip1";
            this.statusStrip1.RightToLeft = System.Windows.Forms.RightToLeft.No;
            this.statusStrip1.Size = new System.Drawing.Size(412, 22);
            this.statusStrip1.SizingGrip = false;
            this.statusStrip1.TabIndex = 0;
            this.statusStrip1.Text = "statusStrip1";
            // 
            // SettingsButton
            // 
            this.SettingsButton.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
            this.SettingsButton.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.sortAlphaToolStripMenuItem,
            this.hueToolStripMenuItem,
            this.animateToolStripMenuItem});
            this.SettingsButton.Image = ((System.Drawing.Image)(resources.GetObject("SettingsButton.Image")));
            this.SettingsButton.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.SettingsButton.Name = "SettingsButton";
            this.SettingsButton.Size = new System.Drawing.Size(62, 20);
            this.SettingsButton.Text = "Settings";
            // 
            // sortAlphaToolStripMenuItem
            // 
            this.sortAlphaToolStripMenuItem.CheckOnClick = true;
            this.sortAlphaToolStripMenuItem.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
            this.sortAlphaToolStripMenuItem.Name = "sortAlphaToolStripMenuItem";
            this.sortAlphaToolStripMenuItem.Size = new System.Drawing.Size(168, 22);
            this.sortAlphaToolStripMenuItem.Text = "Sort alphabetically";
            this.sortAlphaToolStripMenuItem.Click += new System.EventHandler(this.OnClick_Sort);
            // 
            // hueToolStripMenuItem
            // 
            this.hueToolStripMenuItem.Name = "hueToolStripMenuItem";
            this.hueToolStripMenuItem.Size = new System.Drawing.Size(168, 22);
            this.hueToolStripMenuItem.Text = "Hue";
            this.hueToolStripMenuItem.Click += new System.EventHandler(this.OnClick_Hue);
            // 
            // animateToolStripMenuItem
            // 
            this.animateToolStripMenuItem.CheckOnClick = true;
            this.animateToolStripMenuItem.Name = "animateToolStripMenuItem";
            this.animateToolStripMenuItem.Size = new System.Drawing.Size(168, 22);
            this.animateToolStripMenuItem.Text = "Animate";
            this.animateToolStripMenuItem.Click += new System.EventHandler(this.Animate_Click);
            // 
            // GraphicLabel
            // 
            this.GraphicLabel.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
            this.GraphicLabel.Name = "GraphicLabel";
            this.GraphicLabel.RightToLeft = System.Windows.Forms.RightToLeft.No;
            this.GraphicLabel.Size = new System.Drawing.Size(50, 17);
            this.GraphicLabel.Text = "Graphic: ";
            this.GraphicLabel.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            // 
            // BaseGraphicLabel
            // 
            this.BaseGraphicLabel.AutoSize = false;
            this.BaseGraphicLabel.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
            this.BaseGraphicLabel.Name = "BaseGraphicLabel";
            this.BaseGraphicLabel.RightToLeft = System.Windows.Forms.RightToLeft.No;
            this.BaseGraphicLabel.Size = new System.Drawing.Size(100, 17);
            this.BaseGraphicLabel.Text = "BaseGraphic:";
            this.BaseGraphicLabel.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            // 
            // HueLabel
            // 
            this.HueLabel.AutoSize = false;
            this.HueLabel.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
            this.HueLabel.ImageAlign = System.Drawing.ContentAlignment.MiddleRight;
            this.HueLabel.Name = "HueLabel";
            this.HueLabel.RightToLeft = System.Windows.Forms.RightToLeft.No;
            this.HueLabel.Size = new System.Drawing.Size(60, 17);
            this.HueLabel.Text = "Hue:";
            this.HueLabel.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            // 
            // MobList
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.Controls.Add(this.splitContainer1);
            this.DoubleBuffered = true;
            this.Name = "MobList";
            this.Size = new System.Drawing.Size(619, 324);
            this.Load += new System.EventHandler(this.OnLoad);
            this.splitContainer1.Panel1.ResumeLayout(false);
            this.splitContainer1.Panel2.ResumeLayout(false);
            this.splitContainer1.Panel2.PerformLayout();
            this.splitContainer1.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.FacingBar)).EndInit();
            this.tabControl1.ResumeLayout(false);
            this.tabPage1.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.MainPictureBox)).EndInit();
            this.contextMenuStrip1.ResumeLayout(false);
            this.tabPage2.ResumeLayout(false);
            this.tabPage3.ResumeLayout(false);
            this.statusStrip1.ResumeLayout(false);
            this.statusStrip1.PerformLayout();
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.SplitContainer splitContainer1;
        private System.Windows.Forms.TreeView TreeViewMobs;
        private System.Windows.Forms.TabControl tabControl1;
        private System.Windows.Forms.TabPage tabPage1;
        private System.Windows.Forms.PictureBox MainPictureBox;
        private System.Windows.Forms.TabPage tabPage2;
        private System.Windows.Forms.ListView listView;
        private System.Windows.Forms.StatusStrip statusStrip1;
        private System.Windows.Forms.ToolStripSplitButton SettingsButton;
        private System.Windows.Forms.ToolStripMenuItem animateToolStripMenuItem;
        private System.Windows.Forms.ToolStripStatusLabel GraphicLabel;
        private System.Windows.Forms.ToolStripStatusLabel BaseGraphicLabel;
        private System.Windows.Forms.ToolStripStatusLabel HueLabel;
        private System.Windows.Forms.ContextMenuStrip contextMenuStrip1;
        private System.Windows.Forms.ToolStripMenuItem extractImageToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem hueToolStripMenuItem;
        private System.Windows.Forms.TabPage tabPage3;
        private System.Windows.Forms.ListView listView1;
        private System.Windows.Forms.TrackBar FacingBar;
        private System.Windows.Forms.ToolStripMenuItem sortAlphaToolStripMenuItem;

    }
}
