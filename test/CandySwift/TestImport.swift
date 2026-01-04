@_exported import Candy
import Foundation

@main
struct Testing {
    static func main() {
        print("=== CAN CSV Transcoder Test ===")
        
        print("\n1. Creating CSV transcoder...")
        guard let transcoder = Candy.CSVTranscoderWrapper.create("./test_csv_output_swift/") else {
            print("Failed to create CSVTranscoder.")
            return
        }
        print("   ✓ CSVTranscoder created successfully")
        
        print("\n2. Parsing DBC file...")
        let startParse = Date()
        
        let dbcURL = URL(filePath: "test/network.dbc")
        
        print(dbcURL.absoluteString)

        guard let dbcContent = try? String(contentsOf: dbcURL, encoding: .utf8) else {
            print("Failed to read DBC file.")
            return
        }
        
        let parsed = transcoder.parseDBC(std.string(dbcContent))
        if !parsed {
            print("Failed to parse DBC file.")
            return
        }
        
        let parseTime = Date().timeIntervalSince(startParse) * 1000
        print("   ✓ DBC parsed successfully in \(Int(parseTime)) ms")
        
        print("\n3. Processing CAN frames...")
        let numFrames = 100000
        
        let startProcessing = Date()
        for i in 0..<numFrames {
            var frame = CANFrame()
            frame.can_id = UInt32.random(in: 0x100...0x7FF)
            frame.len = 8
            
            // Fill with random data
            frame.data.0 = UInt8.random(in: 0...255)
            frame.data.1 = UInt8.random(in: 0...255)
            frame.data.2 = UInt8.random(in: 0...255)
            frame.data.3 = UInt8.random(in: 0...255)
            frame.data.4 = UInt8.random(in: 0...255)
            frame.data.5 = UInt8.random(in: 0...255)
            frame.data.6 = UInt8.random(in: 0...255)
            frame.data.7 = UInt8.random(in: 0...255)
                        
            transcoder.receiveRawMessage(.init(first: .clock.now(), second: frame))
        }
        
        let processingTime = Date().timeIntervalSince(startProcessing) * 1000
        let framesPerSecond = Double(numFrames) / (processingTime / 1000.0)
        
        print("\n   Frame generation completed in \(Int(processingTime)) ms")
        print("   Average generation rate: \(String(format: "%.2f", framesPerSecond)) frames/sec")
        
        // Final flush
        print("   Final flush (blocking)...")
        transcoder.flushAllBatches()
        print("   ✓ Test completed successfully")
    }
}
