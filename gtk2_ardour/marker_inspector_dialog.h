/*
    Copyright (C) 2014 Waves Audio Ltd.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef __gtk2_ardour_marker_inspector_dialog_h__
#define __gtk2_ardour_marker_inspector_dialog_h__

#include <map>
#include <vector>
#include <string>
#include <gtkmm.h>

#include "ardour/session_handle.h"
#include "ardour/types.h"

#include "waves_ui.h"
#include "pbd/signals.h"
#include "pbd/xml++.h"

class WavesButton;
class Marker;

class MarkerInspectorDialog : public Gtk::Window, public ARDOUR::SessionHandlePtr, public WavesUI {
  public:
	MarkerInspectorDialog ();
    ~MarkerInspectorDialog ();

  protected:
	void on_realize ();

  private:
	Gtk::Container& _empty_panel;
	Gtk::Container& _inspector_panel;
	WavesButton&   _color_palette_button;
	Gtk::Container& _color_buttons_home;

	WavesButton&   _info_panel_button;
	Gtk::Container& _info_panel_home;
	
	Gtk::Label& _location_name;
	WavesButton&   _program_change_button;

#include "marker_inspector_dialog.logic.h"
};

#endif /* __gtk2_ardour_marker_inspector_dialog_h__ */
