import Foundation
import CoreGraphics
import ImageIO
import UniformTypeIdentifiers
import CryptoKit
import zlib

struct BadgeLayout: Hashable, Sendable {
    var fit = false
    var zoom = 1.0
    var offsetX = 0.0
    var offsetY = 0.0
    var framesPerSecond = 10

    func rectangle(width: Int, height: Int) -> CGRect {
        let scale = (fit ? min(466.0 / Double(width), 466.0 / Double(height)) : max(466.0 / Double(width), 466.0 / Double(height))) * zoom
        let w = Double(width) * scale, h = Double(height) * scale
        let x = min(max(offsetX * 466, -max(0, (w - 466) / 2)), max(0, (w - 466) / 2))
        let y = min(max(offsetY * 466, -max(0, (h - 466) / 2)), max(0, (h - 466) / 2))
        return CGRect(x: (466-w)/2+x, y: (466-h)/2+y, width: w, height: h)
    }
}

struct BadgeSource: Sendable {
    let preview: Data
    let width: Int
    let height: Int
    let animated: Bool
    let milliseconds: Int
}

struct BadgePreviewFrame: Sendable {
    let png: Data
    var milliseconds: Int
}

struct BadgeAsset: Sendable {
    static let side = 466
    static let rawBytes = side * side * 2
    static let maxBytes = 3 * 1024 * 1024
    let data: Data
    let previews: [BadgePreviewFrame]
    let digest: String
    var milliseconds: Int { previews.reduce(0) { $0 + $1.milliseconds } }
    var animated: Bool { previews.count > 1 }

    func frame(at seconds: TimeInterval) -> Data? {
        guard !previews.isEmpty, milliseconds > 0 else { return nil }
        var position = Int(seconds * 1000) % milliseconds
        for frame in previews {
            if position < frame.milliseconds { return frame.png }
            position -= frame.milliseconds
        }
        return previews.last?.png
    }

    private static func source(_ data: Data) throws -> CGImageSource {
        guard !data.isEmpty, data.count <= 25 * 1024 * 1024,
              let source = CGImageSourceCreateWithData(data as CFData, [kCGImageSourceShouldCache: false] as CFDictionary),
              let properties = CGImageSourceCopyPropertiesAtIndex(source, CGImageSourceGetPrimaryImageIndex(source), nil) as? [CFString: Any],
              let width = properties[kCGImagePropertyPixelWidth] as? Int,
              let height = properties[kCGImagePropertyPixelHeight] as? Int,
              width > 0, height > 0, width <= 20000, height <= 20000, width * height <= 64_000_000,
              CGImageSourceGetCount(source) <= 3000 else {
            throw CompanionError.message("Immagine non valida o troppo grande. Scegli una foto o una GIF entro 25 MB.")
        }
        return source
    }

    private static func delays(_ source: CGImageSource) throws -> [Int] {
        guard CGImageSourceGetType(source) as String? == UTType.gif.identifier else { return [1000] }
        let count = CGImageSourceGetCount(source)
        var values: [Int] = []
        for index in 0..<count {
            let properties = CGImageSourceCopyPropertiesAtIndex(source, index, nil) as? [CFString: Any]
            let gif = properties?[kCGImagePropertyGIFDictionary] as? [CFString: Any]
            let seconds = (gif?[kCGImagePropertyGIFUnclampedDelayTime] as? Double) ?? (gif?[kCGImagePropertyGIFDelayTime] as? Double) ?? 0.1
            guard seconds.isFinite, seconds >= 0, seconds <= 15 else { throw CompanionError.message("Durata GIF non valida.") }
            values.append(max(20, Int((seconds == 0 ? 0.1 : seconds) * 1000)))
        }
        if count > 1, values.reduce(0,+) > 15000 { throw CompanionError.message("La GIF può durare al massimo 15 secondi.") }
        return values
    }

    private static func image(_ source: CGImageSource, index: Int) throws -> CGImage {
        let options: [CFString: Any] = [kCGImageSourceCreateThumbnailFromImageAlways: true,
            kCGImageSourceThumbnailMaxPixelSize: 1536, kCGImageSourceCreateThumbnailWithTransform: true,
            kCGImageSourceShouldCacheImmediately: true]
        let selected=CGImageSourceGetType(source) as String? == UTType.gif.identifier ? index : CGImageSourceGetPrimaryImageIndex(source)
        guard let image = CGImageSourceCreateThumbnailAtIndex(source, selected, options as CFDictionary) else {
            throw CompanionError.message("Non riesco a leggere questa immagine.")
        }
        return image
    }

    static func inspect(_ data: Data) throws -> BadgeSource {
        let source = try source(data), delays = try delays(source), first = try image(source, index: 0)
        return BadgeSource(preview: try png(first), width: first.width, height: first.height,
                           animated: delays.count > 1, milliseconds: delays.count > 1 ? delays.reduce(0,+) : 0)
    }

