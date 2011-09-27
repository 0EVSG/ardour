/*
    Copyright (C) 2002-2004 Paul Davis 

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

#ifndef __ardour_audioengine_h__
#define __ardour_audioengine_h__

#include <list>
#include <set>
#include <cmath>
#include <exception>
#include <string>

#include <sigc++/signal.h>

#include <glibmm/thread.h>

#include <jack/weakjack.h>
#include <jack/jack.h>
#include <jack/transport.h>

#include <pbd/rcu.h>

#include <ardour/ardour.h>
#include <ardour/types.h>
#include <ardour/data_type.h>

namespace ARDOUR {

class Session;
class Port;

class AudioEngine : public sigc::trackable
{
   public:
	AudioEngine (std::string client_name);
	virtual ~AudioEngine ();
	
	jack_client_t* jack() const { return _jack; }
	bool connected() const { return _jack != 0; }

	bool is_realtime () const;

	std::string client_name() const { return jack_client_name; }

	int reconnect_to_jack ();
	int disconnect_from_jack();

	bool will_reconnect_at_halt ();
	void set_reconnect_at_halt (bool);

	int stop (bool forever = false);
	int start ();
	bool running() const { return _running; }

	Glib::Mutex& process_lock() { return _process_lock; }

	nframes_t frame_rate();
	nframes_t frames_per_cycle();

	int usecs_per_cycle () const { return _usecs_per_cycle; }

	bool get_sync_offset (nframes_t& offset) const;

	nframes_t frames_since_cycle_start () {
  	        jack_client_t* _priv_jack = _jack;
		if (!_running || !_priv_jack) return 0;
		return jack_frames_since_cycle_start (_priv_jack);
	}
	nframes_t frame_time () {
  	        jack_client_t* _priv_jack = _jack;
		if (!_running || !_priv_jack) return 0;
		return jack_frame_time (_priv_jack);
	}

	nframes_t transport_frame () const {
  	        const jack_client_t* _priv_jack = _jack;
		if (!_running || !_priv_jack) return 0;
		return jack_get_current_transport_frame (_priv_jack);
	}
	
	int request_buffer_size (nframes_t);
	
	nframes_t set_monitor_check_interval (nframes_t);
	nframes_t processed_frames() const { return _processed_frames; }

	float get_cpu_load() { 
  	        jack_client_t* _priv_jack = _jack;
		if (!_running || !_priv_jack) return 0;
		return jack_cpu_load (_priv_jack);
	}

	void set_session (Session *);
	void remove_session ();

	class PortRegistrationFailure : public std::exception {
	  public:
		PortRegistrationFailure (const char* why) {
			reason = why;
		}
		virtual const char *what() const throw() { return reason; }

	  private:
		const char* reason;
	};

	class NoBackendAvailable : public std::exception {
	  public:
		virtual const char *what() const throw() { return "could not connect to engine backend"; }
	};

	Port *register_input_port (DataType type, const std::string& portname);
	Port *register_output_port (DataType type, const std::string& portname);
	int   unregister_port (Port *);
	
	int connect (const std::string& source, const std::string& destination);
	int disconnect (const std::string& source, const std::string& destination);
	int disconnect (Port *);
	
	const char ** get_ports (const std::string& port_name_pattern, const std::string& type_name_pattern, uint32_t flags);

	bool can_request_hardware_monitoring ();

	uint32_t n_physical_audio_outputs () const;
	uint32_t n_physical_audio_inputs () const;

	void get_physical_audio_outputs (std::vector<std::string>&);
	void get_physical_audio_inputs (std::vector<std::string>&);

	std::string get_nth_physical_audio_output (uint32_t n) {
		return get_nth_physical_audio (n, JackPortIsInput);
	}

	std::string get_nth_physical_audio_input (uint32_t n) {
		return get_nth_physical_audio (n, JackPortIsOutput);
	}

	/* the caller may not delete the object pointed to by 
	   the return value for either of these functions.
	*/

	Port *get_port_by_name (const std::string& name, bool keep = true);
        Port *get_ardour_port_by_name_unlocked (const string& portname);

	enum TransportState {
		TransportStopped = JackTransportStopped,
		TransportRolling = JackTransportRolling,
		TransportLooping = JackTransportLooping,
		TransportStarting = JackTransportStarting
	};

	void transport_start ();
	void transport_stop ();
	void transport_locate (nframes_t);
	TransportState transport_state ();

	int  reset_timebase ();

        void update_latencies ();

	/* start/stop freewheeling */

	int freewheel (bool onoff);
	bool freewheeling() const { return _freewheeling; }

	/* this signal is sent for every process() cycle while freewheeling.
_	   the regular process() call to session->process() is not made.
	*/

	sigc::signal<int,nframes_t> Freewheel;

	sigc::signal<void> Xrun;

	/* this signal is if JACK notifies us of a graph order event */

	sigc::signal<void> GraphReordered;

	/* this signal is emitted if the sample rate changes */

	sigc::signal<void,nframes_t> SampleRateChanged;

	/* this signal is sent if JACK ever disconnects us */

	sigc::signal<void,const char*> Halted;

	/* these two are emitted when the engine itself is
	   started and stopped
	*/

	sigc::signal<void> Running;
	sigc::signal<void> Stopped;

	std::string make_port_name_relative (std::string);
	std::string make_port_name_non_relative (std::string);
        bool port_is_mine (const std::string& portname) const;

	static AudioEngine* instance() { return _instance; }
	void died ();

  private:
	static AudioEngine*       _instance;

	ARDOUR::Session*           session;
	jack_client_t* volatile   _jack; /* could be reset to null by SIGPIPE or another thread */
	std::string                jack_client_name;
	Glib::Mutex               _process_lock;
	Glib::Cond                 session_removed;
	bool                       session_remove_pending;
	bool                      _running;
	bool                      _has_run;
	nframes_t                 _buffer_size;
	nframes_t                 _frame_rate;
	nframes_t                  monitor_check_interval;
	nframes_t                  last_monitor_check;
	nframes_t                 _processed_frames;
	bool                      _freewheeling;
	bool                      _freewheel_pending;
	bool                      _freewheel_thread_registered;
	sigc::slot<int,nframes_t>  freewheel_action;
	bool                       reconnect_on_halt;
	int                       _usecs_per_cycle;

	typedef std::set<Port*> Ports;
	SerializedRCUManager<Ports> ports;

	int    process_callback (nframes_t nframes);
	void   remove_all_ports ();

	typedef std::pair<std::string,std::string> PortConnection;
	typedef std::list<PortConnection> PortConnections;

	PortConnections port_connections;
	void   remove_connections_for (Port*);

	std::string get_nth_physical_audio (uint32_t which, int flags);

	void port_registration_failure (const std::string& portname);

	static int  _xrun_callback (void *arg);
	static int  _graph_order_callback (void *arg);
	static int  _process_callback (nframes_t nframes, void *arg);
	static int  _sample_rate_callback (nframes_t nframes, void *arg);
	static int  _bufsize_callback (nframes_t nframes, void *arg);
	static void _jack_timebase_callback (jack_transport_state_t, nframes_t, jack_position_t*, int, void*);
	static int  _jack_sync_callback (jack_transport_state_t, jack_position_t*, void *arg);
	static void _freewheel_callback (int , void *arg);
        static void _latency_callback (jack_latency_callback_mode_t mode, void* arg);
        
        void jack_latency_callback (jack_latency_callback_mode_t mode);
	void jack_timebase_callback (jack_transport_state_t, nframes_t, jack_position_t*, int);
	int  jack_sync_callback (jack_transport_state_t, jack_position_t*);
	int  jack_bufsize_callback (nframes_t);
	int  jack_sample_rate_callback (nframes_t);

	int connect_to_jack (std::string client_name);

	static void halted (void *);
	static void halted_info (jack_status_t, const char*, void *);

	void meter_thread ();
	void start_metering_thread ();
	void stop_metering_thread ();

	Glib::Thread*    m_meter_thread;
	static gint      m_meter_exit;
};

} // namespace ARDOUR

#endif /* __ardour_audioengine_h__ */
