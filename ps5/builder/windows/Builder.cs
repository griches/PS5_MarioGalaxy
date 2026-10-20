// Super Mario Galaxy for the PlayStation 5 - the builder's Windows app: drop in
// your disc image, get the PS5 title folder. It runs galaxy-builder.exe
// (ps5/builder/builder.cpp) beside it, with the kit in kit\, and shows its
// progress lines (STEP, PROGRESS, LOG, ERROR, DONE). .NET Framework 4.8 (in
// Windows 10 and 11): built with the framework's own csc.exe.
//
// SPDX-License-Identifier: GPL-3.0-only
using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Windows.Forms;

[assembly: System.Reflection.AssemblyTitle("Super Mario Galaxy PS5 Builder")]
[assembly: System.Reflection.AssemblyProduct("Super Mario Galaxy PS5 Builder")]
[assembly: System.Reflection.AssemblyCopyright("GPL-3.0. Not affiliated with Nintendo.")]

namespace GalaxyBuilder {

static class Program {
    [DllImport("user32.dll")] static extern bool SetProcessDPIAware();

    [STAThread]
    static void Main() {
        if (Environment.OSVersion.Version.Major >= 6) SetProcessDPIAware();
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        Application.Run(new BuilderForm());
    }
}

sealed class BuilderForm : Form {
    readonly Label dropLabel = new Label();
    readonly Label dropHint = new Label();
    readonly Panel dropPanel = new Panel();
    readonly Button chooseDisc = new Button();
    readonly Label outputLabel = new Label();
    readonly Button chooseOutput = new Button();
    readonly Label stepLabel = new Label();
    readonly Label timeLabel = new Label();
    readonly ProgressBar progress = new ProgressBar();
    readonly Button buildButton = new Button();
    readonly Button cancelButton = new Button();
    readonly Button openButton = new Button();
    readonly LinkLabel detailsLink = new LinkLabel();
    readonly TextBox logBox = new TextBox();
    readonly Timer clock = new Timer();

    string disc;
    string output = Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory);
    string title;
    Process process;
    DateTime started;
    bool failed;

