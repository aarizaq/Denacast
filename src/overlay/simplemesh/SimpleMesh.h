//
// Copyright (C) 2010 DenaCast All Rights Reserved.
// http://www.denacast.com
// The DenaCast was designed and developed at the DML(Digital Media Lab http://dml.ir/)
// under supervision of Dr. Behzad Akbari (http://www.modares.ac.ir/ece/b.akbari)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
//

/**
 * @file SimpleMesh.h
 * @author Yasser Seyyedi, Behnam Ahmadifar
 */

#ifndef SIMPLEMESH_H_
#define SIMPLEMESH_H_

#include <omnetpp.h>
#include <OverlayKey.h>
#include <TransportAddress.h>
#include <GlobalNodeList.h>
#include "SimpleMeshMessage_m.h"
#include <DenaCastOverlay.h>


class SimpleMesh : public DenaCastOverlay
{
protected:
    bool isSource;/**< whether this node is server*/
    int activeNeighbors;/**< Neighbor that node connects to them*/
    int passiveNeighbors; /**< Neighbors that node let them connect to it*/
    int neighborNum; /**< Sum of active and passive neighbors*/
    double neighborNotificationPeriod; /**< period to notify tracker about number of current neighbor*/
    double videoAverageRate; /**< average video bit rate for calculating neighbor num*/
    bool adaptiveNeighboring; /**< true if we want to have adaptive neighbors*/
    bool isRegistered; /**< if this node registered in the tracker */
    bool serverGradualNeighboring; /**< true if gradual neighbor is required for source node*/
    TransportAddress trackerAddress; /**< Transport address of tracker node */
    std::map <TransportAddress,double> neighborTimeOut; /**< */
    /**
     * Register node in the tracker
     */
    void selfRegister();
    /**
     * Unregister node in the tracker
     */
    void selfUnRegister();
    /**
     * for leaving this method do this process for node that want to leave with notification
     * @param TransportAdress
     */
    void disconnectProcess(TransportAddress node);

    //selfMessages
    cMessage* meshJoinRequestTimer; /**< self message for scheduling neighboring*/
    cMessage* remainNotificationTimer; /**< self message for scheduling send notification to server*/
    cMessage* serverNeighborTimer; /**< for gradual neighboring this self message plan for this job */


    // statistics
	uint32_t stat_TotalUpBandwidthUsage;
	uint32_t stat_TotalDownBandwidthUsage;

    uint32_t stat_joinREQ; /**< number of sent join request messages */
	uint32_t stat_joinREQBytesSent;  /**< number of sent bytes of join request messages */
	uint32_t stat_joinRSP; /**< number of sent join response messages */
	uint32_t stat_joinRSPBytesSent; /**< number of sent bytes of join response messages */
	uint32_t stat_joinACK; /**< number of sent join acknowledge messages */
	uint32_t stat_joinACKBytesSent; /**< number of sent bytes of join acknowledge messages */
	uint32_t stat_joinDNY; /**< number of sent join deny messages */
	uint32_t stat_joinDNYBytesSent; /**< number of sent bytes of join deny messages */
	uint32_t stat_disconnectMessages; /**< number of sent disconnect messages */
	uint32_t stat_disconnectMessagesBytesSent; /**< number of sent bytes of disconnect messages */
	uint32_t stat_addedNeighbors; /**< number of added neighbors during life cycle of this node */
	uint32_t stat_nummeshJoinRequestTimer; /**< number of meshJoinRequestTimer self messages */

public:

    /**
     * initializes base class-attributes
     *
     * @param stage the init stage
     */
	virtual void initializeOverlay(int stage);
    /**
     * Writes statistical data and removes node from bootstrap oracle
     */
	virtual void finishOverlay();
	virtual void handleTimerEvent(cMessage* msg);
	virtual void handleUDPMessage(BaseOverlayMessage* msg);
	virtual void joinOverlay();

    /**
     *notify its neighbors that it is going to leave the mesh
     */
	virtual void handleNodeGracefulLeaveNotification();

    /**
     * Search neighbor list with specific TransportAddress to see
     * if they are neighbor or not
     *
     * @param Node the TransportAddress
     * @param vector<TransportAddress> neighbor list
     */
    bool isInVector(TransportAddress& Node, std::vector <TransportAddress> &neighbors);

    /**
     * Delete the node from its neighbors list
     *
     * @param Node the TransportAddress
     * @param vector<TransportAddress> neighbor list
     */
    void deleteVector(TransportAddress Node,std::vector <TransportAddress> &neighbors);
};

#endif /* SIMPLEMESH_H_ */

