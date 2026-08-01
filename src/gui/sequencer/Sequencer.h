#pragma once

#include "imgui_neo_sequencer.h"
#include "ofMain.h"
#include "ofxEasingFunc.h"
#include <algorithm>
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  Data types
// ─────────────────────────────────────────────────────────────────────────────

enum class EaseType { Linear, EaseIn, EaseOut, EaseInOut };

// ─────────────────────────────────────────────────────────────────────────────
//  Event types
// ─────────────────────────────────────────────────────────────────────────────

enum class SequencerFrameChangeSource
{
	Playback,
	Scrub,
	Jump
};

enum class SequencerPlaybackDirection
{
	Forward,
	Backward
};

struct Keyframe
{
	std::string id;
	std::string name = "";
	ImGui::FrameIndexType frame = 0;
	float value = 0.0f;
	EaseType ease = EaseType::Linear;
};

struct Track
{
	std::string id;
	std::string name = "Track";
	bool open = true;
	std::vector<Keyframe> keys; // kept sorted by frame
};

struct SequencerKeyframeEvent
{
	std::string trackId;
	std::string trackName;
	std::string keyframeId;
	std::string keyframeName;

	int trackIndex = -1;
	int keyIndex   = -1;

	ImGui::FrameIndexType previousFrame = 0;
	ImGui::FrameIndexType currentFrame  = 0;
	ImGui::FrameIndexType keyFrame      = 0;

	float    value = 0.0f;
	EaseType ease  = EaseType::Linear;

	SequencerPlaybackDirection direction = SequencerPlaybackDirection::Forward;
	SequencerFrameChangeSource source    = SequencerFrameChangeSource::Playback;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Frame evaluation types
// ─────────────────────────────────────────────────────────────────────────────

struct TrackEvaluationResult
{
	float value = 0.0f;

	int prevKeyIndex = -1;
	int nextKeyIndex = -1;

	ImGui::FrameIndexType prevFrame = 0;
	ImGui::FrameIndexType nextFrame = 0;

	float normalizedT = 0.0f; // easing 適用前の 0..1
	float easedT      = 0.0f; // easing 適用後の 0..1

	EaseType ease = EaseType::Linear;
};

struct SequencerTrackEvaluation
{
	std::string trackId;
	std::string trackName;
	int trackIndex = -1;

	ImGui::FrameIndexType frame = 0;

	float value = 0.0f;

	std::string prevKeyframeId;
	std::string nextKeyframeId;

	int prevKeyIndex = -1;
	int nextKeyIndex = -1;

	ImGui::FrameIndexType prevKeyFrame = 0;
	ImGui::FrameIndexType nextKeyFrame = 0;

	float normalizedT = 0.0f;
	float easedT      = 0.0f;

	EaseType ease = EaseType::Linear;
};

struct SequencerFrameEvaluatedEvent
{
	ImGui::FrameIndexType previousFrame = 0;
	ImGui::FrameIndexType currentFrame  = 0;

	float timeSec = 0.0f;
	float fps     = 30.0f;

	SequencerFrameChangeSource source = SequencerFrameChangeSource::Playback;

