// Super Mario Galaxy for the PlayStation 5 - the builder's Mac app: drop in your
// disc image, get the PS5 title folder. It runs Contents/MacOS/galaxy-builder
// (ps5/builder/builder.cpp) with the kit in Contents/Resources/kit and shows its
// progress lines (STEP, PROGRESS, LOG, ERROR, DONE).
//
// SPDX-License-Identifier: GPL-3.0-only
import AppKit
import Combine
import SwiftUI
import UniformTypeIdentifiers

@main
struct BuilderApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) private var delegate
    var body: some Scene {
        WindowGroup("Super Mario Galaxy PS5 Builder") {
            ContentView()
                .frame(minWidth: 620, minHeight: 560)
        }
        .windowResizability(.contentSize)
    }
}

final class AppDelegate: NSObject, NSApplicationDelegate {
    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { true }
}

@MainActor
final class Build: ObservableObject {
    enum State: Equatable { case idle, running, done(URL), failed(String) }

    @Published var disc: URL?
    @Published var output: URL = FileManager.default.urls(for: .desktopDirectory, in: .userDomainMask).first!
    @Published var state: State = .idle
    @Published var stepNumber = 0
    @Published var stepCount = 7
    @Published var stepText = ""
    @Published var fraction: Double?
    @Published var log: [String] = []
    @Published var started: Date?

    private var process: Process?
    private var pending = Data()

    var running: Bool { state == .running }

    func start() {
        guard let disc, !running else { return }
        guard let tool = Bundle.main.url(forAuxiliaryExecutable: "galaxy-builder") else {
            state = .failed("The app is incomplete: galaxy-builder is missing.")
            return
        }
        state = .running
        stepNumber = 0
        stepText = "Starting"
        fraction = nil
        log = []
        started = Date()
        let process = Process()
        process.executableURL = tool
        process.arguments = ["--disc", disc.path, "--out", output.path]
        let pipe = Pipe()
        process.standardOutput = pipe
        process.standardError = pipe
        pipe.fileHandleForReading.readabilityHandler = { [weak self] handle in
            let data = handle.availableData
            Task { @MainActor in self?.receive(data) }
        }
        process.terminationHandler = { [weak self] finished in
            Task { @MainActor in self?.finish(status: finished.terminationStatus) }
        }
        do {
            try process.run()
            self.process = process
        } catch {
            state = .failed("The builder did not start: \(error.localizedDescription)")
        }
    }

    func cancel() {
        guard let process, process.isRunning else { return }
        // the builder's tools are its children: end the whole group
        kill(-process.processIdentifier, SIGTERM)
        process.terminate()
        state = .failed("Cancelled.")
    }

    private func receive(_ data: Data) {
        guard !data.isEmpty else { return }
        pending.append(data)
        while let newline = pending.firstIndex(of: 0x0A) {
            let line = String(decoding: pending[pending.startIndex..<newline], as: UTF8.self)
            pending.removeSubrange(pending.startIndex...newline)
            handle(line)
        }
    }

    private func handle(_ line: String) {
        let parts = line.split(separator: " ", maxSplits: 1).map(String.init)
        guard let kind = parts.first else { return }
        let rest = parts.count > 1 ? parts[1] : ""
        switch kind {
        case "STEP":
            let fields = rest.split(separator: " ", maxSplits: 2).map(String.init)
            if fields.count == 3 {
                stepNumber = Int(fields[0]) ?? stepNumber
                stepCount = Int(fields[1]) ?? stepCount
                stepText = fields[2]
                fraction = nil
            }
        case "PROGRESS":
            let fields = rest.split(separator: " ").compactMap { Double($0) }
            if fields.count == 2, fields[1] > 0 { fraction = min(1, fields[0] / fields[1]) }
        case "LOG":
            log.append(rest)
            if log.count > 2000 { log.removeFirst(log.count - 2000) }
        case "ERROR":
            state = .failed(rest)
        case "DONE":
            state = .done(URL(fileURLWithPath: rest))
        default:
            log.append(line)
        }
    }

    private func finish(status: Int32) {
        process = nil
        if state == .running {
            state = status == 0 ? .failed("The builder stopped without finishing.") : .failed("The builder stopped (status \(status)).")
        }
    }
}

struct ContentView: View {
    @StateObject private var build = Build()
    @State private var targeted = false
    @State private var showLog = false
    @State private var now = Date()
    private let timer = Timer.publish(every: 1, on: .main, in: .common).autoconnect()

    var body: some View {
        VStack(alignment: .leading, spacing: 18) {
            header
            discDrop
            outputRow
            Divider()
            status
            Spacer(minLength: 0)
        }
        .padding(24)
        .onReceive(timer) { now = $0 }
        .onAppear {
            // --disc PATH [--out FOLDER] [--build]: for testing the app without a drop
            let arguments = CommandLine.arguments
            if let i = arguments.firstIndex(of: "--disc"), i + 1 < arguments.count {
                build.disc = URL(fileURLWithPath: arguments[i + 1])
            }
            if let i = arguments.firstIndex(of: "--out"), i + 1 < arguments.count {
                build.output = URL(fileURLWithPath: arguments[i + 1])
            }
            if arguments.contains("--build") { build.start() }
        }
    }

    private var header: some View {
        HStack(spacing: 14) {
            Image(nsImage: NSApp.applicationIconImage)
                .resizable()
                .frame(width: 64, height: 64)
            VStack(alignment: .leading, spacing: 4) {
                Text("Super Mario Galaxy for PS5")
                    .font(.title2.weight(.semibold))
                Text("Makes the PlayStation 5 title from your own disc. Nothing of the game comes with this app.")
                    .font(.callout)
                    .foregroundStyle(.secondary)
            }
        }
    }

