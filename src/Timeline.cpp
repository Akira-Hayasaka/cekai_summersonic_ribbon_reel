#include "Timeline.h"

Timeline::Timeline(std::shared_ptr<Sequencer> _sequencer)
	: sequencer(_sequencer)
	, current_frame(0)
	, current_time_sec(0.0f)
{
	ofAddListener(Sequencer::keyframeEvent, this, &Timeline::on_SequencerKeyframeEvent);
	ofAddListener(Sequencer::frameEvaluatedEvent, this, &Timeline::on_SequencerFrameEvaluatedEvent);
	ofAddListener(Sequencer::endReachedEvent, this, &Timeline::on_SequencerEndReachedEvent);
}

void Timeline::update()
{
}

void Timeline::step()
{
	sequencer->step();
}

void Timeline::draw()
{
}


void Timeline::on_SequencerKeyframeEvent(SequencerKeyframeEvent & e)
{
	//ofLog() << "Keyframe event: track " << e.trackId
	//		<< ", keyframe " << e.keyframeId
	//		<< ", frame " << e.keyFrame
	//		<< ", value " << e.value
	//		<< ", ease " << static_cast<int>(e.ease)
	//		<< ", direction " << (e.direction == SequencerPlaybackDirection::Forward ? "forward" : "backward")
	//		<< ", source " << static_cast<int>(e.source);
}

void Timeline::on_SequencerFrameEvaluatedEvent(SequencerFrameEvaluatedEvent & e)
{
	//ofLog() << "Frame evaluated event: previous frame " << e.previousFrame
	//		<< ", current frame " << e.currentFrame
	//		<< ", time sec " << e.timeSec
	//		<< ", fps " << e.fps
	//		<< ", source " << static_cast<int>(e.source);

	current_frame = e.currentFrame;
	current_time_sec = e.timeSec;

}

void Timeline::on_SequencerEndReachedEvent(SequencerEndReachedEvent & e)
{
	//ofLog() << "End reached event: frame " << e.frame
	//		<< ", source " << static_cast<int>(e.source);
}
