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
import inet.transport.udp.UDP;
import oversim.common.BootstrapList;
import oversim.common.CryptoModule;
import oversim.common.IOverlay;
import oversim.common.ITier;
import oversim.common.NeighborCache;
import inet.linklayer.ieee80211mesh.Ieee80211NewNicMesh;
import inet.networklayer.autorouting.ipv4.HostAutoConfigurator2;
import inet.linklayer.IWirelessNic;
import inet.linklayer.IWiredNic;
import inet.linklayer.ieee80211mesh.ethernet.EthernetInterfaceMesh;
//
// Access router that participates in the overlay
//
// @author Markus Mauch, Bernhard Heep
//
module OverlayAccessRouterWireless
{
    parameters:
        @node();
        string routingFile = default("");
        string overlayType; // overlay protocol compound module to use
        string tier1Type; // tier 1 application to use
        string tier2Type; // tier 2 module to use
        string tier3Type; // tier 3 module to use
        int numTiers; // number of tiers
        int numRadios = default(1);               // the number of radios in the router. by default no wireless
        bool IPForward = default(true);
        @display("bgb=775,464");
    gates:
        inout pppg[]; // gates from overlay
        inout ethg[]; // placeholder for zero-size vector
        input overlayNeighborArrowIn[]; // incoming gate for visualizing overlay neighborship with connection arrows
        output overlayNeighborArrowOut[]; // incoming gate for visualizing overlay neighborship with connection arrows
        input radioIn[numRadios+1] @directIn;
    submodules:
        autoconfigurator: HostAutoConfigurator2 {
            @display("p=434,47");
        }

        notificationBoard: NotificationBoard {
            parameters:
                @display("p=76,192;i=block/control");
        }
        interfaceTable: InterfaceTable {
            parameters:
                @display("p=76,262;i=block/table");
        }
        routingTable: RoutingTable {
            parameters:
                IPForward = true;
                routerId = "auto";
                routingFile = routingFile;
                @display("p=76,340;i=block/table");
        }
        accessNet: AccessNet {
            parameters:
                @display("p=76,402;i=block/cogwheeli");
        }
        tier3: <tier3Type> like ITier {
            parameters:
                @display("p=56,64;i=block/segm");
        }
        tier2: <tier2Type> like ITier {
            parameters:
                @display("p=139,88;i=block/segm");
        }
        tier1: <tier1Type> like ITier {
            parameters:
                @display("p=218,122;i=block/segm");
        }
        overlay: <overlayType> like IOverlay {
            parameters:
                @display("p=290,184;i=block/network2");
        }
        udp: UDP {
            parameters:
                @display("p=290,262;i=block/transport");
        }
        networkLayer: NetworkLayer {
            parameters:
                proxyARP = false;
                @display("p=434,262;i=block/fork;q=queue");
            gates:
                ifIn[sizeof(pppg)+sizeof(ethg)];
                ifOut[sizeof(pppg)+sizeof(ethg)];
        }
        ppp[sizeof(pppg)]: PPPInterface {
            parameters:
                @display("p=434,386,row,90;q=txQueue;i=block/ifcard");
        }


        wlanMesh: Ieee80211NewNicMesh {
            parameters:
                @display("p=257,386;q=queue");
        }

        wlan[numRadios]: <default("Ieee80211Nic")> like IWirelessNic {
            parameters:
                @display("p=561,386;q=queue");
        }
        ethMesh: EthernetInterfaceMesh {
            parameters:
                @display("p=336,386,row,90;q=txQueue");
        }
        eth[sizeof(ethg)-1]: <default("EthernetInterface")> like IWiredNic {
            parameters:
                @display("p=689,386,row,110;q=queue;i=block/ifcard");
        }
        neighborCache: NeighborCache {
            parameters:
                @display("p=168,262;i=block/table");
        }
        bootstrapList: BootstrapList {
            parameters:
                @display("p=168,340;i=block/table");
        }
        cryptoModule: CryptoModule {
            parameters:
                @display("p=168,402");
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

        if (sizeof(radioIn)>0) {
            radioIn[0] --> wlanMesh.radioIn;
            wlanMesh.upperLayerOut --> networkLayer.ifIn++;
            wlanMesh.upperLayerIn <-- networkLayer.ifOut++;
            ethMesh.wifiMeshIn <-- wlanMesh.toEthernet;
            ethMesh.wifiMeshOut --> wlanMesh.fromEthernet;
        }

        for i=0..sizeof(radioIn)-2 {
            radioIn[i+1] --> wlan[i].radioIn;
            wlan[i].upperLayerOut --> networkLayer.ifIn++;
            wlan[i].upperLayerIn <-- networkLayer.ifOut++;
        }


        ethg[0] <--> ethMesh.phys;
        ethMesh.netwOut --> networkLayer.ifIn++;
        ethMesh.netwIn <-- networkLayer.ifOut++;

        for i=0..sizeof(ethg)-2 {
            ethg[i+1] <--> eth[i].phys;
            eth[i].netwOut --> networkLayer.ifIn++;
            eth[i].netwIn <-- networkLayer.ifOut++;
        }

        for i=0..sizeof(pppg)-1 {
            pppg[i] <--> ppp[i].phys;
            ppp[i].netwOut --> networkLayer.ifIn[i];
            ppp[i].netwIn <-- networkLayer.ifOut[i];
        }


}

