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

package oversim.underlay.inetunderlay.ipv6;

import inet.base.NotificationBoard;
import inet.linklayer.ppp.PPPInterface;
import inet.networklayer.common.InterfaceTable;
import inet.networklayer.ipv6.RoutingTable6;
import inet.nodes.ipv6.NetworkLayer6;
import inet.transport.udp.UDP;
import inet.transport.tcp.TCP;
import oversim.common.BootstrapList;
import oversim.common.CryptoModule;
import oversim.common.IOverlay;
import oversim.common.ITier;
import oversim.common.NeighborCache;


//
// Host that participates in the overlay
//
// @author Markus Mauch, Bernhard Heep
//
module InetOverlayHost6
{
    parameters:
        string overlayType; // overlay protocol compound module to use
        string tier1Type; // tier 1 application to use
        string tier2Type; // tier 2 module to use
        string tier3Type; // tier 3 module to use
        int numTiers; // number of tiers
        bool IPForward = default(false);
        @display("bgb=537,472;i=device/wifilaptop_l;i2=block/circle_s");
    gates:
        inout pppg[]; // gates from router
        inout ethg[]; // placeholder for zero-size vector
        input overlayNeighborArrowIn[]; // incoming gate for visualizing overlay neighborship with connection arrows
        output overlayNeighborArrowOut[]; // incoming gate for visualizing overlay neighborship with connection arrows

    submodules:
        notificationBoard: NotificationBoard {
            parameters:
                @display("p=68,233");
        }
        interfaceTable: InterfaceTable {
            parameters:
                @display("p=68,317");
        }
        routingTable6: RoutingTable6 {
            parameters:
                isRouter = false;
                @display("p=68,414");
        }
        tier3: <tier3Type> like ITier {
            parameters:
                @display("p=89,40");
        }
        tier2: <tier2Type> like ITier {
            parameters:
                @display("p=199,65");
        }
        tier1: <tier1Type> like ITier {
            parameters:
                @display("p=289,97");
        }
        overlay: <overlayType> like IOverlay {
            parameters:
                @display("p=374,156");
        }
        udp: UDP {
            parameters:
                @display("p=374,247");
        }
        tcp: TCP {
            parameters:
                @display("p=289,247");
        }
        networkLayer: NetworkLayer6 {
            parameters:
                @display("p=374,332;q=queue");
            gates:
                ifIn[sizeof(pppg)+sizeof(ethg)];
                ifOut[sizeof(pppg)+sizeof(ethg)];
        }
        ppp[sizeof(pppg)]: PPPInterface {
            parameters:
                @display("p=374,414,row,90;q=txQueue");
        }
        neighborCache: NeighborCache {
            parameters:
                @display("p=168,233");
        }
        bootstrapList: BootstrapList {
            parameters:
                @display("p=168,317");
        }
        cryptoModule: CryptoModule {
            parameters:
                @display("p=168,414");
        }
    connections allowunconnected:
        tier1.to_lowerTier --> overlay.appIn if numTiers>0;
        tier1.from_lowerTier <-- overlay.appOut if numTiers>0;
        tier1.udpOut --> udp.appIn++ if numTiers>0;
        udp.appOut++ --> tier1.udpIn if numTiers>0;
        tier1.tcpOut --> tcp.appIn++ if numTiers>0;
        tcp.appOut++ --> tier1.tcpIn if numTiers>0;

        tier2.to_lowerTier --> tier1.from_upperTier if numTiers > 1;
        tier2.from_lowerTier <-- tier1.to_upperTier if numTiers > 1;
        tier2.udpOut --> udp.appIn++ if numTiers>1;
        udp.appOut++ --> tier2.udpIn if numTiers>1;
        tier2.tcpOut --> tcp.appIn++ if numTiers>1;
        tcp.appOut++ --> tier2.tcpIn if numTiers>1;

        tier3.to_lowerTier --> tier2.from_upperTier if numTiers > 2;
        tier3.from_lowerTier <-- tier2.to_upperTier if numTiers > 2;
        tier3.udpOut --> udp.appIn++ if numTiers>2;
        udp.appOut++ --> tier3.udpIn if numTiers>2;
        tier3.tcpOut --> tcp.appIn++ if numTiers>2;
        tcp.appOut++ --> tier3.tcpIn if numTiers>2;

        overlay.udpOut --> udp.appIn++;
        overlay.udpIn <-- udp.appOut++;

        overlay.tcpOut --> tcp.appIn++;
        overlay.tcpIn <-- tcp.appOut++;

        bootstrapList.udpOut --> udp.appIn++;
        bootstrapList.udpIn <-- udp.appOut++;

        udp.ipv6Out --> networkLayer.udpIn;
        udp.ipv6In <-- networkLayer.udpOut;

        tcp.ipv6Out --> networkLayer.tcpIn;
        tcp.ipv6In <-- networkLayer.tcpOut;

        // connections to network outside
        for i=0..sizeof(pppg)-1 {
            pppg[i] <--> ppp[i].phys;
            ppp[i].upperLayerOut --> networkLayer.ifIn[i];
            ppp[i].upperLayerIn <-- networkLayer.ifOut[i];
        }

}