    static func prepare(_ data: Data, layout: BadgeLayout) throws -> BadgeAsset {
        guard layout.zoom.isFinite, (1...4).contains(layout.zoom), layout.offsetX.isFinite, layout.offsetY.isFinite,
              abs(layout.offsetX) <= 128, abs(layout.offsetY) <= 128, [5,10].contains(layout.framesPerSecond) else {
            throw CompanionError.message("Ritaglio non valido.")
        }
        let source = try source(data), delays = try delays(source)
        let duration = delays.count > 1 ? delays.reduce(0,+) : 1000
        let count = delays.count > 1 ? min(120, max(1, duration / (1000 / layout.framesPerSecond))) : 1
        guard delays.count == 1 || count >= 2 else { throw CompanionError.message("Questa GIF è troppo breve per il display. Scegline una da almeno 0,4 secondi.") }
        var compressed: [Data] = [], previews: [BadgePreviewFrame] = [], lastPixels = Data()
        var sourceIndex = 0, sourceEnd = delays[0], payloadBytes = 0
        for index in 0..<count {
            try Task.checkCancellation()
            let timestamp = index * duration / count
            while sourceIndex + 1 < delays.count, timestamp >= sourceEnd { sourceIndex += 1; sourceEnd += delays[sourceIndex] }
            let milliseconds = (index + 1) * duration / count - timestamp
            try autoreleasepool {
                let image = try image(source, index: sourceIndex)
                let pixels = try render(image, layout: layout)
                if pixels == lastPixels, !previews.isEmpty { previews[previews.count-1].milliseconds += milliseconds; return }
                let encoded = try compress(pixels)
                payloadBytes += encoded.count
                guard 16 + (compressed.count+1)*12 + payloadBytes + 32 <= maxBytes else {
                    throw CompanionError.message("Il Badge supera 3 MB. Prova 5 fotogrammi al secondo o una GIF più semplice.")
                }
                compressed.append(encoded)
                previews.append(BadgePreviewFrame(png: try preview(pixels), milliseconds: milliseconds))
                lastPixels = pixels
            }
        }
        let total = 16 + compressed.count * 12 + payloadBytes + 32
        var file = Data("SWB1".utf8)
        file.appendLE(UInt16(side)); file.appendLE(UInt16(side)); file.appendLE(UInt16(compressed.count)); file.appendLE(UInt16(0)); file.appendLE(UInt32(total))
        var offset = 16 + compressed.count * 12
        for (index, frame) in compressed.enumerated() {
            file.appendLE(UInt32(offset)); file.appendLE(UInt32(frame.count)); file.appendLE(UInt32(previews[index].milliseconds)); offset += frame.count
        }
        for frame in compressed { file.append(frame) }
        let hash = Data(SHA256.hash(data: file)); file.append(hash)
        return BadgeAsset(data: file, previews: previews, digest: hash.map { String(format: "%02x",$0) }.joined())
    }

    static func decode(_ data: Data) throws -> BadgeAsset {
        guard data.count >= 61, data.count <= maxBytes, data.prefix(4) == Data("SWB1".utf8),
              data.le16(4) == side, data.le16(6) == side, data.le16(10) == 0, data.le32(12) == data.count else {
            throw CompanionError.message("Badge salvato non valido.")
        }
        let count = data.le16(8)
        guard (1...120).contains(count), 16+count*12+32 < data.count else { throw CompanionError.message("Badge incompleto.") }
        let hash = Data(SHA256.hash(data: data.dropLast(32)))
        guard hash == data.suffix(32) else { throw CompanionError.message("Verifica del Badge fallita.") }
        var previews: [BadgePreviewFrame] = [], end = 16+count*12, duration = 0
        for index in 0..<count {
            try Task.checkCancellation()
            let offset = data.le32(16+index*12), bytes = data.le32(20+index*12), delay = data.le32(24+index*12)
            guard offset == end, bytes > 0, bytes <= rawBytes+65536, (100...15000).contains(delay), offset+bytes <= data.count-32 else {
                throw CompanionError.message("Fotogramma Badge non valido.")
            }
            duration += delay; guard duration <= 15000 else { throw CompanionError.message("Durata Badge non valida.") }
            let pixels = try decompress(data.subdata(in: offset..<offset+bytes))
            previews.append(BadgePreviewFrame(png: try preview(pixels), milliseconds: delay)); end = offset+bytes
        }
        guard end == data.count-32 else { throw CompanionError.message("Dati aggiuntivi nel Badge.") }
        return BadgeAsset(data: data, previews: previews, digest: hash.map { String(format: "%02x",$0) }.joined())
    }

