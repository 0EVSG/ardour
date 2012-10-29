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
#include <WavesMixerAPI/1.0/WavesMixerAPI.h>

#include "pbd/signals.h"

#include "ardour/ardour.h"

namespace ARDOUR {

class SoundGrid : public boost::noncopyable
{
  public:
	~SoundGrid ();

        int initialize (void* window_handle, uint32_t max_tracks);
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

        void finalize (void* ecc, int state);
        void command_status_update (struct WSCommand*);

        static PBD::Signal0<void> Shutdown;
        
        bool add_rack_synchronous (uint32_t clusterType, int32_t process_group, uint32_t channels, uint32_t& trackHandle);
        bool add_rack_asynchronous (uint32_t clusterType, int32_t process_group, uint32_t channels);
        bool remove_rack_synchronous (uint32_t clusterType, uint32_t trackHandle);
        bool remove_all_racks_synchronous ();

        int set_gain (uint32_t in_clusterType, uint32_t in_trackHandle, double in_gainValue);
        bool get_gain (uint32_t in_clusterType, uint32_t in_trackHandle, double &out_gainValue);

        int configure_driver (uint32_t physical_inputs, uint32_t physical_outputs, uint32_t tracks);

        struct Port {
            
            enum Position {
                    Pre,
                    Post
            };

            uint32_t ctype;
            uint32_t cid;
            uint32_t stype;
            uint32_t sindex;
            uint32_t sid;
            uint32_t channel;
            Position position;

            void set_source (WSAudioAssignment& assignment) const;
            void set_destination (WSAudioAssignment& assignment) const;

            /* control sorting and use in STL containers */
            bool operator<(const Port& other) const {
                    return ctype < other.ctype && 
                            stype < other.stype && 
                            cid < other.cid && 
                            sid < other.sid && 
                            sindex < other.sindex && 
                            channel < other.channel && 
                            position < other.position;
            }
            
            bool accepts_input () const {
                    if (ctype == eClusterType_Inputs) {
                            return true;
                    } else if (ctype == eClusterType_Outputs) {
                            return false;
                    } else {
                            if (position == Post) {
                                    return false;
                            }
                    }
                    return true;
            }

            EASGNSource sg_source () const { 
                    switch (position) {
                    case Pre:
                            return kASGNPre;
                    default:
                    case Post:
                            return kASGNPost;
                    }
            }

          protected:
            Port (uint32_t ct, uint32_t ci, uint32_t st, uint32_t si, uint32_t sd, uint32_t c, Position p) 
                    : ctype (ct)
                    , cid (ci)
                    , stype (st)
                    , sindex (si)
                    , sid (sd)
                    , channel (c)
                    , position (p) {}
        };

        struct DriverInputPort : public Port {
                DriverInputPort (uint32_t channel) 
                        : Port (eClusterType_Inputs, eClusterHandle_Physical_Driver, 
                                wvEnum_Unknown, wvEnum_Unknown, wvEnum_Unknown, channel, Port::Pre) {}
        };
        
        struct DriverOutputPort : public Port {
                DriverOutputPort (uint32_t channel) 
                        : Port (eClusterType_Outputs, eClusterHandle_Physical_Driver, 
                                wvEnum_Unknown, wvEnum_Unknown, wvEnum_Unknown, channel, Port::Post) {}
        };
        
        struct PhysicalInputPort : public Port {
                PhysicalInputPort (uint32_t channel) 
                        : Port (eClusterType_Inputs, eClusterHandle_Physical_IO, 
                                wvEnum_Unknown, wvEnum_Unknown, wvEnum_Unknown, channel, Port::Pre) {}
        };
        
        struct PhysicalOutputPort : public Port {
                PhysicalOutputPort (uint32_t channel) 
                        : Port (eClusterType_Outputs, eClusterHandle_Physical_IO, 
                                wvEnum_Unknown, wvEnum_Unknown, wvEnum_Unknown, channel, Port::Post) {}
        };
        
        struct TrackInputPort : public Port {
                TrackInputPort (uint32_t chainer_id, uint32_t channel) 
                        : Port (eClusterType_InputTrack, chainer_id, 
                                eControlType_Input, 0, eControlID_Input_Digital_Trim, channel, Port::Pre) {}
        };

        struct TrackOutputPort : public Port {
                TrackOutputPort (uint32_t chainer_id, uint32_t channel) 
                        : Port (eClusterType_InputTrack, chainer_id,
                                eControlType_Output, 0, eControlID_Output_Gain, channel, Port::Pre) {}
        };

        struct BusInputPort : public Port {
                BusInputPort (uint32_t chainer_id, uint32_t channel) 
                        : Port (eClusterType_GroupTrack, chainer_id, 
                                eControlType_Input, 0, eControlID_Input_Digital_Trim, channel, Port::Pre) {}
        };

        struct BusOutputPort : public Port {
                BusOutputPort (uint32_t chainer_id, uint32_t channel) 
                        : Port (eClusterType_GroupTrack, chainer_id,
                                eControlType_Output, 0, eControlID_Output_Gain, channel, Port::Pre) {}
        };
        
        
        int connect (const Port& src, const Port& dst);
        int disconnect (const Port& src, const Port& dst);

        std::string sg_port_as_jack_port (const Port& port);
        bool        jack_port_as_sg_port (const std::string& jack_port, Port& result);

        void parameter_updated (WEParamType paramID);

  private:
	SoundGrid ();
	static SoundGrid* _instance;

        typedef std::map<std::string,SoundGrid::Port> JACK_SGMap;
        typedef std::map<SoundGrid::Port,std::string> SG_JACKMap;
        
        JACK_SGMap jack_soundgrid_map;
        SG_JACKMap soundgrid_jack_map;

	void* dl_handle;
	void* _sg; // handle managed by SoundGrid library
        WSDCoreHandle        _host_handle;
        WSCoreCallbackTable  _callback_table;
        WSMixerConfig        _mixer_config;

	void display_update ();
	static void _display_update ();

        void _finalize (void* ecc, int state);
        void event_completed (int);

        static WTErr _sg_callback (void*, const WSControlID* pControlID);
        WTErr sg_callback (const WSControlID* pControlID);

        WTErr get (WSControlID*, WSControlInfo*);
        WTErr set (WSEvent*, const std::string&);
        WTErr command (WSCommand*);

        struct EventCompletionClosure {
            std::string name;
            boost::function<void(int)> func;
            uint64_t id;

            EventCompletionClosure (const std::string& n, boost::function<void(int)> f) 
            : name (n), func (f), id (g_atomic_int_add (&id_counter, 1) + 1) {}

            static int id_counter; 
        };

        struct CommandStatusClosure {
            std::string name;
            boost::function<void(WSCommand*)> func;
            uint64_t id;
            
            CommandStatusClosure (const std::string& n, boost::function<void(WSCommand*)> f) 
            : name (n), func (f), id (g_atomic_int_add (&id_counter, 1) + 1) {}
            
            static int id_counter; 
        };

        void assignment_complete (WSCommand* cmd);
        bool get_driver_config (uint32_t& max_inputs, 
                                uint32_t& max_outputs,
                                uint32_t& current_inputs,
                                uint32_t& current_outputs);
        int connect_io_to_driver (uint32_t ninputs, uint32_t noutputs);
                
        uint32_t          _driver_ports; // how many total channels we tell the SG driver to allocate
        std::vector<bool> _driver_input_ports_in_use;
        std::vector<bool> _driver_output_ports_in_use;
};

} // namespace ARDOUR

std::ostream& operator<< (std::ostream& out, const ARDOUR::SoundGrid::Port& p);

#endif /* __ardour_soundgrid__ */