    public BuilderForm() {
        Text = "Super Mario Galaxy PS5 Builder";
        Font = new Font("Segoe UI", 9.75f);
        AutoScaleDimensions = new SizeF(96f, 96f);  // laid out at 96 DPI, scaled to the screen's
        AutoScaleMode = AutoScaleMode.Dpi;
        ClientSize = new Size(640, 560);
        MinimumSize = new Size(560, 480);
        try { Icon = Icon.ExtractAssociatedIcon(Application.ExecutablePath); } catch { }

        var heading = new Label { Text = "Super Mario Galaxy for PS5", Font = new Font("Segoe UI Semibold", 15f),
                                  AutoSize = true, Location = new Point(24, 18) };
        var blurb = new Label { Text = "Makes the PlayStation 5 title from your own disc. Nothing of the game comes with this app.",
                                AutoSize = false, Size = new Size(590, 36), Location = new Point(24, 54),
                                ForeColor = SystemColors.GrayText };

        dropPanel.Location = new Point(24, 96);
        dropPanel.Size = new Size(592, 150);
        dropPanel.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
        dropPanel.BorderStyle = BorderStyle.FixedSingle;
        dropPanel.AllowDrop = true;
        dropPanel.DragEnter += (s, e) => {
            e.Effect = e.Data.GetDataPresent(DataFormats.FileDrop) && process == null ? DragDropEffects.Copy : DragDropEffects.None;
        };
        dropPanel.DragDrop += (s, e) => {
            var files = e.Data.GetData(DataFormats.FileDrop) as string[];
            if (files != null && files.Length > 0) SetDisc(files[0]);
        };
        dropLabel.Text = "Drop your Super Mario Galaxy disc image here";
        dropLabel.Font = new Font("Segoe UI Semibold", 11f);
        dropLabel.TextAlign = ContentAlignment.MiddleCenter;
        dropLabel.Dock = DockStyle.Top;
        dropLabel.Height = 60;
        dropHint.Text = "USA disc (RMGE01), revision 0: .iso, .wbfs, .rvz, .wia, .ciso or .gcz";
        dropHint.ForeColor = SystemColors.GrayText;
        dropHint.TextAlign = ContentAlignment.TopCenter;
        dropHint.Dock = DockStyle.Top;
        dropHint.Height = 30;
        chooseDisc.Text = "Choose...";
        chooseDisc.AutoSize = true;
        chooseDisc.Click += (s, e) => {
            using (var dialog = new OpenFileDialog { Title = "Choose your Super Mario Galaxy disc image",
                                                     Filter = "Disc images|*.iso;*.wbfs;*.rvz;*.wia;*.ciso;*.gcz|All files|*.*" }) {
                if (dialog.ShowDialog(this) == DialogResult.OK) SetDisc(dialog.FileName);
            }
        };
        var chooseHolder = new FlowLayoutPanel { Dock = DockStyle.Top, Height = 40, FlowDirection = FlowDirection.TopDown,
                                                 WrapContents = false };
        chooseHolder.Padding = new Padding(250, 0, 0, 0);
        chooseHolder.Controls.Add(chooseDisc);
        dropPanel.Controls.Add(chooseHolder);
        dropPanel.Controls.Add(dropHint);
        dropPanel.Controls.Add(dropLabel);
        foreach (Control c in new Control[] { dropLabel, dropHint, chooseHolder }) {
            c.AllowDrop = true;
            c.DragEnter += (s, e) => { e.Effect = e.Data.GetDataPresent(DataFormats.FileDrop) && process == null ? DragDropEffects.Copy : DragDropEffects.None; };
            c.DragDrop += (s, e) => { var f = e.Data.GetData(DataFormats.FileDrop) as string[]; if (f != null && f.Length > 0) SetDisc(f[0]); };
        }

        outputLabel.Location = new Point(24, 262);
        outputLabel.Size = new Size(480, 24);
        outputLabel.AutoEllipsis = true;
        outputLabel.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
        chooseOutput.Text = "Change...";
        chooseOutput.AutoSize = true;
        chooseOutput.Location = new Point(530, 258);
        chooseOutput.Anchor = AnchorStyles.Top | AnchorStyles.Right;
        chooseOutput.Click += (s, e) => {
            using (var dialog = new FolderBrowserDialog { Description = "Choose where to save the PS5 title", SelectedPath = output }) {
                if (dialog.ShowDialog(this) == DialogResult.OK) { output = dialog.SelectedPath; ShowOutput(); }
            }
        };

        stepLabel.Location = new Point(24, 304);
        stepLabel.Size = new Size(500, 44);
        stepLabel.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
        stepLabel.Text = "Building takes about 5-15 minutes and needs about 12 GB free.";
        timeLabel.Location = new Point(530, 304);
        timeLabel.Size = new Size(86, 24);
        timeLabel.TextAlign = ContentAlignment.TopRight;
        timeLabel.Anchor = AnchorStyles.Top | AnchorStyles.Right;
        progress.Location = new Point(24, 350);
        progress.Size = new Size(592, 18);
        progress.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
        progress.Maximum = 1000;
        progress.Visible = false;

        buildButton.Text = "Build";
        buildButton.Size = new Size(110, 32);
        buildButton.Location = new Point(506, 380);
        buildButton.Anchor = AnchorStyles.Top | AnchorStyles.Right;
        buildButton.Enabled = false;
        buildButton.Click += (s, e) => Start();
        cancelButton.Text = "Cancel";
        cancelButton.Size = new Size(110, 32);
        cancelButton.Location = new Point(506, 380);
        cancelButton.Anchor = AnchorStyles.Top | AnchorStyles.Right;
        cancelButton.Visible = false;
        cancelButton.Click += (s, e) => Cancel();
        openButton.Text = "Open folder";
        openButton.Size = new Size(110, 32);
        openButton.Location = new Point(386, 380);
        openButton.Anchor = AnchorStyles.Top | AnchorStyles.Right;
        openButton.Visible = false;
        openButton.Click += (s, e) => {
            if (title != null) Process.Start("explorer.exe", "/select,\"" + title + "\"");
        };
        AcceptButton = buildButton;

        detailsLink.Text = "Show details";
        detailsLink.AutoSize = true;
        detailsLink.Location = new Point(24, 388);
        detailsLink.LinkClicked += (s, e) => {
            logBox.Visible = !logBox.Visible;
            detailsLink.Text = logBox.Visible ? "Hide details" : "Show details";
        };
        logBox.Location = new Point(24, 422);
        logBox.Size = new Size(592, 120);
        logBox.Anchor = AnchorStyles.Top | AnchorStyles.Bottom | AnchorStyles.Left | AnchorStyles.Right;
        logBox.Multiline = true;
        logBox.ReadOnly = true;
        logBox.ScrollBars = ScrollBars.Vertical;
        logBox.Font = new Font("Consolas", 8.5f);
        logBox.Visible = false;

        clock.Interval = 1000;
        clock.Tick += (s, e) => {
            var t = DateTime.Now - started;
            timeLabel.Text = string.Format("{0}:{1:00}", (int)t.TotalMinutes, t.Seconds);
        };

        Controls.AddRange(new Control[] { heading, blurb, dropPanel, outputLabel, chooseOutput, stepLabel, timeLabel,
                                          progress, buildButton, cancelButton, openButton, detailsLink, logBox });
        ShowOutput();
        // --disc PATH [--out FOLDER] [--build]: for testing the app without a drop
        Shown += (s, e) => {
            string[] arguments = Environment.GetCommandLineArgs();
            for (int i = 1; i < arguments.Length - 1; ++i) {
                if (arguments[i] == "--disc") SetDisc(arguments[i + 1]);
                if (arguments[i] == "--out") { output = arguments[i + 1]; ShowOutput(); }
            }
            if (Array.IndexOf(arguments, "--build") > 0) Start();
        };
        FormClosing += (s, e) => {
            if (process != null && MessageBox.Show(this, "Stop building?", Text, MessageBoxButtons.YesNo) == DialogResult.No) {
                e.Cancel = true;
            } else {
                Cancel();
            }
        };
    }

