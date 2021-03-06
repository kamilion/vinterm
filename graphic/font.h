#ifndef FONT_H
#define FONT_H

#include <string>
using namespace std;

#include "global.h"

class Font
{
public:
	virtual ~Font() { }

	static Font* LoadFont();
	virtual string Encoding() const = 0;
	virtual void DrawChar(uint32_t ch, uint8_t* pixels, int x, int y, int pitch,
			uint8_t bg_color, uint8_t fg_color, bool underline) const = 0;
	virtual int CharWidth() const = 0;
	virtual int CharHeight() const = 0;

protected:
	Font() { }
};

#endif
