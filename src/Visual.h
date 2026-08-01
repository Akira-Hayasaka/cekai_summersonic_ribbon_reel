#pragma once
#include "ofMain.h"
#include "Headliner.h"
#include "Poster.h"
#include "Poster_History.h"
#include "BG.h"
#include "Reel.h"
#include "Transition.h"
#include "InfoText.h"
#include "Warper.h"
#include "Twist.h"

class Visual
{
public:

	Visual();

	void update();
	void draw();
	std::vector<std::shared_ptr<Headliner>> & getHeadliners();
	const std::vector<std::shared_ptr<Headliner>> & getHeadliners() const;
	Headliner* getHeadliner();
	const Headliner* getHeadliner() const;

private:

	void on_SequencerKeyframeEvent(SequencerKeyframeEvent & e);

	std::vector<std::shared_ptr<Headliner>> headliners;
	std::vector<std::shared_ptr<Poster>> posters;

	std::unique_ptr<Poster_History> poster_history;
	std::unique_ptr<BG> bg;
	std::unique_ptr<Reel> reel;
	std::unique_ptr<Transition> transition;
	std::unique_ptr<InfoText> infoText;
	std::unique_ptr<Warper> warper;
	std::unique_ptr<Twist> twist;

	bool b_make_reel_front = false;
};
