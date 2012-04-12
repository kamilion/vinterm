#ifndef TERMINAL_H
#define TERMINAL_H

#include <string>
using namespace std;

class Framebuffer;
class Screen;
class PTY;

class Terminal
{
public:
	Terminal(Framebuffer& fb, PTY& pty);
	virtual ~Terminal();

	bool Active() const { return active; }
	void Input();
	void Output(Screen const& screen);
	virtual void ExecuteEscapeSequence(string const& sequence);
	virtual void KeyPressed(int key);

protected:
	Framebuffer& fb;
	PTY& pty;

private:
	void InputChar(const char c);
	void InputEscapeChar(const char c);

	bool active;
	bool escape_mode;
	string escape_sequence;
};

#endif