	std::vector<SequencerTrackEvaluation> tracks;
};

struct SequencerEndReachedEvent
{
	ImGui::FrameIndexType frame = 0;
	SequencerFrameChangeSource source = SequencerFrameChangeSource::Playback;
};

struct SeqTimeline
{
	ImGui::FrameIndexType currentFrame = 0;
	ImGui::FrameIndexType startFrame = 0;
	ImGui::FrameIndexType endFrame = 300;
	float fps = 30.0f;
	std::vector<Track> tracks;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Free helpers (inline to avoid ODR violations)
// ─────────────────────────────────────────────────────────────────────────────

inline void sortTrack(Track& track)
{
	std::sort(track.keys.begin(), track.keys.end(),
		[](const Keyframe& a, const Keyframe& b) { return a.frame < b.frame; });
}

inline float applyEase(float t, EaseType ease)
{
	switch (ease)
	{
		case EaseType::EaseIn:    return ofxEasingFunc::Cubic::easeIn(t);
		case EaseType::EaseOut:   return ofxEasingFunc::Cubic::easeOut(t);
		case EaseType::EaseInOut: return ofxEasingFunc::Cubic::easeInOut(t);
		default: return t;
	}
}

inline TrackEvaluationResult evaluateTrackDetailed(const Track& track, float frame)
{
	TrackEvaluationResult r;

	if (track.keys.empty())
		return r;

	if (frame <= static_cast<float>(track.keys.front().frame))
	{
		const auto& k  = track.keys.front();
		r.value        = k.value;
		r.prevKeyIndex = 0;
		r.nextKeyIndex = 0;
		r.prevFrame    = k.frame;
		r.nextFrame    = k.frame;
		r.normalizedT  = 0.0f;
		r.easedT       = 0.0f;
		r.ease         = k.ease;
		return r;
	}

	if (frame >= static_cast<float>(track.keys.back().frame))
	{
		const int   idx = static_cast<int>(track.keys.size()) - 1;
		const auto& k   = track.keys.back();
		r.value         = k.value;
		r.prevKeyIndex  = idx;
		r.nextKeyIndex  = idx;
		r.prevFrame     = k.frame;
		r.nextFrame     = k.frame;
		r.normalizedT   = 1.0f;
		r.easedT        = 1.0f;
		r.ease          = k.ease;
		return r;
	}

	for (size_t i = 0; i + 1 < track.keys.size(); ++i)
	{
		const Keyframe& a = track.keys[i];
		const Keyframe& b = track.keys[i + 1];

		if (static_cast<float>(a.frame) <= frame && frame <= static_cast<float>(b.frame))
		{
			r.prevKeyIndex = static_cast<int>(i);
			r.nextKeyIndex = static_cast<int>(i + 1);
			r.prevFrame    = a.frame;
			r.nextFrame    = b.frame;
			r.ease         = a.ease;

			if (a.frame == b.frame)
			{
				r.value       = b.value;
				r.normalizedT = 1.0f;
				r.easedT      = 1.0f;
				return r;
			}

			r.normalizedT =
				(frame - static_cast<float>(a.frame)) /
				static_cast<float>(b.frame - a.frame);

			r.easedT = applyEase(r.normalizedT, a.ease);
			r.value  = a.value + r.easedT * (b.value - a.value);
			return r;
		}
	}

	return r;
}

inline float evaluateTrack(const Track& track, float frame)
{
	return evaluateTrackDetailed(track, frame).value;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Sequencer
// ─────────────────────────────────────────────────────────────────────────────

class Sequencer
{
public:

	SeqTimeline timeline;
	inline static ofEvent<SequencerKeyframeEvent>       keyframeEvent;
	inline static ofEvent<SequencerFrameEvaluatedEvent>  frameEvaluatedEvent;
	inline static ofEvent<SequencerEndReachedEvent>      endReachedEvent;

	Sequencer()
	{
		timeline.tracks.push_back({makeTrackId(), "Track 1", true, {}});
		loadTimeline();
	}

	void setEmitEventsDuringPlayback(bool enabled) { emitEventsDuringPlayback_ = enabled; }
	void setEmitEventsDuringScrub(bool enabled)    { emitEventsDuringScrub_    = enabled; }

	void setEmitFrameEvaluatedDuringPlayback(bool enabled) { emitFrameEvaluatedDuringPlayback_ = enabled; }
	void setEmitFrameEvaluatedDuringScrub(bool enabled)    { emitFrameEvaluatedDuringScrub_    = enabled; }

	void step()
	{
		const auto frameBefore = timeline.currentFrame;
		timeline.currentFrame = std::min(timeline.currentFrame + ImGui::FrameIndexType(1), timeline.endFrame);
		const auto frameAfter = timeline.currentFrame;
		if (frameAfter != frameBefore)
		{
			if (shouldEmitFrameEvaluatedEvent(SequencerFrameChangeSource::Playback))
				emitFrameEvaluated(frameBefore, frameAfter, SequencerFrameChangeSource::Playback);
			if (shouldEmitKeyframeEvents(SequencerFrameChangeSource::Playback))
				emitKeyframesPassed(frameBefore, frameAfter, SequencerFrameChangeSource::Playback);
			if (frameAfter == timeline.endFrame)
				emitEndReached(SequencerFrameChangeSource::Playback);
		}
	}

	// Pull API: evaluate all tracks at an arbitrary frame without firing events
	std::vector<SequencerTrackEvaluation> evaluateAllTracks(float frame) const
	{
		std::vector<SequencerTrackEvaluation> result;
		result.reserve(timeline.tracks.size());

		for (int i = 0; i < static_cast<int>(timeline.tracks.size()); ++i)
		{
			const Track& track = timeline.tracks[i];
			TrackEvaluationResult eval = evaluateTrackDetailed(track, frame);

			SequencerTrackEvaluation out;
			out.trackId      = track.id;
			out.trackName    = track.name;
			out.trackIndex   = i;
			out.frame        = static_cast<ImGui::FrameIndexType>(frame);
			out.value        = eval.value;
			out.prevKeyIndex = eval.prevKeyIndex;
			out.nextKeyIndex = eval.nextKeyIndex;
			out.prevKeyFrame = eval.prevFrame;
			out.nextKeyFrame = eval.nextFrame;
			out.normalizedT  = eval.normalizedT;
			out.easedT       = eval.easedT;
			out.ease         = eval.ease;

			if (eval.prevKeyIndex >= 0 && eval.prevKeyIndex < static_cast<int>(track.keys.size()))
				out.prevKeyframeId = track.keys[eval.prevKeyIndex].id;

			if (eval.nextKeyIndex >= 0 && eval.nextKeyIndex < static_cast<int>(track.keys.size()))
				out.nextKeyframeId = track.keys[eval.nextKeyIndex].id;

				result.push_back(std::move(out));
				}

				return result;
			}

			// Pull API: get all keyframes for a named track (nullptr if track not found)
			const std::vector<Keyframe>* getKeyframesByTrackName(const std::string& trackName) const
			{
				for (const auto& track : timeline.tracks)
				{
					if (track.name == trackName)
						return &track.keys;
				}
				return nullptr;
			}

	void draw()
	{
		const auto frameBefore    = timeline.currentFrame;
		const bool wasPlayingBefore = playing_;

		advanceTransport();
		handleGlobalKeys();
		drawSequencerWindow();

		const auto frameAfter = timeline.currentFrame;

		if (frameAfter != frameBefore)
		{
			SequencerFrameChangeSource source = wasPlayingBefore
				? SequencerFrameChangeSource::Playback
				: SequencerFrameChangeSource::Scrub;

			if (shouldEmitFrameEvaluatedEvent(source))
				emitFrameEvaluated(frameBefore, frameAfter, source);

			if (shouldEmitKeyframeEvents(source))
				emitKeyframesPassed(frameBefore, frameAfter, source);
		}

		drawInspectorWindow();
	}

	const float get_fps() const { return timeline.fps; }
	const int get_current_frame() const { return timeline.currentFrame; }

private:

	// ID generation
	uint64_t nextTrackId_    = 1;
	uint64_t nextKeyframeId_ = 1;

	std::string makeTrackId()    { return "track_" + std::to_string(nextTrackId_++); }
	std::string makeKeyframeId() { return "key_"   + std::to_string(nextKeyframeId_++); }

	// Event flags — keyframeEvent
	bool emitEventsDuringPlayback_ = true;
	bool emitEventsDuringScrub_    = false;

	// Event flags — frameEvaluatedEvent
	bool emitFrameEvaluatedDuringPlayback_ = true;
	bool emitFrameEvaluatedDuringScrub_    = true;

	// Transport
	bool  playing_   = false;
	float accumTime_ = 0.0f; // seconds from play start

	// Selection
	Keyframe* selectedKey_      = nullptr;
	int       selectedTrackIdx_ = 0; // default: first track

	// Deferred flags
	bool pendingDelete_ = false;
	bool wasDragging_   = false;

	// Track name editing
	char trackNameBuf_[256]  = {};
	int  nameEditTrackIdx_   = -2; // -2 = unsynced sentinel

	// Keyframe name editing
	char      keyNameBuf_[256] = {};
	Keyframe* nameEditKey_     = nullptr;

	// ── Transport ─────────────────────────────────────────────────────────────

	void advanceTransport()
	{
		if (!playing_) return;
		accumTime_ += (float)ofGetLastFrameTime();
		auto newFrame = timeline.startFrame
			+ (ImGui::FrameIndexType)std::floor(accumTime_ * timeline.fps);
		const bool hitEnd = newFrame >= timeline.endFrame;
		if (hitEnd)
		{
			newFrame = timeline.endFrame;
			playing_ = false;
		}
		const auto frameBefore = timeline.currentFrame;
		timeline.currentFrame  = newFrame;
		if (hitEnd && newFrame != frameBefore)
			emitEndReached(SequencerFrameChangeSource::Playback);
	}

	void play()
	{
		accumTime_ = (float)(timeline.currentFrame - timeline.startFrame) / timeline.fps;
		playing_   = true;
	}

	void pause() { playing_ = false; }

	void stop()
	{
		playing_              = false;
		timeline.currentFrame = timeline.startFrame;
		accumTime_            = 0.0f;
	}

	// ── Event helpers ────────────────────────────────────────────────────────

	bool shouldEmitKeyframeEvents(SequencerFrameChangeSource source) const
	{
		switch (source)
		{
		case SequencerFrameChangeSource::Playback: return emitEventsDuringPlayback_;
		case SequencerFrameChangeSource::Scrub:    return emitEventsDuringScrub_;
		case SequencerFrameChangeSource::Jump:     return false;
		default:                                   return false;
		}
	}

	bool shouldEmitFrameEvaluatedEvent(SequencerFrameChangeSource source) const
	{
		switch (source)
		{
		case SequencerFrameChangeSource::Playback: return emitFrameEvaluatedDuringPlayback_;
		case SequencerFrameChangeSource::Scrub:    return emitFrameEvaluatedDuringScrub_;
		case SequencerFrameChangeSource::Jump:     return false;
		default:                                   return false;
		}
	}

	void emitEndReached(SequencerFrameChangeSource source)
	{
		SequencerEndReachedEvent e;
		e.frame  = timeline.endFrame;
		e.source = source;
		ofNotifyEvent(endReachedEvent, e, this);
	}

	void emitFrameEvaluated(
		ImGui::FrameIndexType previousFrame,
		ImGui::FrameIndexType currentFrame,
		SequencerFrameChangeSource source)
	{
		SequencerFrameEvaluatedEvent e;
		e.previousFrame = previousFrame;
		e.currentFrame  = currentFrame;
		e.fps           = timeline.fps;
		e.timeSec       = timeline.fps > 0.0f
			? static_cast<float>(currentFrame - timeline.startFrame) / timeline.fps
			: 0.0f;
		e.source = source;
		e.tracks = evaluateAllTracks(static_cast<float>(currentFrame));

		ofNotifyEvent(frameEvaluatedEvent, e, this);
	}

	void emitKeyframesPassed(
		ImGui::FrameIndexType prevFrame,
		ImGui::FrameIndexType currFrame,
		SequencerFrameChangeSource source)
	{
		if (prevFrame == currFrame) return;

		const bool forward = currFrame > prevFrame;

		for (int ti = 0; ti < static_cast<int>(timeline.tracks.size()); ++ti)
		{
			const Track& track = timeline.tracks[ti];

			for (int ki = 0; ki < static_cast<int>(track.keys.size()); ++ki)
			{
				const Keyframe& key = track.keys[ki];

				bool crossed = false;
				if (forward)
					crossed = prevFrame < key.frame && key.frame <= currFrame;
				else
					crossed = currFrame <= key.frame && key.frame < prevFrame;

				if (!crossed) continue;

				SequencerKeyframeEvent e;
				e.trackId       = track.id;
				e.trackName     = track.name;
				e.keyframeId    = key.id;
				e.keyframeName  = key.name;
				e.trackIndex    = ti;
				e.keyIndex      = ki;
				e.previousFrame = prevFrame;
				e.currentFrame  = currFrame;
				e.keyFrame      = key.frame;
				e.value         = key.value;
				e.ease          = key.ease;
				e.direction     = forward
					? SequencerPlaybackDirection::Forward
					: SequencerPlaybackDirection::Backward;
				e.source = source;

				ofNotifyEvent(keyframeEvent, e, this);
			}
		}
	}

	// ── Key bindings ──────────────────────────────────────────────────────────

	void handleGlobalKeys()
	{
		// Suppress hotkeys while any text field (e.g. track name) has keyboard focus
		if (ImGui::GetIO().WantTextInput) return;

		// T: add keyframe to selected track
		if (ImGui::IsKeyPressed(ImGuiKey_T)
			&& selectedTrackIdx_ >= 0
			&& selectedTrackIdx_ < (int)timeline.tracks.size())
		{
			addKeyframeToTrack(timeline.tracks[selectedTrackIdx_], timeline.currentFrame);
		}

		// X: mark selection for deletion
		if (ImGui::IsKeyPressed(ImGuiKey_X))
			pendingDelete_ = true;

		// Space: play / pause toggle
		if (ImGui::IsKeyPressed(ImGuiKey_Space))
			playing_ ? pause() : play();
	}

	// ── Keyframe helpers ──────────────────────────────────────────────────────

	void addKeyframeToTrack(Track& track, ImGui::FrameIndexType frame)
	{
		// Duplicate-frame guard
		for (const auto& k : track.keys)
			if (k.frame == frame) return;

		float val = evaluateTrack(track, (float)frame);
		track.keys.push_back({makeKeyframeId(), "", frame, val, EaseType::Linear});
		sortTrack(track);
		selectedKey_ = nullptr; // pointer invalidated by sort
	}

	// ── Draw: sequencer window ────────────────────────────────────────────────

	void drawSequencerWindow()
	{
		// Minimum height: toolbar (~45px) + sequencer header (~28px) + per-track (~26px) + padding (~20px)
		const float minH = 130.0f + (float)timeline.tracks.size() * 24.0f;
		ImGui::SetNextWindowSize(ImVec2(900.0f, std::max(500.0f, minH)), ImGuiCond_FirstUseEver);
		ImGui::Begin("Sequencer");

		// Force minimum height every frame — SetWindowSize() overwrites the stored size
		// immediately, so this also fixes windows loaded from imgui.ini that are too small.
		{
			ImVec2 sz = ImGui::GetWindowSize();
			if (sz.y < minH)
				ImGui::SetWindowSize(ImVec2(sz.x, minH));
		}

		drawToolbar();

		ImGuiNeoSequencerFlags flags =
			ImGuiNeoSequencerFlags_AllowLengthChanging
			| ImGuiNeoSequencerFlags_EnableSelection
			//| ImGuiNeoSequencerFlags_HideZoom
			| ImGuiNeoSequencerFlags_AlwaysShowHeader
			| ImGuiNeoSequencerFlags_Selection_EnableDragging
			| ImGuiNeoSequencerFlags_Selection_EnableDeletion;

		// BeginNeoSequencer calls BeginChild() internally *without* an explicit size,
		// so the inner child fills GetContentRegionAvail() at call time. If the outer
		// window is momentarily small (e.g. first frame, or imgui.ini size), that child
		// is also small and tracks get clipped. Fix: pre-size a host BeginChild so the
		// inner child always gets the correct height regardless of window state.
		// Note: the library also subtracts GetFontSize()*ZoomHeightScale from the clip
		// rect bottom even with HideZoom set, so the 60px base includes that buffer.
		const float seqH = 60.0f + (float)timeline.tracks.size() * 24.0f;
		ImGui::BeginChild("##seq_host", ImVec2(0.0f, seqH), false,
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground);

		if (ImGui::BeginNeoSequencer("MySequencer",
				&timeline.currentFrame, &timeline.startFrame, &timeline.endFrame,
				ImVec2(0, 0), flags))
		{
			// Re-detect selection each frame from ImGui state
			Keyframe* newSelectedKey      = nullptr;
			int       newSelectedTrackIdx = selectedTrackIdx_;
			bool      didDelete           = false;

			for (int i = 0; i < (int)timeline.tracks.size(); ++i)
			{
				Track& track = timeline.tracks[i];

				if (ImGui::BeginNeoTimelineEx(track.name.c_str(), &track.open))
				{
					// Track-level selection
					if (ImGui::IsNeoTimelineSelected())
						newSelectedTrackIdx = i;

					// Draw keyframes
					for (auto& key : track.keys)
					{
						ImGui::NeoKeyframe(&key.frame);
						if (ImGui::IsNeoKeyframeSelected())
						{
							newSelectedKey      = &key;
							newSelectedTrackIdx = i;
						}
					}

					// Deletion — collect per-timeline, erase from track.keys
					if (pendingDelete_ && ImGui::NeoCanDeleteSelection())
					{
						uint32_t selSize = ImGui::GetNeoKeyframeSelectionSize();
						if (selSize > 0)
						{
							std::vector<ImGui::FrameIndexType> toDelete(selSize);
							ImGui::GetNeoKeyframeSelection(toDelete.data());
							for (auto f : toDelete)
							{
								track.keys.erase(
									std::remove_if(track.keys.begin(), track.keys.end(),
										[f](const Keyframe& k) { return k.frame == f; }),
									track.keys.end());
							}
							didDelete = true;
						}
					}

					ImGui::EndNeoTimeLine();
				}
			}

			// Post-loop: clear selection and reset flags
			if (didDelete)
			{
				ImGui::NeoClearSelection();
				newSelectedKey = nullptr;
			}
			pendingDelete_ = false;

			// Sort after drag ends — drag uses raw pointer addresses as IDs, so
			// sort only after dragging stops, then clear stale selection.
			bool isDragging = ImGui::NeoIsDraggingSelection();
			if (wasDragging_ && !isDragging)
			{
				for (auto& track : timeline.tracks) sortTrack(track);
				ImGui::NeoClearSelection();
				newSelectedKey = nullptr;
			}
			wasDragging_ = isDragging;

			selectedKey_      = newSelectedKey;
			selectedTrackIdx_ = newSelectedTrackIdx;

			ImGui::EndNeoSequencer();
		}

		ImGui::EndChild(); // ##seq_host
		ImGui::End();
	}

	void drawToolbar()
	{
		// Transport
		if (ImGui::Button(playing_ ? "Pause" : "Play ")) playing_ ? pause() : play();
		ImGui::SameLine();
		if (ImGui::Button("Stop")) stop();
		ImGui::SameLine();
		ImGui::Text("Frame: %d", (int)timeline.currentFrame);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(60.0f);
		float prevFps = timeline.fps;
		if (ImGui::DragFloat("FPS", &timeline.fps, 0.5f, 1.0f, 120.0f, "%.0f"))
		{
			// Preserve duration in seconds when FPS changes
			if (prevFps > 0.0f)
			{
				float dur = (float)(timeline.endFrame - timeline.startFrame) / prevFps;
				timeline.endFrame = timeline.startFrame
					+ (ImGui::FrameIndexType)std::max(1.0f, std::round(dur * timeline.fps));
				if (timeline.currentFrame > timeline.endFrame)
					timeline.currentFrame = timeline.endFrame;
			}
		}

		ImGui::SameLine();
		float dur = (float)(timeline.endFrame - timeline.startFrame) / timeline.fps;
		ImGui::SetNextItemWidth(80.0f);
		if (ImGui::DragFloat("Dur(s)", &dur, 0.1f, 0.01f, 3600.0f, "%.2fs"))
		{
			timeline.endFrame = timeline.startFrame
				+ (ImGui::FrameIndexType)std::max(1.0f, std::round(dur * timeline.fps));
			if (timeline.currentFrame > timeline.endFrame)
				timeline.currentFrame = timeline.endFrame;
		}

		// Track management
		ImGui::SameLine();
		if (ImGui::Button("+ Track"))
		{
			std::string name = "Track " + std::to_string(timeline.tracks.size() + 1);
			timeline.tracks.push_back({makeTrackId(), name, true, {}});
		}
		ImGui::SameLine();
		if (ImGui::Button("- Track")
			&& selectedTrackIdx_ >= 0
			&& selectedTrackIdx_ < (int)timeline.tracks.size())
		{
			timeline.tracks.erase(timeline.tracks.begin() + selectedTrackIdx_);
			selectedTrackIdx_ = (int)timeline.tracks.size() > 0
				? std::min(selectedTrackIdx_, (int)timeline.tracks.size() - 1)
				: 0;
			selectedKey_ = nullptr;
		}

		// Persistence
		ImGui::SameLine();
		if (ImGui::Button("Save")) saveTimeline();
		ImGui::SameLine();
		if (ImGui::Button("Load"))
		{
			loadTimeline();
			selectedKey_      = nullptr;
			selectedTrackIdx_ = 0;
			nameEditTrackIdx_ = -2;  // force buffer re-sync after load
			nameEditKey_      = nullptr; // force buffer re-sync after load
		}

		// Hint
		ImGui::SameLine();
		ImGui::TextDisabled("  T: add key   X: delete   Space: play");
	}

	// ── Draw: inspector window ────────────────────────────────────────────────

	void drawInspectorWindow()
	{
		ImGui::SetNextWindowSize(ImVec2(280, 220), ImGuiCond_FirstUseEver);
		ImGui::Begin("Key Inspector");

		const bool validTrack = selectedTrackIdx_ >= 0
							 && selectedTrackIdx_ < (int)timeline.tracks.size();

		// ── Track name section (always visible when a track is selected) ──────
		if (validTrack)
		{
			Track& track = timeline.tracks[selectedTrackIdx_];

			// Re-sync buffer when selected track changes
			if (nameEditTrackIdx_ != selectedTrackIdx_)
			{
				snprintf(trackNameBuf_, sizeof(trackNameBuf_), "%s", track.name.c_str());
				nameEditTrackIdx_ = selectedTrackIdx_;
			}

			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::InputText("##trackname", trackNameBuf_, sizeof(trackNameBuf_)))
				track.name = trackNameBuf_;

			ImGui::Separator();
		}

		// ── Keyframe section ─────────────────────────────────────────────────
		if (selectedKey_)
		{
			// Re-sync buffer when selected key changes
			if (nameEditKey_ != selectedKey_)
			{
				snprintf(keyNameBuf_, sizeof(keyNameBuf_), "%s", selectedKey_->name.c_str());
				nameEditKey_ = selectedKey_;
			}
			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::InputText("##keyname", keyNameBuf_, sizeof(keyNameBuf_)))
				selectedKey_->name = keyNameBuf_;

			// Frame (re-sort on change)
			int frame = (int)selectedKey_->frame;
			if (ImGui::InputInt("Frame", &frame))
			{
				selectedKey_->frame = (ImGui::FrameIndexType)frame;
				if (validTrack)
					sortTrack(timeline.tracks[selectedTrackIdx_]);
				selectedKey_ = nullptr; // pointer invalidated by sort
			}

			if (selectedKey_)
			{
				ImGui::DragFloat("Value", &selectedKey_->value, 0.01f);

				const char* easeNames[] = {"Linear", "EaseIn", "EaseOut", "EaseInOut"};
				int easeIdx = (int)selectedKey_->ease;
				if (ImGui::Combo("Ease", &easeIdx, easeNames, 4))
					selectedKey_->ease = (EaseType)easeIdx;
			}

			// Evaluated value at current frame
			if (validTrack)
			{
				float val = evaluateTrack(
					timeline.tracks[selectedTrackIdx_], (float)timeline.currentFrame);
				ImGui::Separator();
				ImGui::Text("Eval @ frame %d:  %.4f", (int)timeline.currentFrame, val);
			}
		}
		else
		{
			ImGui::TextDisabled("No keyframe selected");
			ImGui::Spacing();
			ImGui::TextDisabled("T     :  add key at current frame");
			ImGui::TextDisabled("X     :  delete selected keys");
			ImGui::TextDisabled("Space :  play / pause");
		}

		ImGui::End();
	}

