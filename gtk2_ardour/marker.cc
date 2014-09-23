/*
    Copyright (C) 2001 Paul Davis

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

#include <cmath>
#include <sigc++/bind.h>
#include <boost/scoped_ptr.hpp>

#include "ardour/tempo.h"
#include "ardour/profile.h"

#include "canvas/rectangle.h"
#include "canvas/container.h"
#include "canvas/line.h"
#include "canvas/polygon.h"
#include "canvas/text.h"
#include "canvas/canvas.h"
#include "canvas/scroll_group.h"
#include "canvas/utils.h"
#include "canvas/debug.h"

#include "ardour_ui.h"
/*
 * ardour_ui.h include was moved to the top of the list
 * due to a conflicting definition of 'Rect' between
 * Apple's MacTypes.h and GTK.
 */
#include "gui_thread.h"
#include "marker.h"
#include "public_editor.h"
#include "utils.h"
#include "rgb_macros.h"

#include <gtkmm2ext/utils.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace Gtkmm2ext;

PBD::Signal1<void,Marker*> Marker::CatchDeletion;

const double Marker::_marker_height = 17.0;
const char * Marker::default_new_marker_prefix = N_("Marker");

static const double name_padding = 10.0;

RulerMarker::RulerMarker (ARDOUR::Location* l, PublicEditor& editor, ArdourCanvas::Container& parent, double height, guint32 rgba, const std::string& text,
                          framepos_t start, framepos_t end)
        : RangeMarker (l, editor, parent, height, rgba, text, start, end)
{
        /* make sure we call our own color stuff, since we look different */

        use_color ();
}

void
RulerMarker::use_color ()
{
        Marker::use_color ();

        /* unset the effects of RangeMarker::use_color () */

        _name_background->set_pattern (Cairo::RefPtr<Cairo::SurfacePattern> ());
}

RangeMarker::RangeMarker (ARDOUR::Location* l, PublicEditor& editor, ArdourCanvas::Container& parent, double height, guint32 rgba, const std::string& text,
                          framepos_t start, framepos_t end)
        : Marker (l, editor, parent, height, rgba, text, Range, start, true)
        , _end_frame (end)
        , _end_line (0)
{
        assert (start < end);

        /* Marker::Marker calls these but will not have used our versions since it is a constructor.
        */

        set_position (frame_position);
        setup_line ();
        use_color ();
        setup_name_display ();
}

RangeMarker::~RangeMarker ()
{
        delete _end_line;
}

void
RangeMarker::use_color ()
{
        Marker::use_color ();

        double dimen = _height * 2.0;

        Cairo::RefPtr<Cairo::Surface> surface = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, dimen, dimen);
        Cairo::RefPtr<Cairo::Context> context = Cairo::Context::create (surface);

        /* make a darker version of the color */

        double h, s, v;
        ArdourCanvas::color_to_hsv (_color, h, s, v);
        s *= 1.2;
        v *= 1.2;
        ArdourCanvas::Color darker = ArdourCanvas::hsv_to_color (h, s, v, 1.0);
        
        /* fill background with transparent version of color */
        ArdourCanvas::set_source_rgba (context, darker);
        context->rectangle (0.0, 0.0, dimen, dimen);
        context->fill ();

        /* now draw lines */

        ArdourCanvas::set_source_rgba (context, _color);
        const double line_width = 2.0;
        context->set_line_width (line_width);

        const double d = line_width / (cos ((45.0 / 360.0) * (M_PI * 2.0)));
        
        for (double p = d; p < (dimen * 3.0); p += (2.0 * d)) {
                context->move_to (p, 0);
                context->line_to (0, p);
                context->stroke ();
        }

        /* and assign pattern to the background rect */

        Cairo::RefPtr<Cairo::SurfacePattern> pattern = Cairo::SurfacePattern::create (surface);
        pattern->set_extend (Cairo::EXTEND_REPEAT);
        _name_background->set_pattern (pattern);

        /* set end line correctly, also */

        if (_end_line) {
                _end_line->set_outline_color (_color);
        }

        if (_name_item) {
                /* black text */
                _name_item->set_color (ArdourCanvas::rgba_to_color (0.0, 0.0, 0.0, 1.0));
        }
}

