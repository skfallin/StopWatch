import SwiftUI
import PhotosUI
import UniformTypeIdentifiers

struct BadgePage: View {
    @EnvironmentObject private var connection: WatchConnection
    @State private var photo: PhotosPickerItem?
    @State private var importer = false
    @State private var sourceData: Data?
    @State private var source: BadgeSource?
    @State private var sourceID = UUID()
    @State private var importID = UUID()
    @State private var layout = BadgeLayout()
    @State private var asset: BadgeAsset?
    @State private var preparedKey: PreparationKey?
    @State private var error: String?
    @State private var preparationError: String?
    @State private var upload: Task<Void, Never>?
    @State private var sent = false
    private struct PreparationKey: Hashable { let source: UUID; let layout: BadgeLayout }
    private var key: PreparationKey { PreparationKey(source: sourceID,layout: layout) }
    private var preparing: Bool { sourceData != nil && preparedKey != key && preparationError == nil }
    private var remote: BadgeStatus? { connection.badgeState ?? connection.status?.badge }
    private var readyAsset: BadgeAsset? { sourceData == nil || preparedKey == key ? asset : nil }

    var body: some View {
        VStack(spacing: 12) {
            previewCard
            sendCard
            behaviorCard

        }
        .fileImporter(isPresented: $importer,allowedContentTypes: [.image],allowsMultipleSelection: false) { result in
            Task {
                do {
                    guard let url=try result.get().first else { return }
                    let access=url.startAccessingSecurityScopedResource();defer { if access { url.stopAccessingSecurityScopedResource() } }
                    guard (try url.resourceValues(forKeys: [.fileSizeKey]).fileSize ?? Int.max)<=25*1024*1024 else { throw CompanionError.message("Il file supera 25 MB.") }
                    await importImage(try Data(contentsOf: url))
                } catch { self.error=error.localizedDescription }
            }
        }
        .task(id: photo) {
            guard let photo else { return }
            do { guard let data=try await photo.loadTransferable(type: Data.self) else { throw CompanionError.message("Immagine non disponibile.") };await importImage(data) }
            catch is CancellationError { }
            catch { self.error=error.localizedDescription }
        }
        .task(id: key) {
            guard let sourceData else { return }
            let preparationKey=self.key;preparationError=nil
            do {
                try await Task.sleep(for: .milliseconds(250))
                let worker=Task.detached(priority: .userInitiated) { try BadgeAsset.prepare(sourceData,layout: preparationKey.layout) }
                let result=try await withTaskCancellationHandler(operation: { try await worker.value },onCancel: { worker.cancel() })
                try Task.checkCancellation();asset=result;preparedKey=preparationKey;sent=false
            } catch is CancellationError { }
            catch { preparationError=error.localizedDescription }
        }
        .task(id: connection.localBadgeURL) {
            guard sourceData == nil, asset == nil, let url=connection.localBadgeURL, FileManager.default.fileExists(atPath: url.path) else { return }
            do {
                let data=try Data(contentsOf: url)
                let saved=try await Task.detached { try BadgeAsset.decode(data) }.value
                if sourceData == nil { asset=saved }
            } catch { self.error=error.localizedDescription }
        }
        .onDisappear { upload?.cancel() }
    }

