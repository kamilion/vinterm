#include "terminal/charmatrix.h"

#include <cstdlib>
#include <iostream>
#include <utility>

#include "options.h"
#include "terminal/clipboard.h"

CharMatrix::CharMatrix()
	: dirty(new set<int>), InsertMode(false), CursorVisibility(VISIBLE), 
	  blink(new Blink(options->blink_speed)), 
	  current_attr(Attribute()), w(80), h(24), 
	  cursor_x(0), cursor_y(0), scroll_top(0), scroll_bottom(h-1),
	  saved_x(0), saved_y(0), flashing(false), 
	  backtrack(new Backtrack(*this, 5)), current_backtrack(0),
	  screen_advances(0), old_cursor_x(0), old_cursor_y(0)
{
	chars.insert(chars.begin(), w*h, Char());
	for(int x=0; x<w; x+=8)
		tabs.insert(x);
}


CharMatrix::~CharMatrix()
{
	chars.clear();
	saved_screen.clear();
	dirty->clear();
	delete backtrack;
	delete dirty;
	delete blink;
}


Char 
CharMatrix::Ch(int x, int y) const 
{
	//if(!IsSelected(x, y))
		return chars[x+(y*W())]; 
	/*else
	{
		Char ch = chars[x+(y*W())];
		ch.Attr.Reverse = !ch.Attr.Reverse;
		return ch;
	}*/
}


void
CharMatrix::BackTrack()
{
	if(current_backtrack >= backtrack->Screens())
		return;
	if(current_backtrack == 0)
		SaveScreen();
	current_backtrack++;
	for(int x=0; x<W(); x++)
		for(int y=0; y<H(); y++)
		{
			Char c = backtrack->Get(current_backtrack, x, y);
			Put(c.Ch, c.Attr, x, y);
		}
	CursorVisibility = NOT_VISIBLE;
}


void
CharMatrix::ForeTrack()
{
	if(current_backtrack == 0)
		return;
	else if(current_backtrack == 1)
		ForetrackToScreen();
	else
	{
		current_backtrack--;

		for(int x=0; x<W(); x++)
			for(int y=0; y<H(); y++)
			{
				Char c = backtrack->Get(current_backtrack, x, y);
				Put(c.Ch, c.Attr, x, y);
			}
	}
}


void
CharMatrix::ForetrackToScreen()
{
	if(current_backtrack > 0)
	{
		RestoreScreen();
		CursorVisibility = VISIBLE;
		current_backtrack = 0;
	}
}


void 
CharMatrix::Resize(int new_w, int new_h)
{
	vector<Char> old_chars;
	old_chars.assign(chars.begin(), chars.end());

	// resize chars
	chars.clear();
	chars.insert(chars.begin(), new_w*new_h, Char());
	for(int x=0; x<min(w, new_w); x++)
		for(int y=0; y<min(h, new_h); y++)
		{
			if(chars[x+y*new_w] != old_chars[x+y*w])
			{
				chars[x+y*new_w] = old_chars[x+y*w];
				dirty->insert(x+y*new_w);
			}
		}

	// resize saved screen
	if(!saved_screen.empty())
	{
		old_chars.clear();
		old_chars.assign(saved_screen.begin(), saved_screen.end());
		saved_screen.clear();
		saved_screen.insert(saved_screen.begin(), new_w*new_h, Char());
		for(int x=0; x<min(w, new_w); x++)
			for(int y=0; y<min(h, new_h); y++)
				saved_screen[x+y*new_w] = old_chars[x+y*w];
	}

	// new parameters
	scroll_bottom += (new_h - h);
	w = new_w;
	h = new_h;

	if(scroll_bottom < scroll_top)
		scroll_bottom = scroll_top;
	if(cursor_x >= w)
		cursor_x = w-1;
	if(cursor_y >= h)
		cursor_y = h-1;

	// reset blink clock
	blink->ResetClock();
}


void 
CharMatrix::Put(const char c, bool ignore_insert_mode)
{
	if(cursor_x >= w)
	{
		cout << "non-line-break in line " << cursor_y << endl;
		chars[cursor_x - 1 + cursor_y * W()].Attr.ContinueNextLine = true;
		cursor_x = 0;
		AdvanceCursorY();
	}
	ValidateCursorPosition();

	Put(c, cursor_x, cursor_y, ignore_insert_mode);
	
	++cursor_x;
}


void 
CharMatrix::Put(const char c, const int x, const int y, 
		bool ignore_insert_mode)
{
	Put(c, current_attr, x, y, ignore_insert_mode);
}


