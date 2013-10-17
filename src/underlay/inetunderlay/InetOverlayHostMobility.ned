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
import inet.linklayer.ppp.PPPInterface;
import inet.networklayer.common.InterfaceTable;
import inet.networklayer.ipv4.RoutingTable;
import inet.nodes.inet.NetworkLayer;
import inet.transport.udp.UDP;
import oversim.common.BootstrapList;
import oversim.common.CryptoModule;
import oversim.common.IOverlay;
import oversim.common.ITier;
import oversim.common.NeighborCache;
import inet.base.MMapBoard;

import inet.networklayer.IManetRouting;
import inet.nodes.inet.WirelessHost;
import inet.networklayer.autorouting.ipv4.HostAutoConfigurator2;
import inet.mobility.IMobility;
import inet.linklayer.IWirelessNic;
import inet.linklayer.ieee80211mesh.Ieee80211NicMeshMulti;


//
// Host that participates in the overlay
//
// @author Markus Mauch, Bernhard Heep
//
module InetOverlayHostMobility
{
    parameters:
        string overlayType; // overlay protocol compound module to use
        string tier1Type; // tier 1 application to use
        string tier2Type; // tier 2 module to use
        string tier3Type; // tier 3 module to use
        int numTiers; // number of tiers
        string routingFile = default("");
        @display("bgb=408,472;i=device/wifilaptop_l;i2=block/circle_s");
        @node();

        string mobilityType = default("StationaryMobility");
        int meshChannels = default(1);

    gates:
        //inout pppg[]; // gates from router
        //inout ethg[]; // placeholder for zero-size vector
        input overlayNeighborArrowIn[]; // incoming gate for visualizing overlay neighborship with connection arrows
        output overlayNeighborArrowOut[]; // incoming gate for visualizing overlay neighborship with connection arrows

        input radioIn[meshChannels] @directIn;

    submodules:
        autoconfigurator: HostAutoConfigurator2 {
            interfaces = default("wlan");         // list of interfaces to autoassign addresses to, separated by a single space characters
            defaultInterface = default("wlan"); // only if a node has been defined like   isDefaultRoute=true for all nodes
            defaultAddressInterface = default("wlan"); // only if a node has been defined like   isDefaultRoute=true, is the interface that will be used for search the ip address, only for the default
            @display("p=53,162");
        }

        notificationBoard: NotificationBoard {
            parameters:
                @display("p=68,208");
        }
        interfaceTable: InterfaceTable {
            parameters:
                @display("p=68,278");
        }
        routingTable: RoutingTable {
            parameters:
                IPForward = true;
                routerId = "";
                routingFile = "";
                @display("p=68,348");
        }
        tier3: <tier3Type> like ITier {
            parameters:
                @display("p=56,64");
        }
        tier2: <tier2Type> like ITier {
            parameters:
                @display("p=139,80");
        }
        tier1: <tier1Type> like ITier {
            parameters:
                @display("p=210,114");
        }
        overlay: <overlayType> like IOverlay {
            parameters:
                @display("p=274,176");
        }
        udp: UDP {
            parameters:
                @display("p=274,254");
        }
        networkLayer: NetworkLayer {
            parameters:
                proxyARP = true; // sets proxy \ARP mode (replying to \ARP requests for the addresses for which a routing table entry exists)
                @display("p=274,332;q=queue");
            gates:
                ifIn[sizeof(pppg)+sizeof(ethg)+1];
                ifOut[sizeof(pppg)+sizeof(ethg)+1];
        }
        //ppp[sizeof(pppg)]: PPPInterface {
        //    parameters:
        //        @display("p=274,414,row,90;q=txQueue");
        //}
        neighborCache: NeighborCache {
            parameters:
                @display("p=168,208");
        }
        bootstrapList: BootstrapList {
            parameters:
                @display("p=168,280");
        }
        cryptoModule: CryptoModule {
            parameters:
                @display("p=168,348");
        }
        mobility: <mobilityType> like IMobility if mobilityType != "" && meshChannels > 0 {
            parameters:
                @display("p=30,62");
        }
        wlan: Ieee80211NicMeshMulti {
            parameters:
                meshChannels = meshChannels;
                @display("p=274,407;q=queue");
        }


        //manetrouting: <routingProtocol> like IManetRouting if routingProtocol != "" &&  LinkLayerType != "Ieee80211NicMesh" {

        //    @display("p=358,395");
        //}

        mmapBoard: MMapBoard {
            parameters:
                @display("p=68,414;i=block/blackboard");
        }




    connections allowunconnected:
        tier1.to_lowerTier --> overlay.appIn if numTiers>0;
        tier1.from_lowerTier <-- overlay.appOut if numTiers>0;
        tier1.udpOut --> udp.appIn++ if numTiers>0;
        udp.appOut++ --> tier1.udpIn if numTiers>0;

        tier2.to_lowerTier --> tier1.from_upperTier if numTiers > 1;
        tier2.from_lowerTier <-- tier1.to_upperTier if numTiers > 1;
        tier2.udpOut --> udp.appIn++ if numTiers>1;
        udp.appOut++ --> tier2.udpIn if numTiers>1;

        tier3.to_lowerTier --> tier2.from_upperTier if numTiers > 2;
        tier3.from_lowerTier <-- tier2.to_upperTier if numTiers > 2;
        tier3.udpOut --> udp.appIn++ if numTiers>2;
        udp.appOut++ --> tier3.udpIn if numTiers>2;

        overlay.udpOut --> udp.appIn++;
        overlay.udpIn <-- udp.appOut++;

        bootstrapList.udpOut --> udp.appIn++;
        bootstrapList.udpIn <-- udp.appOut++;

        udp.ipOut --> networkLayer.udpIn;
        udp.ipIn <-- networkLayer.udpOut;

        // connections to network outside
        //for i=0..sizeof(pppg)-1 {
        //    pppg[i] <--> ppp[i].phys;
        //    ppp[i].upperLayerOut --> networkLayer.ifIn[i];
        //    ppp[i].upperLayerIn <-- networkLayer.ifOut[i];
        //}

        for i=0..sizeof(meshChannels)-1 {
            radioIn[i] --> wlan.radioIn[i];

        }
        wlan.upperLayerOut --> networkLayer.ifIn[sizeof(pppg)];
        wlan.upperLayerIn <-- networkLayer.ifOut[sizeof(pppg)];

        //networkLayer.manetOut --> manetrouting.from_ip if routingProtocol != "";
        //networkLayer.manetIn <-- manetrouting.to_ip if routingProtocol != "";

}

