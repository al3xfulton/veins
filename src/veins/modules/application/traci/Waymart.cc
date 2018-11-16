
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

#include "veins/modules/application/traci/Waymart.h"

Define_Module(Waymart);

void Waymart::initialize(int stage) {
    BaseWaveApplLayer::initialize(stage);
    if (stage == 0) {
        sentMessage = false;
        sentFakeMessage = false;
        lastDroveAt = simTime();
        currentSubscribedServiceId = -1;
        alertAccident = "Alert::Accident";
        infoWeather = "Info::Weather";
        dataField1 = "origSender::";
        dataField2 = "**mData::";
        dataField3 = "**mData2::";
        delimiter1 = "**";
        delimiter2 = "::";
        timeFromMessage = 0;
    }
}

void Waymart::onWSA(WaveServiceAdvertisment* wsa) {
    if (currentSubscribedServiceId == -1) {
        mac->changeServiceChannel(wsa->getTargetChannel());
        currentSubscribedServiceId = wsa->getPsid();
        if  (currentOfferedServiceId != wsa->getPsid()) {
            stopService();
            startService((Channels::ChannelNumber) wsa->getTargetChannel(), wsa->getPsid(), "Mirrored Traffic Service");
        }
    }
}

void Waymart::onWSM(WaveShortMessage* wsm) {
    findHost()->getDisplayString().updateWith("r=16,green");

    std::string thisPSC = wsm->getPsc();
    std::string psc_cat = thisPSC.substr(0, thisPSC.find(delimiter2));
    std::string psc_type = thisPSC.substr(thisPSC.find(delimiter2) + 2, thisPSC.length() - psc_cat.length() - 2);

    if(psc_cat == "Alert" && psc_type == "Accident") {
        std::string thisData = wsm->getWsmData();
        std::string data_sender = thisData.substr(0, thisData.find(delimiter1));
        std::string temp = thisData.substr(thisData.find(delimiter1) + 2, thisData.length() - data_sender.length() - 2);

        std::string data_road = temp.substr(0, temp.find(delimiter1));
        std::string data_time = temp.substr(temp.find(delimiter1) + 2, temp.length() - data_road.length() - 2);

        std::string sender_id = data_sender.substr(data_sender.find(delimiter2) + 2, data_sender.length()-dataField1.length());
        std::string road_id = data_road.substr(data_road.find(delimiter2) + 2, data_road.length()-(dataField2.length()-2));
        std::string time_sent = data_time.substr(data_time.find(delimiter2) + 2, data_time.length()-(dataField3.length()-2));

        //printf("%s reports accident on %s at %s \n", sender_id.c_str(), road_id.c_str(), time_sent.c_str());

        if (mobility->getRoadId()[0] != ':'){
            iter = reports.find(road_id);

            if (iter != reports.end()){ // Road ID already in map
                //printf("Vehicle %d receives report of %s Accident on: %s\n", myId, sender_id.c_str(), road_id.c_str());

                if (std::stoi(time_sent) - std::stoi(iter->second.second) > 300) {
                    //printf("Vehicle %d replaces very old info: accident %s at %s \n", myId, road_id.c_str(), time_sent.c_str());
                    iter->second = std::make_pair(sender_id, time_sent);
                    // Echo
                    //repeat the received traffic update once in 2 seconds plus some random delay
                    wsm->setSenderAddress(myId);
                    wsm->setSerial(3);
                    scheduleAt(simTime() + 2 + uniform(0.01,0.2), wsm->dup());
                    // Don't reroute
                }
                else if (std::stoi(time_sent) >= std::stoi(iter->second.second) && iter->second.first != sender_id) { // we know it's a verification from a different sender
                    //printf("Vehicle %d verified Accident on: %s\n", myId, road_id.c_str());

                    iter->second = std::make_pair(sender_id, time_sent);
                    traciVehicle->changeRoute(road_id, 9999);
                    // Send echo
                    //repeat the received traffic update once in 2 seconds plus some random delay
                    wsm->setSenderAddress(myId);
                    wsm->setSerial(3);
                    scheduleAt(simTime() + 2 + uniform(0.01,0.2), wsm->dup());
                }
                else { // An echo of a recent message or a repeat from the original sender
                    //printf("Vehicle %d received a repeat or old information: accident %s at %s \n", myId, road_id.c_str(), time_sent.c_str());
                    // Don't echo, react, or update map
                }
            }
            else { // put new thing in map
                reports[road_id] = std::make_pair(sender_id, time_sent);
                // Send echo
                //repeat the received traffic update once in 2 seconds plus some random delay
                wsm->setSenderAddress(myId);
                wsm->setSerial(3);
                scheduleAt(simTime() + 2 + uniform(0.01,0.2), wsm->dup());
            }

        }

    }
    else if (psc_cat == "Info" && psc_type == "Weather") { // can check here for benign Info updates
        //printf("%s %s\n", psc_cat.c_str(), psc_type.c_str());

        std::string thisData = wsm->getWsmData();
        std::string data_sender = thisData.substr(0, thisData.find(delimiter1));
        std::string temp = thisData.substr(thisData.find(delimiter1) + 2, thisData.length() - data_sender.length() - 2);

        std::string data_road = temp.substr(0, temp.find(delimiter1));
        std::string data_state = temp.substr(temp.find(delimiter1) + 2, temp.length() - data_road.length() - 2);

        std::string sender_id = data_sender.substr(data_sender.find(delimiter2) + 2, data_sender.length()-dataField1.length());
        std::string road_id = data_road.substr(data_road.find(delimiter2) + 2, data_road.length()-(dataField2.length()-2));
        std::string state_weather = data_state.substr(data_state.find(delimiter2) + 2, data_state.length()-(dataField3.length()-2));

        //printf("%s says %s at %s \n", sender_id.c_str(), state_weather.c_str(), road_id.c_str());
    }
    else {
        //printf("Unrecognized message: %s %s \n", psc_cat.c_str(), psc_type.c_str());
    }
}

