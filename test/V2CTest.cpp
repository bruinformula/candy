#include <iostream>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <bit>
#include <sstream>
#include <filesystem>

#include "DBC/DBCParser.hpp"
#include "transcoders/V2CTranscoder.hpp"
#include "DBC/DBCParser.hpp"

#include "CAN/frame/FrameIterator.hpp"
#include "CAN/CANHelpers.hpp"

void print_frames(const CAN::FramePacket& fp, CAN::V2CTranscoder& transcoder, int32_t frame_counter) {
	using namespace std::chrono;

	std::cout << "New frame_packet (from " << frame_counter << " frames):" << std::endl;

	for (auto it = CAN::begin(fp); it != CAN::end(fp); ++it) {
		const auto& [ts, frame] = *it;
		auto frame_data = std::bit_cast<int64_t>(frame.data);
		auto t = duration_cast<milliseconds>(ts.time_since_epoch()).count() / 1000.0;

		std::cout << std::fixed
			<< " CANFrame at t: " << t << "s, can_id: " << frame.can_id << std::endl;

		auto msg = transcoder.find_message(frame.can_id);
		for (const auto& sig : msg->signals(frame_data))
			std::cout << "  " << sig.name() << ": " << (int64_t)sig.decode(frame_data) << std::endl;
	}
}

int main() {
	CAN::V2CTranscoder transcoder;

	auto start = std::chrono::system_clock::now();

	bool parsed = transcoder.parse_dbc(CAN::read_file("test/can_land/network.dbc"));
	if (!parsed)
		return 1;

	auto end = std::chrono::system_clock::now();

	std::cout << "Parsed DBC in " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms" << std::endl;

	int32_t frame_counter = 0;

	while (true) {
		// Simulate receiving a frame from CAN bus
		CANFrame frame = CAN::generate_frame();
		frame_counter++;

		auto fp = transcoder.transcode(std::chrono::system_clock::now(), frame);
		
		if (fp.is_empty())
			continue;

		std::cout << frame_counter << std:: endl;


		// Send the aggregated frame_packet to a remote server, store it locally, or process it.
		// This example just prints it to stdout.

		print_frames(fp, transcoder, frame_counter);
		frame_counter = 0;
	}

	return 0;
}