void
RangeMarker::_set_position (framepos_t start, framepos_t end)
{
        if (end >= 0) {
                _end_frame = end;
        }

        Marker::_set_position (start, end);

        if (end >= 0) {
                
                /* clamp displayed length of text to visible marker width
                   since the marker's visible width is variable depending
                   on zoom (not true for single-position, non-range markers.
                */
                
                double pixel_width = editor.sample_to_pixel (_end_frame - frame_position);
                _name_background->set_x1 (_name_background->x0() + pixel_width);
                
                cerr << "reset x1 to " << _name_background->x1() << " based on " << frame_position << " .. " << _end_frame << endl;
                
                if (_name_item) {
                        _name_item->clamp_width (pixel_width - _label_offset);
                }
        }
}        

void
RangeMarker::setup_line ()
{
        Marker::setup_line ();

        if (!_end_line) {
                _end_line = new ArdourCanvas::Line (editor.get_hscroll_group());
                _end_line->set_y1 (ArdourCanvas::COORD_MAX);
        }

        /* line at the end (the start line is handled by Marker */
        
        /* lines in a different canvas (scroll)group so we have to convert the position
           into a different coordinate system.
        */
        
        ArdourCanvas::Duple h = _name_background->item_to_canvas (ArdourCanvas::Duple (0.0, _height));
        ArdourCanvas::Duple g = group->canvas_origin();

       /* merge and adjust them */
       g.x += _shift + _name_background->x1();
       g.y = h.y;

       ArdourCanvas::Duple d = _end_line->canvas_to_item (g);
       
       _end_line->set_y0 (d.y); /* bottom of marker, in the right coordinate system */
       _end_line->set_x0 (d.x);
       _end_line->set_x1 (d.x);
       _end_line->set_outline_color (_color);
       _end_line->show ();
}

void
RangeMarker::bounds_changed ()
{
        _set_position (_location->start(), _location->end());
}

void
RangeMarker::canvas_height_set (double h) 
{
        if (_end_line) {
                /* h is already in the right coordinate system since it is an absolute height */
                _end_line->set_y1 (h);
        }
}

void
RangeMarker::setup_name_display ()
{
        /* No need to adjust name background size here, since it is always the same */

	if (_name.empty()) {
                if (_name_item) {
                        _name_item->hide ();
                }
	} else {

                if (!_name_item) {
                        _name_item = new ArdourCanvas::Text (group);
                        CANVAS_DEBUG_NAME (_name_item, string_compose ("Marker::_name_item for %1", _name));
                        _name_item->set_font_description (name_font);
                }
                
		_name_item->show ();
                _name_item->set_x_position (_label_offset);
                /* Limit text to width of background rect */
		_name_item->clamp_width (_name_background->get().width());
		_name_item->set (_name);
        }
}

