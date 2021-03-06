#ifndef BLINK_H
#define BLINK_H

#include <stdint.h>

class CharMatrix;

class Blink
{
public:
	Blink(const uint32_t milliseconds);

	void DoBlink(CharMatrix const& cm);
	bool TimeToBlink() const;
	void ResetClock();
	bool State() const { return state; }

private:
	bool state;
	const uint32_t milliseconds;
	uint32_t last_blink;
};

#endif
