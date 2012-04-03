#ifndef VT100_H
#define VT100_H

#include <vector>
#include <string>
using namespace std;

#include "terminal.h"

typedef enum { CURSOR, APPLICATION } Mode;



class VT100 : public Terminal
{
public:
	VT100(Options const& options)
		: Terminal(options, "vt100"), cursor_keys(CURSOR),
                  keypad_keys(CURSOR) { }

protected:
	virtual void ExecuteEscapeSequence(string const& s);
	virtual bool SpecialKeyPress(SDL_keysym key);

	// list of commands
	void ChangeCursorPosition(const int x, const int y);
	void EraseScreen();
	void EraseFromCursorToEndOfScreen();
	void EraseToEOL();
	void MoveCursorUp(const int n);
	void MoveCursorDown(const int n);
	void MoveCursorLeft(const int n);
	void MoveCursorRight(const int n);

private:
	void AddModifier(Uint8* k, string& code);
	void KeypadPress(SDLKey key);
	bool ParseCommand(string const& seq, char& command, 
			vector<int>& parameters);

	Mode cursor_keys, keypad_keys;

	static const struct KeyCode {
		SDLKey key;
		string code;
		string cursor_or_application_code;
	} keycodes[]; 
};

#endif