void 
CharMatrix::Put(const char c, Attribute attr, const int x, const int y,
			bool ignore_insert_mode)
{
	// restart selection
	SetNoSelection();

	if(InsertMode && !ignore_insert_mode)
		InsertChars(1);

	// check if flashing
	if(flashing)
		attr.Reverse = !attr.Reverse;

	// put char on the grid
	int pos = x + (y * W());
	if(pos < 0 || pos >= (W() * H()))
		abort();
	chars[pos].Ch = c;
	chars[pos].Attr = attr;

	// add to list of positions to be updated on the screen
	dirty->insert(pos);

	// reset blinks
	blink->ResetClock();
}


bool
CharMatrix::AdvanceCursorY()
{
	bool update_scr = false;

	++cursor_y;

	// advance page
	if(cursor_y > scroll_bottom)
	{
		ScrollUp();
		--cursor_y;

		// verify if needs to update screen
		screen_advances++;
		if(screen_advances % H() == 0)
			update_scr = true;
	}
	ValidateCursorPosition();

	return update_scr;
}


void CharMatrix::RecedeCursorY()
{
	--cursor_y;
	
	if(cursor_y < scroll_top)
	{
		ScrollDown();
		++cursor_y;
	}
	ValidateCursorPosition();
}


void
CharMatrix::CarriageReturn()
{
	cursor_x = 0;
}


void
CharMatrix::Tab()
{
	++cursor_x;
	while((cursor_x % 8) != 0)
	{
		++cursor_x;
		if(cursor_x >= w)
		{
			cursor_x = 0;
			AdvanceCursorY();
		}
	}
}


void
CharMatrix::Backspace()
{
	if(cursor_x > 0)
		--cursor_x;
}


void 
CharMatrix::MoveCursor(Direction dir, int moves)
{
	int x = 0, y = 0;
	switch(dir)
	{
		case UP: y = -1; break;
		case DOWN: y = 1; break;
		case LEFT: x = -1; break;
		case RIGHT: x = 1; break;
		default: abort();
	}

	if(moves == 0)
		moves = 1;

	cursor_x += (x * moves);
	cursor_y += (y * moves);

	ValidateCursorPosition();
}


void 
CharMatrix::SetCursorPosition(int x, int y)
{
	cursor_x = x-1;
	cursor_y = y-1;
	ValidateCursorPosition();
}


void
CharMatrix::ScrollUp()
{
	// add line to backtrack
	vector<Char> line;
	for(int x(0); x<w; x++)
		line.push_back(Ch(x, 0));
	backtrack->PushLine(line);

	// move everything up
	for(int y(scroll_top+1); y<=scroll_bottom; y++)
		for(int x(0); x<w; x++)
			Put(Ch(x,y).Ch, Ch(x,y).Attr, x, y-1);

	// clear last line
	for(int x(0); x<w; x++)
		Put(' ', Attribute(), x, scroll_bottom);
}


void
CharMatrix::ScrollDown()
{
	for(int y(scroll_bottom-1); y>=scroll_top; y--)
		for(int x(0); x<w; x++)
			Put(Ch(x,y).Ch, Ch(x,y).Attr, x, y+1);
	for(int x(0); x<w; x++)
		Put(' ', Attribute(), x, scroll_top);
}


void 
CharMatrix::SetScrollingRegion(int top, int bottom)
{
	scroll_top = top-1;
	scroll_bottom = bottom-1;
	cursor_x = cursor_y = 0;

	if(scroll_bottom < scroll_top)
		cerr << "warning: scroll_bottom < scroll_top!" << endl;
}


void 
CharMatrix::AddLinesBelowCursor(int n)
{
	int save_sc_top = scroll_top;
	scroll_top = cursor_y;
	for(int i=0; i<n; i++)
		ScrollDown();
	scroll_top = save_sc_top;
}


void 
CharMatrix::ClearRow(bool upto_cursor, int y)
{
	if(y == -1) 
		y = cursor_y;
	for(int x = (upto_cursor ? 0 : cursor_x); 
			x <= (upto_cursor ? cursor_x : w-1); x++)
		Put(' ', Attribute(), x, y);
}


void
CharMatrix::DeleteLines(int n)
{
	int save_sc_top = scroll_top;
	//int save_sc_bt = scroll_bottom;
	scroll_top = cursor_y;
	//scroll_bottom = h-1;
	for(int i=0; i<n; i++)
		ScrollUp();
	scroll_top = save_sc_top;
	//scroll_bottom = save_sc_bt;
}


void CharMatrix::DeleteChars(int n)
{
	for(int x=cursor_x; x <= w-n; x++)
	{
		Char ch = Ch(x+n, cursor_y);
		Put(ch.Ch, ch.Attr, x, cursor_y);
	}
	for(int x=w-n; x<w; x++)
		Put(' ', x, cursor_y); // XXX - attribute?
}


void CharMatrix::EraseChars(int n)
{
	for(int x=cursor_x; x<(cursor_x+n); x++)
		Put(' ', x, cursor_y); // XXX - attribute?
}