    void SetDisc(string path) {
        if (process != null) return;
        disc = path;
        dropLabel.Text = Path.GetFileName(path);
        dropHint.Text = "Drop another image to change it";
        buildButton.Enabled = true;
        openButton.Visible = false;
        stepLabel.ForeColor = SystemColors.ControlText;
        stepLabel.Text = "Building takes about 5-15 minutes and needs about 12 GB free.";
    }

    void ShowOutput() { outputLabel.Text = "Save the title in:  " + output; }

    void Start() {
        if (disc == null || process != null) return;
        string tool = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "galaxy-builder.exe");
        if (!File.Exists(tool)) { Fail("The app is incomplete: galaxy-builder.exe is missing from its folder."); return; }
        failed = false;
        title = null;
        logBox.Clear();
        progress.Value = 0;
        progress.Style = ProgressBarStyle.Marquee;
        progress.Visible = true;
        stepLabel.ForeColor = SystemColors.ControlText;
        stepLabel.Text = "Starting";
        buildButton.Visible = false;
        openButton.Visible = false;
        cancelButton.Visible = true;
        chooseDisc.Enabled = chooseOutput.Enabled = false;
        started = DateTime.Now;
        timeLabel.Text = "0:00";
        clock.Start();
        var info = new ProcessStartInfo(tool, "--disc " + Quote(disc) + " --out " + Quote(output)) {
            UseShellExecute = false, CreateNoWindow = true, RedirectStandardOutput = true, RedirectStandardError = true,
            StandardOutputEncoding = Encoding.UTF8
        };
        process = new Process { StartInfo = info, EnableRaisingEvents = true };
        process.OutputDataReceived += (s, e) => { if (e.Data != null) BeginInvoke((Action)(() => Handle(e.Data))); };
        process.ErrorDataReceived += (s, e) => { if (e.Data != null) BeginInvoke((Action)(() => AddLog(e.Data))); };
        process.Exited += (s, e) => BeginInvoke((Action)Finished);
        try {
            process.Start();
            process.BeginOutputReadLine();
            process.BeginErrorReadLine();
        } catch (Exception error) {
            process = null;
            Fail("The builder did not start: " + error.Message);
        }
    }

    static string Quote(string argument) { return "\"" + argument.TrimEnd('\\') + "\""; }

    void Handle(string line) {
        int space = line.IndexOf(' ');
        string kind = space < 0 ? line : line.Substring(0, space);
        string rest = space < 0 ? "" : line.Substring(space + 1);
        if (kind == "STEP") {
            var f = rest.Split(new[] { ' ' }, 3);
            if (f.Length == 3) stepLabel.Text = "Step " + f[0] + " of " + f[1] + ": " + f[2];
            progress.Style = ProgressBarStyle.Marquee;
        } else if (kind == "PROGRESS") {
            var f = rest.Split(' ');
            double done, total;
            if (f.Length == 2 && double.TryParse(f[0], out done) && double.TryParse(f[1], out total) && total > 0) {
                progress.Style = ProgressBarStyle.Continuous;
                progress.Value = (int)Math.Min(1000, done * 1000 / total);
            }
        } else if (kind == "LOG") {
            AddLog(rest);
        } else if (kind == "ERROR") {
            Fail(rest);
        } else if (kind == "DONE") {
            title = rest;
            stepLabel.ForeColor = Color.DarkGreen;
            stepLabel.Text = "Your title is ready. Copy the " + Path.GetFileName(rest) + " folder to /data/homebrew/ on your PS5 over FTP, then start Super Mario Galaxy from the home screen.";
            openButton.Visible = true;
        } else {
            AddLog(line);
        }
    }

    void AddLog(string line) {
        if (logBox.TextLength > 200000) logBox.Text = logBox.Text.Substring(100000);
        logBox.AppendText(line + Environment.NewLine);
    }

    void Fail(string message) {
        failed = true;
        stepLabel.ForeColor = Color.Firebrick;
        stepLabel.Text = message;
        if (!logBox.Visible) { logBox.Visible = true; detailsLink.Text = "Hide details"; }
    }

    void Finished() {
        if (process == null) return;
        int status = process.ExitCode;
        process.Dispose();
        process = null;
        clock.Stop();
        progress.Visible = false;
        cancelButton.Visible = false;
        buildButton.Visible = true;
        buildButton.Text = "Build again";
        chooseDisc.Enabled = chooseOutput.Enabled = true;
        if (title == null && !failed) Fail("The builder stopped (status " + status + ").");
    }

    void Cancel() {
        if (process == null) return;
        try { process.Kill(); } catch { }  // galaxy-builder's tools are in its job: they end with it
        Fail("Cancelled.");
    }
}

}
