#pragma once

#include "ofMain.h"
#include "ofxPubSubOsc.h"
#include "Timeline.h"
#include "Sequencer.h"
#include "Renderer.h"
#include "rxcpp/rx.hpp"

struct Proto
{
	using Ref = std::shared_ptr<Proto>;
	Proto();
	std::vector<float> v;
};

namespace Globals
{
	extern ofJson package;
	extern ofJson asset;
	extern Proto::Ref proto;
	extern float ELAPSED_TIME;
	extern std::unique_ptr<Timeline> timeline;
	extern std::shared_ptr<Sequencer> sequencer;
	extern std::unique_ptr<Renderer> renderer;
}
