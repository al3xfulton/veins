// Copyright (C) 2006-2011 Christoph Sommer <christoph.sommer@uibk.ac.at>
//
// Documentation for these modules is at http://veins.car2x.org/
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// Edited by Rachel Eaton, Alex Fulton, Jamie Thorpe
//

#include "veins/modules/application/traci/Baseline.h"

Define_Module(Baseline);

void Baseline::initialize(int stage) {
    BaseWaveApplLayer::initialize(stage);
    if (stage == 0) {
        sentMessage = false;
        sentFakeMessage = false;
        lastDroveAt = simTime();
        currentSubscribedServiceId = -1;
        messageCount = 0;

        attackStarted = false;
        attackPosition = "";
    }
}

void Baseline::onWSA(WaveServiceAdvertisment* wsa) {
    if (currentSubscribedServiceId == -1) {
        mac->changeServiceChannel(wsa->getTargetChannel());
        currentSubscribedServiceId = wsa->getPsid();
        if  (currentOfferedServiceId != wsa->getPsid()) {
            stopService();
            startService((Channels::ChannelNumber) wsa->getTargetChannel(), wsa->getPsid(), "Mirrored Traffic Service");
        }
    }
}

void Baseline::onWSM(WaveShortMessage* wsm) {
    findHost()->getDisplayString().updateWith("r=16,green");

    if (mobility->getRoadId()[0] != ':' && wsm->getSenderAddress() != myId) {

        // Make sure you are not an attacker reacting to your own fake accident
        if (!attackStarted || attackPosition != wsm->getWsmData()){
            traciVehicle->changeRoute(wsm->getWsmData(), 9999);
            printf("ID %d reroute \n", myId);

            if (!sentMessage) {
                messageCount += 1;
                printf("Node Id: %d Count: %d\n", myId, messageCount);
                sentMessage = true;

                //repeat the received traffic update once in 2 seconds plus some random delay
                wsm->setSenderAddress(myId);
                wsm->setSerial(3);
                scheduleAt(simTime() + 2 + uniform(0.01,0.2), wsm->dup());
            }
        }
    }

}

void Baseline::handleSelfMsg(cMessage* msg) {
    if (WaveShortMessage* wsm = dynamic_cast<WaveShortMessage*>(msg)) {
        //send this message on the service channel until the counter is 3 or higher.
        //this code only runs when channel switching is enabled
        sendDown(wsm->dup());
        wsm->setSerial(wsm->getSerial() +1);
        if (wsm->getSerial() >= 3) {
            //stop service advertisements
            stopService();
            delete(wsm);
        }
        else {
            scheduleAt(simTime()+1, wsm);
        }
    }
    else {
        BaseWaveApplLayer::handleSelfMsg(msg);
    }
}

void Baseline::handlePositionUpdate(cObject* obj) {
    BaseWaveApplLayer::handlePositionUpdate(obj);

    // stopped for for at least 10s? Indicating a crash
    if (mobility->getSpeed() < 1) {
        if (simTime() - lastDroveAt >= 10 && sentMessage == false) {
            findHost()->getDisplayString().updateWith("r=16,red");
            sentMessage = true;

            WaveShortMessage* wsm = new WaveShortMessage();
            populateWSM(wsm);
            wsm->setWsmData(mobility->getRoadId().c_str());
            messageCount += 1;
            printf("Node Id: %d Count: %d\n", myId, messageCount);

            //host is standing still due to crash
            if (dataOnSch) {
                startService(Channels::SCH2, 42, "Traffic Information Service");
                //started service and server advertising, schedule message to self to send later
                scheduleAt(computeAsynchronousSendingTime(1,type_SCH),wsm);
            }
            else {
                //send right away on CCH, because channel switching is disabled
                sendDown(wsm);
            }
        }
    }
    else {
        lastDroveAt = simTime();
        // no crash - check for trigger for fake crash
        if (mobility->getFakeState() == 1 && !sentFakeMessage){

            // If attack triggered, generate and send accident message using
            // stored road ID (as opposed to current roud ID)
            findHost()->getDisplayString().updateWith("r=16,blue");
            sentFakeMessage = true;
            attackStarted = true;
            attackPosition = mobility->getSavedRoadId();
            printf("%d generating accident \n", myId);

            WaveShortMessage* wsm = new WaveShortMessage();
            populateWSM(wsm);
            wsm->setWsmData(mobility->getSavedRoadId().c_str());
            messageCount += 1;
            printf("Node Id: %d Count: %d\n", myId, messageCount);

            if (dataOnSch) {
                startService(Channels::SCH2, 42, "Traffic Information Service");
                //started service and server advertising, schedule message to self to send later
                scheduleAt(computeAsynchronousSendingTime(1,type_SCH),wsm);
            }
            else {
                //send right away on CCH, because channel switching is disabled
                sendDown(wsm);
            }
        }
    }
}
