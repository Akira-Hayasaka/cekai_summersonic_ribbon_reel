#include "Globals.h"

Proto::Proto()
{
	v.resize(10);
	ofxSubscribeOsc(13000, "/proto_values", v);
}

namespace Globals
{
	ofJson package;
	ofJson asset;
	Proto::Ref proto;
	float ELAPSED_TIME;
	std::unique_ptr<Timeline> timeline;
	std::shared_ptr<Sequencer> sequencer;
	std::unique_ptr<Renderer> renderer;
}
