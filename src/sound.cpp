#include "sound.h"
#include "assets.h"

#include <godot_cpp/classes/audio_stream_mp3.hpp>
#include <godot_cpp/classes/audio_stream_ogg_vorbis.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/core/math.hpp>

namespace ssrc {

void SoundSystem::_ready() {
	for (int i = 1; i <= CHANNELS; i++) {
		AudioStreamPlayer *p = memnew(AudioStreamPlayer);
		p->set_name(String("channel") + String::num_int64(i));
		add_child(p);
		channels[i].player = p;
		p->connect("finished", callable_mp(this, &SoundSystem::on_finished).bind(i));
	}
}

static Ref<AudioStream> looped(const Ref<AudioStream> &src, bool loop) {
	if (src.is_null()) {
		return src;
	}
	Ref<AudioStream> s = src->duplicate();
	if (Ref<AudioStreamMP3> mp3 = s; mp3.is_valid()) {
		mp3->set_loop(loop);
	} else if (Ref<AudioStreamOggVorbis> ogg = s; ogg.is_valid()) {
		ogg->set_loop(loop);
	} else if (Ref<AudioStreamWAV> wav = s; wav.is_valid()) {
		wav->set_loop_mode(loop ? AudioStreamWAV::LOOP_FORWARD : AudioStreamWAV::LOOP_DISABLED);
		if (loop) {
			wav->set_loop_end(int(wav->get_length() * wav->get_mix_rate()));
		}
	}
	return s;
}

void SoundSystem::start(int ch, const PlayItem &item) {
	Channel &c = channels[ch];
	Ref<AudioStream> stream = Assets::sound(item.member);
	c.member = item.member;
	if (stream.is_null()) {
		c.member = "";
		return;
	}
	c.loops_left = item.loop_count;
	c.player->set_stream(looped(stream, item.loop_count == 0));
	c.player->set_pitch_scale(Math::pow(2.0, item.rate_shift / 12.0));
	apply_volume(ch);
	c.player->play(item.start_ms / 1000.0);
}

void SoundSystem::play(int ch, const PlayItem &item) {
	Channel &c = channels[ch];
	c.playlist.clear();
	c.fading = false;
	c.stop_after_fade = false;
	start(ch, item);
}

void SoundSystem::queue(int ch, const PlayItem &item) {
	Channel &c = channels[ch];
	if (!c.player->is_playing() && c.member.is_empty()) {
		start(ch, item);
	} else {
		c.playlist.push_back(item);
	}
}

void SoundSystem::stop(int ch) {
	Channel &c = channels[ch];
	c.playlist.clear();
	c.player->stop();
	c.member = "";
	c.fading = false;
}

void SoundSystem::stop_all() {
	for (int i = 1; i <= CHANNELS; i++) {
		stop(i);
	}
}

void SoundSystem::on_finished(int ch) {
	Channel &c = channels[ch];
	if (c.loops_left > 1) {
		c.loops_left--;
		c.player->play();
		return;
	}
	if (!c.playlist.empty()) {
		PlayItem next = c.playlist.front();
		c.playlist.erase(c.playlist.begin());
		start(ch, next);
	} else {
		c.member = "";
	}
}

void SoundSystem::fade_out(int ch, real_t ms) {
	fade_to(ch, 0, ms);
	channels[ch].stop_after_fade = true;
}

void SoundSystem::fade_to(int ch, real_t volume255, real_t ms) {
	Channel &c = channels[ch];
	c.fade_from = c.volume;
	c.fade_to = volume255;
	c.fade_ms = MAX(ms, 1.0);
	c.fade_t = 0;
	c.fading = true;
	c.stop_after_fade = false;
}

void SoundSystem::set_volume(int ch, real_t volume255) {
	channels[ch].volume = volume255;
	channels[ch].fading = false;
	apply_volume(ch);
}

void SoundSystem::set_bus_volumes(real_t music, real_t effects) {
	music_gain = music;
	effects_gain = effects;
	for (int i = 1; i <= CHANNELS; i++) {
		apply_volume(i);
	}
}

void SoundSystem::apply_volume(int ch) {
	Channel &c = channels[ch];
	real_t gain = (ch == 8 ? music_gain : effects_gain) * CLAMP(c.volume / 255.0, 0.0, 2.0);
	c.player->set_volume_db(gain <= 0.0001 ? -80.0 : 20.0 * Math::log(gain) / Math::log(10.0));
}

bool SoundSystem::is_busy(int ch) const {
	return !channels[ch].member.is_empty();
}

String SoundSystem::current_member(int ch) const {
	return channels[ch].member;
}

int SoundSystem::playlist_count(int ch) const {
	return int(channels[ch].playlist.size()) + (channels[ch].member.is_empty() ? 0 : 1);
}

real_t SoundSystem::current_time_ms(int ch) const {
	return channels[ch].player->get_playback_position() * 1000.0;
}

real_t SoundSystem::member_duration_ms(const String &member) {
	Ref<AudioStream> s = Assets::sound(member);
	return s.is_valid() ? s->get_length() * 1000.0 : 0.0;
}

void SoundSystem::_process(double) {
	// Real time, not the (time-scaled) game clock.
	uint64_t now = Time::get_singleton()->get_ticks_usec();
	double delta = last_usec ? (now - last_usec) / 1000000.0 : 0.0;
	last_usec = now;
	for (int i = 1; i <= CHANNELS; i++) {
		Channel &c = channels[i];
		if (!c.fading) {
			continue;
		}
		c.fade_t += delta * 1000.0;
		real_t t = CLAMP(c.fade_t / c.fade_ms, 0.0, 1.0);
		c.volume = Math::lerp(c.fade_from, c.fade_to, t);
		apply_volume(i);
		if (t >= 1.0) {
			c.fading = false;
			if (c.stop_after_fade) {
				stop(i);
				c.volume = 255;
			}
		}
	}
}

} // namespace ssrc
