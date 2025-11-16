
#include "Candy/CAN/CANKernelTypes.hpp"

#include "Candy/DBC/Transcoders/V2C/TransmissionGroup.hpp"

namespace Candy {

	static int32_t millis_diff(CANTime tp, uint32_t utc) {
		using namespace std::chrono;
		return duration_cast<milliseconds>(tp - CANTime(seconds(utc))).count();
	}

	void TransmissionGroup::try_publish(CANTime up_to, FramePacket& fp) {
		if (_group_origin + _assemble_freq <= up_to) {
			if (all_collected())
				publish(up_to, fp);
			_group_origin = up_to;
		}
	}

	void TransmissionGroup::publish(CANTime tp, FramePacket& fp) {
		// for messages with muxed signals, non-muxed signal values should be taken 
		// from the frame with the latest timestamp, indicated by can::use_non_muxed(cf, true)

		std::sort(_msg_clumps.begin(), _msg_clumps.end(), [](const auto& a, const auto& b) {
			if (a.message_id != b.message_id) // primarily by message_ids
				return a.message_id < b.message_id;
			return a.stamp > b.stamp; // secondarily by timestamp, descending
		});

		int64_t prev_id = 0;
		for (const auto& smsg : _msg_clumps) {
			CANFrame cf { 0 };
			cf.can_id = smsg.message_id;
			cf.len = CAN_MAX_DLEN;
			use_non_muxed(cf, prev_id != cf.can_id);
			prev_id = cf.can_id;
			auto raw_data = smsg.mdata;
			std::memcpy(cf.data, &raw_data, CAN_MAX_DLEN);
			fp.append(millis_diff(tp, fp.utc()));
			fp.append(cf);
		}
	}

	bool TransmissionGroup::all_collected() const {
		using namespace std::chrono;
		for (const auto& smsg : _msg_clumps) {
			auto d = duration_cast<milliseconds>(smsg.stamp - _group_origin);
			if (d < milliseconds(0) || d >= _assemble_freq)
				return false;
		}
		return true;
	}

	// used only to assemble messages

	void TransmissionGroup::assign(canid_t message_id, int64_t message_mux) {
		_msg_clumps.push_back({ .message_id = message_id, .message_mux = message_mux });
	}

	bool TransmissionGroup::within_interval(CANTime stamp) const {
		return stamp >= _group_origin && stamp < _group_origin + _assemble_freq;
	}

	void TransmissionGroup::add_clumped(CANTime stamp, canid_t message_id, int64_t message_mux, uint64_t cval) {
		for (auto& smsg : _msg_clumps) {
			if (smsg.message_id == message_id && smsg.message_mux == message_mux) {
				smsg.stamp = stamp; smsg.mdata = cval;
				break;
			}
		}
	}

}