Marker::Marker (ARDOUR::Location* l, PublicEditor& ed, ArdourCanvas::Container& parent, double height, guint32 rgba, const string& annotation,
		Type type, framepos_t start_pos, bool handle_events)

	: editor (ed)
        , _location (l)
	, _parent (&parent)
        , group (0)
        , mark (0)
        , _name_item (0)
	, _start_line (0)
        , _scene_change_rect (0)
        , _scene_change_text (0)
        , frame_position (start_pos)
	, _type (type)
	, _shown (false)
        , _height (height)
	, _color (rgba)
	, _left_label_limit (DBL_MAX)
	, _right_label_limit (DBL_MAX)
	, _label_offset (0)
        , _have_scene_change (false)
{
	unit_position = editor.sample_to_pixel (frame_position);
	unit_position -= _shift;

	group = new ArdourCanvas::Container (&parent, ArdourCanvas::Duple (unit_position, 0));
	CANVAS_DEBUG_NAME (group, string_compose ("Marker::group for %1", annotation));

	_name_background = new ArdourCanvas::Rectangle (group);
	CANVAS_DEBUG_NAME (_name_background, string_compose ("Marker::_name_background for %1", annotation));
        /* x-coordinates will be set elsewhere */
        _name_background->set_y0 (0.0);
	_name_background->set_y1 (_height - 1.0); 

        _label_offset = name_padding;

	/* setup name pixbuf sizes */
	name_font = Pango::FontDescription (ARDOUR_UI::config()->get_canvasvar_SmallBoldFont());
	Gtk::Label foo;
	Glib::RefPtr<Pango::Layout> layout = foo.create_pango_layout (X_("Hg")); /* ascender + descender */
	int width;
	layout->set_font_description (name_font);
	Gtkmm2ext::get_ink_pixel_size (layout, width, name_height);
	
	editor.ZoomChanged.connect (sigc::mem_fun (*this, &Marker::reposition));

	if (handle_events) {
                /* events will be handled by both the group, make sure it can be used to lookup this object.
                 */
                group->set_data ("marker", this);
		group->Event.connect (sigc::bind (sigc::mem_fun (editor, &PublicEditor::canvas_marker_event), group, this));
	}

	set_name (annotation);
        pick_basic_color (rgba);
        use_color ();

        if (_location) {
                /* Listen to region properties that we care about */

                _location->FlagsChanged.connect (location_connections, invalidator(*this), boost::bind (&Marker::flags_changed, this), gui_context());
                _location->NameChanged.connect (location_connections, invalidator(*this), boost::bind (&Marker::name_changed, this), gui_context());
                _location->StartChanged.connect (location_connections, invalidator(*this), boost::bind (&Marker::bounds_changed, this), gui_context());
                _location->EndChanged.connect (location_connections, invalidator(*this), boost::bind (&Marker::bounds_changed, this), gui_context());
                _location->Changed.connect (location_connections, invalidator(*this), boost::bind (&Marker::bounds_changed, this), gui_context());
        }
}

Marker::~Marker ()
{
	CatchDeletion (this); /* EMIT SIGNAL */

	/* destroying the parent group destroys its contents, namely any polygons etc. that we added */
	delete group;

        /* this isn't a child of the group, but belongs to the global canvas hscroll group */
	delete _start_line;
}

void Marker::reparent(ArdourCanvas::Container & parent)
{
	group->reparent (&parent);
	_parent = &parent;
}

void
Marker::name_changed ()
{
        _name = _location->name();
        setup_name_display ();
}

void
Marker::flags_changed ()
{
        /* flags are basically indicate by different coloring, so choose
           the right one again.
        */
        pick_basic_color (0);

        /* location could also have been hidden */

        if (_location && _location->is_hidden()) {
                group->hide ();
        } else {
                group->show ();
        }
}

void
Marker::bounds_changed ()
{
        /* handler can only be invoked if _location was non-null */

        set_position (_location->start ());
}

void
Marker::canvas_height_set (double h) 
{
        if (_start_line) {
                _start_line->set_y1 (h);
        }
}

void
Marker::set_color (ArdourCanvas::Color c)
{
        _color = c;
        use_color ();
}

void
Marker::reset_color ()
{
        pick_basic_color (0);
}

void
Marker::pick_basic_color (ArdourCanvas::Color c)
{
        ArdourCanvas::Color col;

        if (_location) {
                if (_location->is_cd_marker()) {
                        col = ARDOUR_UI::config()->get_canvasvar_LocationCDMarker();
                } else if (_location->is_mark()) {
                        col = ARDOUR_UI::config()->get_canvasvar_LocationMarker();
                } else if (_location->is_auto_loop()) {
                        col = ARDOUR_UI::config()->get_canvasvar_LocationLoop();
                } else if (_location->is_auto_punch()) {
                        col = ARDOUR_UI::config()->get_canvasvar_LocationPunch();
                } else if (_location->is_skip()) {
                        if (_location->is_skipping()) {
                                col = ARDOUR_UI::config()->get_canvasvar_LocationSkipping();
                        } else {
                                col = ARDOUR_UI::config()->get_canvasvar_LocationSkip();
                        }
                } else {
                        col = ARDOUR_UI::config()->get_canvasvar_LocationRange();
                }
        } else {
                col = c;
        }

        set_color (col);
}

