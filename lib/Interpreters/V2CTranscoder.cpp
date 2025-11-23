#include <numeric>

#include "Candy/Interpreters/V2CTranscoder.hpp"

namespace Candy {
FramePacket V2CTranscoder::transcode(std::pair<CANTime, CANFrame> sample) {
	using namespace std::chrono;

	setup_timers(sample.first);
	
	if (_last_update_tp + update_frequency <= sample.first) {
		store_assembled(_last_update_tp + update_frequency);
		while (_last_update_tp + update_frequency <= sample.first)
			_last_update_tp += update_frequency;
	}

	CANTime frame_begin { seconds(frame_packet.utc()) };
	CANTime frame_end = frame_begin +publish_frequency;

	FramePacket rv {};

	if (sample.first < frame_begin || sample.first >= frame_end) {
		if (!frame_packet.is_empty())
			rv = std::move(frame_packet);
		frame_packet.prepare(duration_cast<seconds>(sample.first.time_since_epoch()).count());
	}

	auto mi = _msgs.find(sample.second.can_id);
	if (mi != _msgs.end()) {
		mi->second.assemble(sample);
	}

	return rv;
}

void V2CTranscoder::setup_timers(CANTime stamp) {
	using namespace std::chrono;

	if (_last_update_tp != CANTime{})
		return;

	frame_packet.prepare(duration_cast<seconds>(stamp.time_since_epoch()).count());
	_last_update_tp = stamp;

	for (auto& txg : transmission_groups)
		txg->time_begin(_last_update_tp);
}

void V2CTranscoder::store_assembled(CANTime up_to) {
	for (auto& txg : transmission_groups)
		txg->try_publish(up_to, frame_packet);
}


TranslatedMessage* V2CTranscoder::find_message(canid_t message_id) {
	auto msg_it = _msgs.find(message_id);
	return msg_it == _msgs.end() ? nullptr : &(msg_it->second);
}

void V2CTranscoder::assign_tx_group(const std::string& object_type, unsigned message_id, const std::string& tx_group) {
	auto grp_it = std::find_if(transmission_groups.begin(), transmission_groups.end(),
		[&](const auto& g) { return g->name() == tx_group; });

	if (grp_it == transmission_groups.end())
		return;

	if (auto msg_ptr = find_message(message_id); msg_ptr)
		msg_ptr->assign_group(grp_it->get(), message_id);
}

void V2CTranscoder::add_signal(canid_t message_id, TranslatedSignal sig) {
	if (auto msg_ptr = find_message(message_id); msg_ptr)
		msg_ptr->add_signal(std::move(sig));
}

void V2CTranscoder::add_muxer(canid_t message_id, TranslatedMultiplexer mux) {
	if (auto msg_ptr = find_message(message_id); msg_ptr)
		msg_ptr->add_muxer(std::move(mux));
}

void V2CTranscoder::add_message(canid_t message_id, std::string_view message_name) {
	_msgs.try_emplace(message_id);
}

void V2CTranscoder::set_env_var(const std::string& name, int64_t ev_value) {
	using namespace std::chrono;

	if (name == "V2CTxTime") {
		publish_frequency = milliseconds(ev_value);
	}
	else if (name.ends_with("GroupTxFreq")) {
		transmission_groups.emplace_back(new TransmissionGroup(name, ev_value));

		auto ufreq = update_frequency / 1ms;
		ufreq = ufreq ? std::gcd(ufreq, ev_value) : ev_value;
		update_frequency = milliseconds(ufreq);
	}
}

void V2CTranscoder::set_sig_val_type(canid_t message_id, const std::string& sig_name, unsigned sig_ext_val_type) {
	if (auto msg_ptr = find_message(message_id); msg_ptr)
		msg_ptr->sig_val_type(sig_name, sig_ext_val_type);
}

void V2CTranscoder::set_sig_agg_type(canid_t message_id, const std::string& sig_name, const std::string& agg_type) {
	if (auto msg_ptr = find_message(message_id); msg_ptr)
		msg_ptr->sig_agg_type(sig_name, agg_type);
}

}
