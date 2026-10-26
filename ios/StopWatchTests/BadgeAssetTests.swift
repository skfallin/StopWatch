import XCTest
import ImageIO
import CryptoKit
import zlib
@testable import StopWatch

final class BadgeAssetTests: XCTestCase {
    private func fixture(_ name:String,_ ext:String) throws -> Data {
        #if SWIFT_PACKAGE
        let bundle=Bundle.module
        #else
        let bundle=Bundle(for:Self.self)
        #endif
        let url=try XCTUnwrap(bundle.url(forResource:name,withExtension:ext,subdirectory:"Fixtures") ?? bundle.url(forResource:name,withExtension:ext))
        return try Data(contentsOf:url)
    }
    private func pixels(_ asset:BadgeAsset,_ frame:Int = 0) throws -> Data {
        func u32(_ n:Int)->Int { (0..<4).reduce(0) { $0 | Int(asset.data[n+$1]) << ($1*8) } }
        let offset=u32(16+frame*12),count=u32(20+frame*12),input=asset.data.subdata(in:offset..<offset+count)
        var output=Data(count:BadgeAsset.rawBytes),size=uLongf(BadgeAsset.rawBytes)
        let result=input.withUnsafeBytes { source in output.withUnsafeMutableBytes { target in
            uncompress(target.bindMemory(to:UInt8.self).baseAddress,&size,source.bindMemory(to:UInt8.self).baseAddress,uLong(input.count))
        } }
        XCTAssertEqual(result,Z_OK);XCTAssertEqual(Int(size),BadgeAsset.rawBytes)
        return output
    }
    private func pixel(_ data:Data,_ x:Int,_ y:Int)->UInt16 {
        let index=(y*466+x)*2
        return UInt16(data[index]) | UInt16(data[index+1])<<8
    }
    func testStaticRoundTripExportsCompatiblePixels() throws {
        let asset=try BadgeAsset.prepare(fixture("quadrants","png"),layout:BadgeLayout())
        XCTAssertEqual(asset.previews.count,1);XCTAssertFalse(asset.animated)
        XCTAssertEqual(asset.data.prefix(4),Data("SWB1".utf8))
        XCTAssertEqual(Data(SHA256.hash(data:asset.data.dropLast(32))),asset.data.suffix(32))
        let raw=try pixels(asset)
        XCTAssertEqual(pixel(raw,5,5),0xf800)
        XCTAssertEqual(pixel(raw,460,5),0x07e0)
        XCTAssertEqual(pixel(raw,5,460),0x001f)
        XCTAssertEqual(pixel(raw,460,460),0xffff)
        let restored=try BadgeAsset.decode(asset.data)
        XCTAssertEqual(restored.digest,asset.digest);XCTAssertEqual(restored.previews.count,1)
        if let path=ProcessInfo.processInfo.environment["STOPWATCH_BADGE_FIXTURE"] { try asset.data.write(to:URL(fileURLWithPath:path),options:.atomic) }
    }
    func testGIFCompositesPartialFramesAndPreservesTiming() throws {
        let source=try fixture("partial","gif")
        let info=try BadgeAsset.inspect(source)
        XCTAssertTrue(info.animated);XCTAssertEqual(info.milliseconds,600)
        let asset=try BadgeAsset.prepare(source,layout:BadgeLayout())
        XCTAssertEqual(asset.previews.count,3);XCTAssertEqual(asset.milliseconds,600)
        XCTAssertEqual(asset.previews.map(\.milliseconds),[100,200,300])
        let second=try pixels(asset,1),third=try pixels(asset,2)
        XCTAssertEqual(pixel(second,5,5),0xf800)
        XCTAssertEqual(pixel(second,150,150),0x0400)
        XCTAssertEqual(pixel(third,150,150),0xf800)
        XCTAssertEqual(pixel(third,300,300),0x001f)
        XCTAssertEqual(try BadgeAsset.decode(asset.data).milliseconds,600)
    }
    func testEXIFOrientationAndFitLetterboxing() throws {
        let source=try fixture("rotated","jpg"), info=try BadgeAsset.inspect(source)
        XCTAssertEqual(info.width,16);XCTAssertEqual(info.height,32)
        var layout=BadgeLayout();layout.fit=true
        let raw=try pixels(BadgeAsset.prepare(source,layout:layout))
        XCTAssertEqual(pixel(raw,5,233),0)
        XCTAssertEqual(pixel(raw,233,233)&0xf800,0xf800)
    }
    func testCropVerticalDirectionMatchesPhonePreview() throws {
        var layout=BadgeLayout();layout.zoom=2;layout.offsetY=0.25
        let raw=try pixels(BadgeAsset.prepare(fixture("quadrants","png"),layout:layout))
        XCTAssertEqual(pixel(raw,100,233),0xf800)
    }
    func testInvalidAndCorruptedAssetsAreRejected() throws {
        XCTAssertThrowsError(try BadgeAsset.inspect(Data("not an image".utf8)))
        let original=try BadgeAsset.prepare(fixture("quadrants","png"),layout:BadgeLayout())
        var broken=original.data;broken[broken.count-1]^=1
        XCTAssertThrowsError(try BadgeAsset.decode(broken))
        XCTAssertThrowsError(try BadgeAsset.decode(original.data.dropLast(40)))
        XCTAssertThrowsError(try BadgeAsset.decode(Data(repeating:0,count:BadgeAsset.maxBytes+1)))
        var layout=BadgeLayout();layout.zoom = .nan
        XCTAssertThrowsError(try BadgeAsset.prepare(fixture("quadrants","png"),layout:layout))
        layout=BadgeLayout();layout.framesPerSecond=60
        XCTAssertThrowsError(try BadgeAsset.prepare(fixture("quadrants","png"),layout:layout))
    }
    func testPhoneEditsPreserveIndependentBadgeAndFeedbackSettings() {
        var current=DeviceConfiguration();current.badge.keepAwake=true;current.soundPack="glass";current.colorTheme="buttons"
        var draft=DeviceConfiguration();draft.ssid="Casa"
        current.apply(draft,part:.wifi)
        XCTAssertTrue(current.badge.keepAwake);XCTAssertEqual(current.soundPack,"glass");XCTAssertEqual(current.colorTheme,"buttons")
        draft.badge.autoRotate=false;current.apply(draft,part:.badge)
        XCTAssertFalse(current.badge.autoRotate);XCTAssertEqual(current.soundPack,"glass");XCTAssertEqual(current.ssid,"Casa")
    }
}
