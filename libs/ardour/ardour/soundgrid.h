/*
    Copyright (C) 2011 Paul Davis

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

#ifndef __ardour_soundgrid_h__
#define __ardour_soundgrid_h__

#include <vector>
#include <string>

#include <glib.h>

#include <boost/utility.hpp>
#include <boost/function.hpp>

#include <WavesPublicAPI/WTErr.h>
#include <WavesPublicAPI/WavesMixerAPI/1.0/WavesMixerAPI.h>

#include "pbd/signals.h"

#include "ardour/ardour.h"

namespace ARDOUR {

class SoundGrid : public boost::noncopyable
{
  public:
	~SoundGrid ();

        int initialize (void* window_handle);
        int teardown ();

	static SoundGrid& instance();
	static bool available ();
	static std::vector<std::string> lan_port_names();
        static std::string current_lan_port_name();
	static std::string coreaudio_device_name ();
	static uint32_t current_network_buffer_size ();
	static std::vector<uint32_t> possible_network_buffer_sizes ();

        static int set_parameters (const std::string& device, int sr, int bufsize);

	struct InventoryItem {
	    virtual ~InventoryItem() {}; /* force virtual so that we can use dynamic_cast<> */

	    uint32_t    assign;
	    std::string name;
	    std::string mac;
            std::string hostname;
	    uint32_t    channels;
	    std::string device;
	    std::string status;
	};

	struct SGSInventoryItem : public InventoryItem {
	};

	struct IOInventoryItem : public InventoryItem {
                int32_t sample_rate;
                int32_t output_channels;
	};

	typedef std::vector<InventoryItem*> Inventory;
	static void update_inventory (Inventory&);
	static void clear_inventory (Inventory&);

        static void driver_register (const WSDCoreHandle, const WSCoreCallbackTable*, const WSMixerConfig*);

        static void set_pool (void* pool);
        
        static void finalize (void* ecc, int state);

        static PBD::Signal0<void> Shutdown;
        
  private:
	SoundGrid ();
	static SoundGrid* _instance;
        
	void* dl_handle;
	void* _sg; // handle managed by SoundGrid library
        WSDCoreHandle        _host_handle;
        WSCoreCallbackTable  _callback_table;
        WSMixerConfig        _mixer_config;
        void* _pool;

	void display_update ();
	static void _display_update ();

        void _finalize (void* ecc, int state);
        void event_completed (int);

        static WTErr _sg_callback (const WSControlID* pControlID);
        static WTErr sg_callback (const WSControlID* pControlID);

        WTErr get (WSControlID*, WSControlInfo*);
        WTErr set (WSEvent*, const std::string&);

        struct EventCompletionClosure {
            std::string name;
            boost::function<void(int)> func;
            uint64_t id;

            EventCompletionClosure (const std::string& n, boost::function<void(int)> f) 
            : name (n), func (f), id (g_atomic_int_add (&id_counter, 1) + 1) {}

            static int id_counter; 
        };
};

} // namespace ARDOUR

#endif /* __ardour_soundgrid__ */