void
Marker::setup_line ()
{
        if (_start_line == 0) {
                _start_line = new ArdourCanvas::Line (editor.get_hscroll_group());
                _start_line->Event.connect (sigc::bind (sigc::mem_fun (editor, &PublicEditor::canvas_marker_event), group, this));
                _start_line->set_y1 (ArdourCanvas::COORD_MAX);
        }

        /* lines in a different canvas (scroll)group so we have to convert the position
           into a different coordinate system.
        */
        
        ArdourCanvas::Duple h = _name_background->item_to_canvas (ArdourCanvas::Duple (0.0, _height));
        ArdourCanvas::Duple g = group->canvas_origin();
        
        /* merge and adjust them */
        g.x += _shift;
        g.y = h.y;
        
        ArdourCanvas::Duple d = _start_line->canvas_to_item (g);
        
        _start_line->set_y0 (d.y); /* bottom of marker, in the right coordinate system */
        _start_line->set_x0 (d.x);
        _start_line->set_x1 (d.x);
        _start_line->set_outline_color (_color);
        _start_line->show ();
}

ArdourCanvas::Item&
Marker::the_item() const
{
	return *group;
}

void
Marker::set_name (const string& new_name)
{
	_name = new_name;

	setup_name_display ();
}

/** @return true if our label is on the left of the mark, otherwise false */
bool
Marker::label_on_left () const
{
	return (_type == SessionEnd || _type == RangeEnd || _type == LoopEnd || _type == PunchOut);
}

void
Marker::setup_name_display ()
{
	double limit = _left_label_limit;
        
	if (_name.empty()) {

                if (_name_item) {
                        _name_item->hide ();
                }

                _name_background->set_x0 (0);
                /* hard-coded fixed width used if there is no text to display */
                _name_background->set_x1 (10);
                
                return;

	} else {

                int name_width;
                int scene_change_width = 0;

                if (!_name_item) {
                        _name_item = new ArdourCanvas::Text (group);
                        CANVAS_DEBUG_NAME (_name_item, string_compose ("Marker::_name_item for %1", _name));
                        _name_item->set_font_description (name_font);
                        /* white with 95% opacity */
                        _name_item->set_color (ArdourCanvas::rgba_to_color (1.0,1.0,1.0,0.95));
                }

                if (_have_scene_change) {
                        
                        /* coordinates of rect that will surround "MIDI" */
                        
                        ArdourCanvas::Rect r;
                        int midi_height;
                        
                        pixel_size (X_("MIDI"), name_font, name_width, midi_height);
                        
                        r.x0 = 2.0;
                        r.x1 = r.x1 + name_width + 7.0;
                        
                        if (_scene_change_text == 0) {
                                _scene_change_rect = new ArdourCanvas::Rectangle (group);
                                _scene_change_text = new ArdourCanvas::Text (group);
                                /* move name label over */
                                _label_offset += r.x1;
                        }
                        
                        /* white with 95% opacity */
                        _scene_change_rect->set_outline_color (ArdourCanvas::rgba_to_color (1.0, 1.0, 1.0, 0.95));
                        _scene_change_rect->set_fill (false);
                        
                        _scene_change_text->set_font_description (name_font);
                        _scene_change_text->set_color (ArdourCanvas::rgba_to_color (1.0, 1.0, 1.0, 0.95));
                        _scene_change_text->set (X_("MIDI"));
                        
                        /* 4 pixels left margin, place it in the vertical middle.
                         */
                        _scene_change_text->set_position (ArdourCanvas::Duple (4.0, (_height / 2.0) - (name_height / 2.0)));
                        
                        r.y0 = _scene_change_text->position().y - 2.0;
                        r.y1 = r.y0 + name_height + 4.0;
                        
                        _scene_change_rect->set (r);
                        scene_change_width = r.x1;
                        
                } else {
                        if (_scene_change_text) {
                                delete _scene_change_text;
                                delete _scene_change_rect;
                                _scene_change_text = 0;
                                _scene_change_rect = 0;
                        }
                }
                
                double name_text_width = pixel_width (_name, name_font);
                
                name_width = min ((name_text_width + (2.0 * name_padding)), limit);
		_name_item->show ();
                _name_item->set_position (ArdourCanvas::Duple (_label_offset, (_height / 2.0) - (name_height / 2.0)));
		_name_item->clamp_width (name_width);
		_name_item->set (_name);
                
                _name_background->set_x0 (_name_item->position().x - _label_offset);
                _name_background->set_x1 (_name_background->x0() + name_width + scene_change_width);
        }
}