void 
CharMatrix::InsertChars(int n)
{
	for(int x=w-n-1; x >= cursor_x; x--)
	{
		Char ch = Ch(x, cursor_y);
		Put(ch.Ch, ch.Attr, x+n, cursor_y);
	}
	for(int x=cursor_x; x<(cursor_x+n); x++)
		Put(' ', current_attr, x, cursor_y); // XXX - attribute?
}


void 
CharMatrix::SaveScreen()
{
	// copy screen contents
	saved_screen.clear();
	saved_screen.assign(chars.begin(), chars.end());
	saved_x = cursor_x;
	saved_y = cursor_y;

	// clear screen
	cursor_x = cursor_y = 0;
	for(int y = cursor_y; y < h; y++)
		ClearRow(false, y);
}


void 
CharMatrix::RestoreScreen()
{
	chars.assign(saved_screen.begin(), saved_screen.end());
	saved_screen.clear();
	cursor_x = saved_x;
	cursor_y = saved_y;
	ValidateCursorPosition();

	for(int i=0; i < (w*h); i++)
		dirty->insert(i);
}


void CharMatrix::RegisterBlinks() const
{
	for(int i=0; i<(w*h); i++)
		if(chars[i].Attr.Blink)
			dirty->insert(i);
}


void CharMatrix::SetAttr(AttrType attr, bool value)
{
	switch(attr)
	{
	case NONE:
		current_attr = Attribute();
		break;
	case HIGHLIGHT:
		current_attr.Highlight = value;
		break;
	case UNDERLINE:
		current_attr.Underline = value;
		break;
	case BLINK:
		current_attr.Blink = value;
		break;
	case REVERSE:
		current_attr.Reverse = value;
		break;
	case DIM:
		current_attr.Dim = value;
		break;
	case INVISIBLE:
		current_attr.Invisible = value;
		break;
	default:
		abort();
	}
}


void
CharMatrix::Flash(bool reverse)
{
	if(reverse || flashing) 
	{
		for(int i=0; i<(w*h); i++)
		{
			chars[i].Attr.Reverse = !chars[i].Attr.Reverse;
			dirty->insert(i);
		}
		flashing = reverse;
	}
}


void
CharMatrix::ValidateCursorPosition()
{
	if(cursor_x < 0)
	{
		fprintf(stderr, "warning: application tried to move cursor to x < 0.\n");
		cursor_x = 0;
	}
	else if(cursor_x >= w)
	{
		fprintf(stderr, "warning: application tried to move cursor to x >= w.\n");
		cursor_x = w-1;
	}
	if(cursor_y < 0)
	{
		fprintf(stderr, "warning: application tried to move cursor to y < 0.\n");
		cursor_y = 0;
	}
	else if(cursor_y >= h)
	{
		fprintf(stderr, "warning: application tried to move cursor to y >= h.\n");
		cursor_y = h-1;
	}
}


void 
CharMatrix::SetStartSelection(int x, int y)
{
	selection.start = x+y*W();
}


void 
CharMatrix::SetEndSelection(int x, int y)
{
	int old_start = selection.start;
	int old_end = selection.end;

	selection.end = x+y*W();

	/* if(selection.start > selection.end)
		swap(selection.start, selection.end); */

	if(selection.Active())
	{
		int st = min(selection.start, min(old_start, min(selection.end, old_end)));
		int en = max(selection.start, max(old_start, max(selection.end, old_end)));
		if(st < 0)
			st = 0;

		// update screen
		for(int i=st; i<=en; ++i)
			dirty->insert(i);

		// add to clipboard
		string selection;
		for(int i=st; i<=en; ++i)
		{
			// add to selection
			selection += chars[i].Ch;

			// check for EOL
			if(i % W() == (W()-1) && !chars[i].Attr.ContinueNextLine)
			{
				while(selection[selection.size()-1] == ' ')
					selection.pop_back();
				selection += "\n";
			}
		}
		clipboard->Store(selection);
	}
}


void 
CharMatrix::SetNoSelection()
{
	if(selection.Active())
	{
		for(int i=0; i < (w*h); i++)
			dirty->insert(i);
		selection.Reset();
	}
}


bool 
CharMatrix::IsSelected(int x, int y) const
{
	if(!selection.Active())
		return false;

	return ((x+y*W()) >= min(selection.start, selection.end)) 
	    && ((x+y*W()) <= max(selection.start, selection.end));
}


void
CharMatrix::CheckForBlink()
{
	// blink, if necessary
	if(blink->TimeToBlink())
	{
		blink->DoBlink(*this);
		dirty->insert(CursorX() + CursorY() * W());
		RegisterBlinks();
	}

	// register a change where the cursor was and where it is now
	if(CursorX() != old_cursor_x || CursorY() != old_cursor_y)
	{
		dirty->insert(old_cursor_x + old_cursor_y * W());
		dirty->insert(CursorX() + CursorY() * W());
		old_cursor_x = CursorX();
		old_cursor_y = CursorY();
	}
}