	// ── JSON ──────────────────────────────────────────────────────────────────

	void saveTimeline()
	{
		ofJson j;
		j["fps"]          = timeline.fps;
		j["startFrame"]   = timeline.startFrame;
		j["endFrame"]     = timeline.endFrame;
		j["currentFrame"] = timeline.currentFrame;
		j["tracks"]       = ofJson::array();

		for (const auto& track : timeline.tracks)
		{
			ofJson jt;
			jt["name"] = track.name;
			jt["open"] = track.open;
			jt["keys"] = ofJson::array();
			for (const auto& key : track.keys)
			{
				ofJson jk;
				jk["name"]  = key.name;
				jk["frame"] = key.frame;
				jk["value"] = key.value;
				jk["ease"]  = (int)key.ease;
				jt["keys"].push_back(jk);
			}
			j["tracks"].push_back(jt);
		}

		ofSavePrettyJson(ofToDataPath("timeline.json"), j);
	}

	void loadTimeline()
	{
		std::string path = ofToDataPath("timeline.json");
		if (!ofFile(path).exists()) return;

		ofJson j = ofLoadJson(path);
		if (j.is_null() || j.empty()) return;

		timeline.fps          = j.value("fps", 30.0f);
		timeline.startFrame   = (ImGui::FrameIndexType)j.value("startFrame", 0);
		timeline.endFrame     = (ImGui::FrameIndexType)j.value("endFrame", 300);
		timeline.currentFrame = (ImGui::FrameIndexType)j.value("currentFrame", 0);

		timeline.tracks.clear();
		if (j.contains("tracks"))
		{
			for (const auto& jt : j["tracks"])
			{
				Track track;
				track.id   = makeTrackId();
				track.name = jt.value("name", std::string("Track"));
				track.open = jt.value("open", true);
				if (jt.contains("keys"))
				{
					for (const auto& jk : jt["keys"])
					{
						Keyframe key;
						key.id    = makeKeyframeId();
						key.name  = jk.value("name", std::string(""));
						key.frame = (ImGui::FrameIndexType)jk.value("frame", 0);
						key.value = jk.value("value", 0.0f);
						key.ease  = (EaseType)jk.value("ease", 0);
						track.keys.push_back(key);
					}
				}
				sortTrack(track);
				timeline.tracks.push_back(std::move(track));
			}
		}

		if (timeline.tracks.empty())
			timeline.tracks.push_back({makeTrackId(), "Track 1", true, {}});
	}
};
