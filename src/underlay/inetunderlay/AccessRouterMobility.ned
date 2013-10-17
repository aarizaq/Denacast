//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//

package oversim.underlay.inetunderlay;

import inet.base.NotificationBoard;
import inet.linklayer.ethernet.EthernetInterface;
import inet.linklayer.ppp.PPPInterface;
import inet.networklayer.common.InterfaceTable;
import inet.networklayer.ipv4.RoutingTable;
import inet.nodes.inet.NetworkLayer;

import inet.networklayer.IManetRouting;
import inet.nodes.inet.WirelessHost;
import inet.networklayer.autorouting.ipv4.HostAutoConfigurator2;
import inet.mobility.IMobility;
import inet.linklayer.IWirelessNic;
import inet.linklayer.ieee80211mesh.Ieee80211NicMeshMulti;


//
// Access router connects the access nets to the network backbone
//
module AccessRouterMobility
{
    parameters:
        @node();
        string routingFile = default("");
        
		string mobilityType = default("StationaryMobility");
        int meshChannels = default(1);
        
        
        
    gates:
        inout pppg[];    // gates from overlay
        inout ethg[];    // unused here
		input radioIn[meshChannels] @directIn;
    submodules:

        notificationBoard: NotificationBoard {
            parameters:
                @display("p=60,60;i=block/control");
        }
        interfaceTable: InterfaceTable {
            parameters:
                @display("p=150,60;i=block/table");
        }
        routingTable: RoutingTable {
            parameters:
                IPForward = true;
                routerId = "auto";
                routingFile = routingFile;
                @display("p=240,60;i=block/table");
        }
        accessNet: AccessNet {
            parameters:
                @display("p=330,60");
        }
        networkLayer: NetworkLayer {
            parameters:
                @display("p=199,141;i=block/fork;q=queue");
            gates:
                ifIn[sizeof(pppg)+sizeof(ethg)+1];
                ifOut[sizeof(pppg)+sizeof(ethg)+1];
        }
        ppp[sizeof(pppg)]: PPPInterface {
            parameters:
                @display("p=90,257,row,110;q=queue;i=block/ifcard");
        }
        eth[sizeof(ethg)]: EthernetInterface {
            parameters:
                @display("p=286,268,row,110;q=queue;i=block/ifcard");
        }
        wlan: Ieee80211NicMeshMulti {
            parameters:
                meshChannels = meshChannels;
                @display("p=159,386;q=queue");
        }

        //manetrouting: <routingProtocol> like IManetRouting if routingProtocol != "" &&  LinkLayerType != "Ieee80211NicMesh" {
        //    @display("p=522,307");
        //}
        mobility: <mobilityType> like IMobility if mobilityType != "" && meshChannels > 0 {
            parameters:
                @display("p=30,62");
        }


        
    connections allowunconnected:
        // connections to network outside
        for i=0..sizeof(pppg)-1 {
            pppg[i] <--> ppp[i].phys;
            ppp[i].upperLayerOut --> networkLayer.ifIn[i];
            ppp[i].upperLayerIn <-- networkLayer.ifOut[i];
        }

        for i=0..sizeof(ethg)-1 {
            ethg[i] <--> eth[i].phys;
            eth[i].upperLayerOut --> networkLayer.ifIn[sizeof(pppg)+i];
            eth[i].upperLayerIn <-- networkLayer.ifOut[sizeof(pppg)+i];
        }
        
        for i=0..sizeof(meshChannels)-1 {
            radioIn[i] --> wlan.radioIn[i];

        }
        wlan.upperLayerOut --> networkLayer.ifIn[sizeof(pppg)+sizeof(ethg)];
        wlan.upperLayerIn <-- networkLayer.ifOut[sizeof(pppg)+sizeof(ethg)];

        //networkLayer.manetOut --> manetrouting.from_ip if routingProtocol != "";
        //networkLayer.manetIn <-- manetrouting.to_ip if routingProtocol != "";
}