    private var previewCard: some View {
    PaperCard {
        HStack {
            VStack(alignment: .leading,spacing: 6) {
                Text("IL TUO BADGE").font(.caption.weight(.medium)).tracking(1.4).foregroundStyle(Palette.secondary)
                Text("Fallo tuo.").font(.title.weight(.medium))
            }
            Spacer()
            Image(systemName: "photo.on.rectangle.angled").font(.title2).accessibilityHidden(true)
        }
        BadgePreview(asset: readyAsset,source: source,layout: $layout)
            .overlay(alignment: .bottom) {
                if preparing { ProgressView("Preparo…").font(.caption).tint(.white).padding(12).background(.black.opacity(0.8),in: Capsule()).padding(.bottom,20) }
            }
        HStack {
            Label(readyAsset?.animated == true ? "GIF animata" : "Immagine",systemImage: readyAsset?.animated == true ? "play.circle" : "photo")
            Spacer()
            Text("466 × 466").monospacedDigit()
        }.font(.caption).foregroundStyle(Palette.secondary)
        if let asset = readyAsset {
            Text("\(asset.previews.count) fotogramm\(asset.previews.count == 1 ? "a" : "i") · \(ByteCountFormatter.string(fromByteCount: Int64(asset.data.count),countStyle: .file))" +
                 (asset.animated ? String(format: " · %.1f s",Double(asset.milliseconds)/1000) : ""))
                .font(.caption).foregroundStyle(Palette.secondary)
        }
        PhotosPicker(selection: $photo,matching: .images,photoLibrary: .shared()) {
            Label(source == nil && asset == nil ? "Scegli foto o GIF" : "Cambia immagine",systemImage: "photo.badge.plus")
                .font(.headline).frame(maxWidth: .infinity).padding(.vertical,15)
                .foregroundStyle(.white).background(Palette.ink,in: Capsule())
        }.buttonStyle(.plain).disabled(upload != nil)
        Button { importer = true } label: { Label("Scegli da File",systemImage: "folder").frame(maxWidth: .infinity,minHeight: 44) }
            .buttonStyle(.plain).disabled(upload != nil)
        if source != nil {
            Picker("Inquadratura",selection: $layout.fit) {
                Text("Riempi").tag(false); Text("Immagine intera").tag(true)
            }.pickerStyle(.segmented).disabled(upload != nil)
            HStack {
                Text("Pizzica e trascina per inquadrare.").font(.caption).foregroundStyle(Palette.secondary)
                Spacer()
                Button("Ripristina") { let rate=layout.framesPerSecond;layout=BadgeLayout();layout.framesPerSecond=rate }
                    .font(.caption).frame(minHeight: 44).disabled(upload != nil)
            }
            if source?.animated == true {
                Picker("Animazione",selection: $layout.framesPerSecond) {
                    Text("Eco · 5 fps").tag(5);Text("Fluida · 10 fps").tag(10)
                }.pickerStyle(.segmented).disabled(upload != nil)
            }
        }
    }
    }

    private var sendCard: some View {
    PaperCard {
        Text("Pronto da indossare.").font(.title2.weight(.medium))
        Text("L'anteprima usa gli stessi fotogrammi e colori che invieremo al display. GIF fino a 15 secondi e 3 MB dopo la preparazione.")
            .font(.subheadline).foregroundStyle(Palette.secondary)
        if let message = preparationError ?? error { Label(message,systemImage: "exclamationmark.circle").font(.subheadline).foregroundStyle(.red) }
                if let message=remote?.error, !message.isEmpty { Label(message,systemImage:"exclamationmark.circle").font(.subheadline).foregroundStyle(.red) }
        if connection.ready && connection.status?.badge == nil {
            Label("Aggiorna il firmware dello StopWatch per usare Badge.",systemImage: "arrow.down.circle").font(.subheadline).foregroundStyle(Palette.secondary)
        }
        if let issue=connection.badgeTransferIssue {
            Label("Wi-Fi non riuscito: \(issue) Invio tramite Bluetooth.",systemImage:"wifi.exclamationmark")
                .font(.caption).foregroundStyle(Palette.secondary)
        }
        if let seconds=connection.badgeTransferSeconds {
            Label(String(format:"Ultimo invio: %@ · %.1f s",connection.badgeTransferMethod,seconds),systemImage:"checkmark.circle")
                .font(.caption).foregroundStyle(Palette.secondary)
        }
        if let progress = connection.badgeProgress {
            ProgressView(value: progress).tint(Palette.orange)
            HStack {
                Text(connection.badgeTransferDetail).font(.caption)
                Spacer()
                Button("Annulla") { upload?.cancel() }.font(.caption).frame(minHeight: 44)
            }
        } else {
            Button {
                guard let asset = readyAsset else { return }
                error=nil;sent=false
                upload=Task {
                    do { try await connection.uploadBadge(asset);sent=true }
                    catch is CancellationError { }
                    catch { self.error=error.localizedDescription }
                    upload=nil
                }
            } label: {
                HStack { Text(sent && remote?.digest == readyAsset?.digest ? "Badge inviato" : "Invia allo StopWatch"); Spacer(); Image(systemName: "arrow.up.right") }
                    .font(.headline).padding(.vertical,15).padding(.horizontal,20)
                    .foregroundStyle(.white).background(Palette.ink,in: Capsule())
            }.buttonStyle(.plain).disabled(readyAsset == nil || preparing || !connection.ready || connection.status?.badge == nil || connection.working)
                .opacity(readyAsset == nil || !connection.ready ? 0.45 : 1)
        }
        Button { connection.output("badge.show") } label: { Label("Mostra sul display",systemImage: "circle.inset.filled").frame(maxWidth: .infinity,minHeight: 44) }
            .buttonStyle(.plain).disabled(!connection.ready || connection.working || remote?.available != true)
        if !connection.ready { Text("Puoi prepararlo ora e inviarlo quando lo StopWatch sarà collegato.").font(.caption).foregroundStyle(Palette.secondary) }
    }
    }

