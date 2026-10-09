// Director-style sound channels (sound(1)..sound(8)) on Godot audio players.
// The game scripts drive audio through channel numbers, playlists, rateShift
// and fades; keeping that model makes the port of carsound etc. one-to-one.
//
// Channel use in Supersonic RC: 1 engine, 2 steering whine, 3 crashes,
// 4 stings/alarms, 8 music.
#pragma once

#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/node.hpp>
#include <vector>

namespace ssrc {

using namespace godot;

struct PlayItem {
	String member;
	int loop_count = 1; // 0 = forever
	real_t rate_shift = 0; // semitones
	real_t start_ms = 0;
};

class SoundSystem : public Node {
	GDCLASS(SoundSystem, Node)

public:
	static constexpr int CHANNELS = 8;

	void play(int ch, const PlayItem &item);
	void queue(int ch, const PlayItem &item);
	void stop(int ch);
	void stop_all();
	void fade_out(int ch, real_t ms);
	void fade_to(int ch, real_t volume255, real_t ms);
	void set_volume(int ch, real_t volume255);
	bool is_busy(int ch) const;
	String current_member(int ch) const;
	int playlist_count(int ch) const;
	real_t current_time_ms(int ch) const;
	static real_t member_duration_ms(const String &member);

	void set_bus_volumes(real_t music, real_t effects);

	void _ready() override;
	void _process(double delta) override;

protected:
	static void _bind_methods() {}

private:
	struct Channel {
		AudioStreamPlayer *player = nullptr;
		String member;
		std::vector<PlayItem> playlist;
		real_t volume = 255;
		real_t fade_from = 255, fade_to = 255, fade_ms = 0, fade_t = 0;
		bool fading = false;
		bool stop_after_fade = false;
		int loops_left = 1;
	};
	Channel channels[CHANNELS + 1];
	real_t music_gain = 1, effects_gain = 1;
	uint64_t last_usec = 0;
	void start(int ch, const PlayItem &item);
	void apply_volume(int ch);
	void on_finished(int ch);
};

} // namespace ssrc
