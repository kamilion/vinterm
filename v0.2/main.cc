#include <string>
#include <iostream>
using namespace std;

#include "options.h"
#include "graphic/curses_lib.h"
#include "terminal/framebuffer.h"
#include "terminal/vinterm.h"

int main(int argc, char** argv)
{
	// initialize graphic library
	GraphicLibrary* gl;
	try 
	{
		gl = GraphicLibrary::Initialize(GraphicLibrary::CURSES, 80, 24);
	} 
	catch(string& er) 
	{
		cerr << er << endl;
		return 1;
	}

	Options options(argc, argv);
	Framebuffer fb(options);
	gl->Initialize(options, fb);
	Vinterm terminal(fb, *gl);

	fb << 'a';

	while(terminal.Active())
	{
		terminal.Input();
		terminal.Output();
		gl->Update();
	}

	delete gl;

	return 0;
}