    private var behaviorCard: some View {
    PaperCard {
        Label("Si mantiene dritto.",systemImage: "gyroscope").font(.title3.weight(.medium))
        Text("L'IMU segue i movimenti. Quando ti fermi, i sensori rallentano e una foto non viene ridisegnata inutilmente.")
            .font(.subheadline).foregroundStyle(Palette.secondary)
        NavigationLink {
            if let configuration=connection.configuration { ConfigurationEditor(part: .badge,configuration: configuration) }
        } label: { HStack { Text("Rotazione e risparmio energetico");Spacer();Image(systemName: "chevron.right") }.font(.subheadline).frame(minHeight:44) }
            .buttonStyle(.plain).disabled(!connection.ready || connection.working || connection.status?.badge == nil)
    }
    }

    @MainActor private func importImage(_ data: Data) async {
        let id=UUID();importID=id;error=nil
        do {
            let info=try await Task.detached(priority: .userInitiated) { try BadgeAsset.inspect(data) }.value
            try Task.checkCancellation();guard importID==id else { return }
            source=info;sourceData=data;sourceID=id;layout=BadgeLayout();preparationError=nil;sent=false
        } catch is CancellationError { }
        catch { self.error=error.localizedDescription }
    }
}

private struct BadgePreview: View {
    let asset: BadgeAsset?
    let source: BadgeSource?
    @Binding var layout: BadgeLayout
    @GestureState private var magnification: CGFloat=1
    @GestureState private var translation: CGSize = .zero
    private var editing: Bool { magnification != 1 || translation != .zero }

    var body: some View {
        GeometryReader { geometry in
            let side=max(1,geometry.size.width)
            let live=liveLayout(side:side)
            TimelineView(.animation(minimumInterval: 0.1,paused: asset?.animated != true || editing)) { timeline in
                ZStack {
                    Color.black
                    if let asset, !editing, let data=asset.frame(at: timeline.date.timeIntervalSinceReferenceDate), let image=UIImage(data:data) {
                        Image(uiImage:image).resizable().scaledToFit()
                    } else if let source, let image=UIImage(data:source.preview) {
                        let rect=live.rectangle(width:source.width,height:source.height)
                        Image(uiImage:image).resizable().frame(width:rect.width*side/466,height:rect.height*side/466)
                            .position(x:rect.midX*side/466,y:rect.midY*side/466)
                    } else {
                        Image(systemName:"photo.on.rectangle.angled").font(.system(size:60,weight:.light)).foregroundStyle(.white.opacity(0.65))
                    }
                }
                .frame(width:side,height:side).clipShape(Circle())
                .overlay(Circle().strokeBorder(.white.opacity(0.18),lineWidth:1))
            }
            .contentShape(Circle())
            .gesture(DragGesture().updating($translation) { value,state,_ in if source != nil { state=value.translation } }
                .onEnded { value in if source != nil { layout.offsetX=min(128,max(-128,layout.offsetX+Double(value.translation.width/side)));layout.offsetY=min(128,max(-128,layout.offsetY+Double(value.translation.height/side))) } })
            .simultaneousGesture(MagnificationGesture().updating($magnification) { value,state,_ in if source != nil { state=value } }
                .onEnded { value in if source != nil { layout.zoom=min(4,max(1,layout.zoom*Double(value))) } })
            .accessibilityLabel("Anteprima circolare del Badge")
        }.aspectRatio(1,contentMode:.fit)
    }

    private func liveLayout(side:CGFloat) -> BadgeLayout {
        var value=layout
        value.zoom=min(4,max(1,layout.zoom*Double(magnification)))
        value.offsetX+=Double(translation.width/side);value.offsetY+=Double(translation.height/side)
        return value
    }
}