void
Marker::_set_position (framepos_t frame, framepos_t)
{
	frame_position = frame;
	unit_position = editor.sample_to_pixel (frame_position) - _shift;
	group->set_x_position (unit_position);
	setup_line ();
}

void
Marker::reposition ()
{
	set_position (frame_position);
}

void
Marker::show ()
{
	_shown = true;

        group->show ();
	setup_line ();
}

void
Marker::hide ()
{
	_shown = false;

	group->hide ();
	setup_line ();
}

void
Marker::use_color ()
{
       if (mark) {
                mark->set_fill_color (_color);
                mark->set_outline_color (_color);
        }

	if (_start_line) {
		_start_line->set_outline_color (_color);
	}

        if (_name_background) {
                _name_background->set_fill (true);
                _name_background->set_fill_color (_color);
                /* white with 20% opacity */
                _name_background->set_outline_color (ArdourCanvas::rgba_to_color (1.0, 1.0, 1.0, 0.20));
                
                _name_background->set_outline_what (ArdourCanvas::Rectangle::What (ArdourCanvas::Rectangle::TOP|
                                                                                   ArdourCanvas::Rectangle::LEFT|
                                                                                   ArdourCanvas::Rectangle::RIGHT));
        }
}

/** Set the number of pixels that are available for a label to the left of the centre of this marker */
void
Marker::set_left_label_limit (double p)
{
	_left_label_limit = p;

	if (_left_label_limit < 0) {
		_left_label_limit = 0;
	}

	if (label_on_left ()) {
		setup_name_display ();
	}
}

/** Set the number of pixels that are available for a label to the right of the centre of this marker */
void
Marker::set_right_label_limit (double p)
{
	_right_label_limit = p;

	if (_right_label_limit < 0) {
		_right_label_limit = 0;
	}

	if (!label_on_left ()) {
		setup_name_display ();
	}
}

void
Marker::set_has_scene_change (bool yn)
{
        _have_scene_change = yn;
        setup_name_display ();
}
                
/***********************************************************************/

TempoMarker::TempoMarker (PublicEditor& editor, ArdourCanvas::Container& parent, guint32 rgba, const string& text,
			  ARDOUR::TempoSection& temp)
	: Marker (0, editor, parent, Marker::marker_height(), rgba, text, Tempo, 0, false),
	  _tempo (temp)
{
	set_position (_tempo.frame());
	group->Event.connect (sigc::bind (sigc::mem_fun (editor, &PublicEditor::canvas_tempo_marker_event), group, this));
}

TempoMarker::~TempoMarker ()
{
}

/***********************************************************************/

MeterMarker::MeterMarker (PublicEditor& editor, ArdourCanvas::Container& parent, guint32 rgba, const string& text,
			  ARDOUR::MeterSection& m)
	: Marker (0, editor, parent, Marker::marker_height(), rgba, text, Meter, 0, false),
	  _meter (m)
{
	set_position (_meter.frame());
	group->Event.connect (sigc::bind (sigc::mem_fun (editor, &PublicEditor::canvas_meter_marker_event), group, this));
}

MeterMarker::~MeterMarker ()
{
}

