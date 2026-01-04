@_exported import Candy

@main
struct Testing {
    static func main() {
        //var frame = CANFrame()
        let message = Candy.CANMessage()
        guard let transcoder = Candy.SQLTranscoderWrapper.create("./test.db") else {
            print("Failed to create SQL transcoder")
            return
        }

        let newTranscoder = transcoder

        print("Successfully created SQL transcoder: \(newTranscoder)")
        print(message)
    }
}