    private var discDrop: some View {
        VStack(spacing: 10) {
            Image(systemName: build.disc == nil ? "opticaldisc" : "checkmark.circle.fill")
                .font(.system(size: 34))
                .foregroundStyle(build.disc == nil ? Color.secondary : Color.green)
            if let disc = build.disc {
                Text(disc.lastPathComponent).font(.headline).lineLimit(1).truncationMode(.middle)
                Text("Drop another image to change it").font(.caption).foregroundStyle(.secondary)
            } else {
                Text("Drop your Super Mario Galaxy disc image here").font(.headline)
                Text("USA disc (RMGE01), revision 0: .iso, .wbfs, .rvz, .wia, .ciso or .gcz")
                    .font(.caption).foregroundStyle(.secondary)
            }
            Button("Choose…") { chooseDisc() }.disabled(build.running)
        }
        .frame(maxWidth: .infinity, minHeight: 150)
        .background(
            RoundedRectangle(cornerRadius: 12)
                .strokeBorder(style: StrokeStyle(lineWidth: 2, dash: [7]))
                .foregroundStyle(targeted ? Color.accentColor : Color.secondary.opacity(0.5))
        )
        .onDrop(of: [.fileURL], isTargeted: $targeted) { providers in
            guard !build.running, let provider = providers.first else { return false }
            _ = provider.loadObject(ofClass: URL.self) { url, _ in
                if let url { Task { @MainActor in build.disc = url } }
            }
            return true
        }
    }

    private var outputRow: some View {
        HStack {
            Text("Save the title in:").foregroundStyle(.secondary)
            Text(build.output.path.replacingOccurrences(of: NSHomeDirectory(), with: "~"))
                .lineLimit(1).truncationMode(.middle)
            Spacer()
            Button("Change…") { chooseOutput() }.disabled(build.running)
        }
    }

    @ViewBuilder private var status: some View {
        switch build.state {
        case .idle:
            HStack {
                Text("Building takes about 5–15 minutes and needs about 12 GB free.")
                    .font(.callout).foregroundStyle(.secondary)
                Spacer()
                Button("Build") { build.start() }
                    .keyboardShortcut(.defaultAction)
                    .disabled(build.disc == nil)
            }
        case .running:
            VStack(alignment: .leading, spacing: 8) {
                HStack {
                    Text("Step \(build.stepNumber) of \(build.stepCount): \(build.stepText)").font(.headline)
                    Spacer()
                    Text(elapsed).monospacedDigit().foregroundStyle(.secondary)
                }
                if let fraction = build.fraction {
                    ProgressView(value: fraction)
                } else {
                    ProgressView().progressViewStyle(.linear)
                }
                HStack {
                    logToggle
                    Spacer()
                    Button("Cancel") { build.cancel() }
                }
                logView
            }
        case .done(let folder):
            VStack(alignment: .leading, spacing: 10) {
                Label("Your title is ready", systemImage: "checkmark.seal.fill")
                    .font(.headline).foregroundStyle(.green)
                Text("Copy the **\(folder.lastPathComponent)** folder to **/data/homebrew/** on your PS5 over FTP, then start Super Mario Galaxy from the home screen (restart ShadowMountPlus if it doesn't appear).")
                    .fixedSize(horizontal: false, vertical: true)
                HStack {
                    Button("Show in Finder") { NSWorkspace.shared.activateFileViewerSelecting([folder]) }
                        .keyboardShortcut(.defaultAction)
                    Button("Build again") { build.state = .idle }
                    Spacer()
                    logToggle
                }
                logView
            }
        case .failed(let message):
            VStack(alignment: .leading, spacing: 10) {
                Label(message, systemImage: "exclamationmark.triangle.fill")
                    .foregroundStyle(.red).fixedSize(horizontal: false, vertical: true)
                HStack {
                    Button("Try again") { build.state = .idle }
                    Spacer()
                    logToggle
                }
                logView
            }
        }
    }

    private var logToggle: some View {
        Button(showLog ? "Hide details" : "Show details") { showLog.toggle() }
            .buttonStyle(.link)
    }

    @ViewBuilder private var logView: some View {
        if showLog {
            ScrollViewReader { scroller in
                ScrollView {
                    LazyVStack(alignment: .leading, spacing: 1) {
                        ForEach(Array(build.log.enumerated()), id: \.offset) { index, line in
                            Text(line).font(.system(.caption, design: .monospaced)).id(index)
                                .textSelection(.enabled)
                        }
                    }
                    .frame(maxWidth: .infinity, alignment: .leading)
                }
                .frame(height: 160)
                .background(Color(nsColor: .textBackgroundColor))
                .onChange(of: build.log.count) { count in scroller.scrollTo(count - 1, anchor: .bottom) }
            }
        }
    }

    private var elapsed: String {
        guard let started = build.started else { return "" }
        let seconds = Int(now.timeIntervalSince(started))
        return String(format: "%d:%02d", seconds / 60, seconds % 60)
    }

    private func chooseDisc() {
        let panel = NSOpenPanel()
        panel.title = "Choose your Super Mario Galaxy disc image"
        panel.allowsMultipleSelection = false
        panel.canChooseDirectories = false
        if panel.runModal() == .OK { build.disc = panel.url }
    }

    private func chooseOutput() {
        let panel = NSOpenPanel()
        panel.title = "Choose where to save the PS5 title"
        panel.canChooseDirectories = true
        panel.canChooseFiles = false
        panel.canCreateDirectories = true
        if panel.runModal() == .OK, let url = panel.url { build.output = url }
    }
}
