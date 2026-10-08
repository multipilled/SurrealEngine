#pragma once

class UActor;
class VisibleFrame;

// Draws the particles of a Brother Bear emitter actor
class VisibleEmitter
{
public:
	void Draw(VisibleFrame* frame, UActor* actor);
};