void Waymart::handleSelfMsg(cMessage* msg) {
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

void Waymart::handlePositionUpdate(cObject* obj) {
    BaseWaveApplLayer::handlePositionUpdate(obj);

    if (timeFromMessage >= 30) {
        timeFromMessage = 0;

        WaveShortMessage* wsm = new WaveShortMessage();
        populateWSM(wsm);
        wsm->setPsc(infoWeather.c_str());
        std::string filler = dataField1 + std::to_string(myId) + dataField2 + (mobility->getRoadId().c_str()) + dataField3 + ("rain");
        wsm->setWsmData(filler.c_str());

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
    else {
        timeFromMessage ++;
    }

    // stopped for for at least 10s? Indicating a crash
    if (mobility->getSpeed() < 1) {

        if (simTime() - lastDroveAt >= 10) {

            if (sentMessage == false) {
                //printf("%d prepping accident message \n", myId);
                findHost()->getDisplayString().updateWith("r=16,red");
                sentMessage = true;

                WaveShortMessage* wsm = new WaveShortMessage();
                populateWSM(wsm);
                wsm->setPsc(alertAccident.c_str());
                std::string filler = dataField1 + std::to_string(myId) + dataField2 + (mobility->getRoadId().c_str()) + dataField3 + (simTime().str());
                wsm->setWsmData(filler.c_str());

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


    }
    else {
        lastDroveAt = simTime();
        // no crash - check for trigger for fake crash
        //printf("%d about to send accident message \n", myId);
        if (mobility->getFakeState() == 1 && !sentFakeMessage){
            //printf("%d prepping accident message \n", myId);

            findHost()->getDisplayString().updateWith("r=16,blue"); //What is this actually changing?
            sentMessage = true; // JAMIE: should we do this, or set getFakeState to 0?
            sentFakeMessage = true;

            WaveShortMessage* wsm = new WaveShortMessage();
            populateWSM(wsm);
            wsm->setPsc(alertAccident.c_str());
            std::string filler = dataField1 + std::to_string(myId) + dataField2 + (mobility->getSavedRoadId().c_str() + dataField3 + (simTime().str()));
            wsm->setWsmData(filler.c_str());

            // I have no idea what this means
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