    private static func render(_ image: CGImage, layout: BadgeLayout) throws -> Data {
        var rgba = Data(count: side*side*4)
        let rect = layout.rectangle(width: image.width, height: image.height)
        let drawn = rgba.withUnsafeMutableBytes { buffer -> Bool in
            guard let context = CGContext(data: buffer.baseAddress, width: side, height: side, bitsPerComponent: 8, bytesPerRow: side*4,
                space: CGColorSpace(name:CGColorSpace.sRGB) ?? CGColorSpaceCreateDeviceRGB(), bitmapInfo: CGImageAlphaInfo.noneSkipLast.rawValue | CGBitmapInfo.byteOrder32Big.rawValue) else { return false }
            context.setFillColor(red: 0, green: 0, blue: 0, alpha: 1); context.fill(CGRect(x: 0,y: 0,width: side,height: side))
            context.interpolationQuality = .high
            context.draw(image,in: CGRect(x: rect.minX,y: Double(side)-rect.maxY,width: rect.width,height: rect.height))
            return true
        }
        guard drawn else { throw CompanionError.message("Memoria insufficiente per l'immagine.") }
        var pixels = Data(count: rawBytes)
        rgba.withUnsafeBytes { input in pixels.withUnsafeMutableBytes { output in
            let from = input.bindMemory(to: UInt8.self), to = output.bindMemory(to: UInt8.self)
            for i in 0..<side*side {
                let value = UInt16(from[i*4] & 248)<<8 | UInt16(from[i*4+1] & 252)<<3 | UInt16(from[i*4+2])>>3
                to[i*2] = UInt8(value & 255); to[i*2+1] = UInt8(value>>8)
            }
        } }
        return pixels
    }

    private static func preview(_ pixels: Data) throws -> Data {
        var rgba = Data(count: side*side*4)
        pixels.withUnsafeBytes { input in rgba.withUnsafeMutableBytes { output in
            let from=input.bindMemory(to: UInt8.self), to=output.bindMemory(to: UInt8.self)
            for i in 0..<side*side {
                let value=UInt16(from[i*2]) | UInt16(from[i*2+1])<<8
                to[i*4]=UInt8((value>>11)*255/31); to[i*4+1]=UInt8(((value>>5)&63)*255/63); to[i*4+2]=UInt8((value&31)*255/31); to[i*4+3]=255
            }
        } }
        guard let provider=CGDataProvider(data: rgba as CFData), let image=CGImage(width: side,height: side,bitsPerComponent: 8,bitsPerPixel: 32,bytesPerRow: side*4,
            space: CGColorSpace(name:CGColorSpace.sRGB) ?? CGColorSpaceCreateDeviceRGB(),bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.noneSkipLast.rawValue | CGBitmapInfo.byteOrder32Big.rawValue),
            provider: provider,decode: nil,shouldInterpolate: true,intent: .defaultIntent) else { throw CompanionError.message("Anteprima non disponibile.") }
        return try png(image)
    }

    private static func png(_ image: CGImage) throws -> Data {
        let output=NSMutableData()
        guard let destination=CGImageDestinationCreateWithData(output,UTType.png.identifier as CFString,1,nil) else { throw CompanionError.message("Anteprima non disponibile.") }
        CGImageDestinationAddImage(destination,image,nil)
        guard CGImageDestinationFinalize(destination) else { throw CompanionError.message("Creazione anteprima fallita.") }
        return output as Data
    }

    private static func compress(_ input: Data) throws -> Data {
        var size=compressBound(uLong(input.count)), output=Data(count: Int(compressBound(uLong(input.count))))
        let result=input.withUnsafeBytes { source in output.withUnsafeMutableBytes { target in
            compress2(target.bindMemory(to: UInt8.self).baseAddress,&size,source.bindMemory(to: UInt8.self).baseAddress,uLong(input.count),6)
        } }
        guard result==Z_OK else { throw CompanionError.message("Compressione Badge fallita.") }
        output.count=Int(size); return output
    }
    private static func decompress(_ input: Data) throws -> Data {
        var size=uLongf(rawBytes), output=Data(count: rawBytes)
        let result=input.withUnsafeBytes { source in output.withUnsafeMutableBytes { target in
            uncompress(target.bindMemory(to: UInt8.self).baseAddress,&size,source.bindMemory(to: UInt8.self).baseAddress,uLong(input.count))
        } }
        guard result==Z_OK, size==rawBytes else { throw CompanionError.message("Fotogramma compresso non valido.") }
        return output
    }
}

private extension Data {
    mutating func appendLE<T: FixedWidthInteger>(_ value: T) {
        var little=value.littleEndian
        Swift.withUnsafeBytes(of: &little) { append(contentsOf: $0) }
    }
    func le16(_ offset: Int) -> Int { Int(self[offset]) | Int(self[offset+1])<<8 }
    func le32(_ offset: Int) -> Int { le16(offset) | le16(offset+2)<<16 }
}
