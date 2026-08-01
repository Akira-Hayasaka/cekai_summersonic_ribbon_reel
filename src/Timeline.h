#pragma once
#include "ofMain.h"
#include "Sequencer.h"

class Timeline
{
public:
	Timeline(std::shared_ptr<Sequencer> _sequencer);

    void update();
	void step();
    void draw();

	const float getCurrentTimeSec() const { return current_time_sec; }

private:

	void on_SequencerKeyframeEvent(SequencerKeyframeEvent & e);
	void on_SequencerFrameEvaluatedEvent(SequencerFrameEvaluatedEvent & e);
	void on_SequencerEndReachedEvent(SequencerEndReachedEvent & e);

	std::shared_ptr<Sequencer> sequencer;
    int current_frame;
	float current_time_sec;
};
