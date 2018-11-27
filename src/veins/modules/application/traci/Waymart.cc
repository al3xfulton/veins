
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
#include <stdlib.h>
#include <random>

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
        infoTrust = "Info::Trust";
        dataField1 = "origSender::";
        dataField2 = "**mData::";
        dataField3 = "**mData2::";
        delimiter1 = "**";
        delimiter2 = "::";
        timeFromMessage = 0;

        updateTime = round(59 * (rand() / RAND_MAX));
        timeFromUpdate = 0;
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

        // Check if you can verify the new message; I'm assuming you can't ever verify an accident message
        // If you can, verify whether it is true
        updateMatrix(std::stoi(sender_id), false, false); // Assumes all incoming messages are True
        //printf("%s reports accident on %s at %s \n", sender_id.c_str(), road_id.c_str(), time_sent.c_str());

        if (mobility->getRoadId()[0] != ':'){
            float dist_mean;
            float sigma;
            double sample;
            double rerouteThreshold = 0.5;

            // we just added it so don't need to check if it's present
            Trust currentTrust = trustMap[std::stoi(sender_id)];

            outsideIter = outOpinionMap.find(std::stoi(sender_id));
            // no outside opinion!
            if(outsideIter == outOpinionMap.end()){

                 dist_mean = (currentTrust.dataBelief+currentTrust.dataPlausibility)/2;
                 sigma = (currentTrust.dataPlausibility-currentTrust.dataBelief)/6; // this is somewhat arbitrary
                 sample = getSample(dist_mean, sigma);
            // we do have a number of outside opinions
            } else {
                OutsideOpinion outsideOpinion = outOpinionMap[std::stoi(sender_id)];
                float total_belief = (currentTrust.dataBelief+outsideOpinion.outBelief*outsideOpinion.contributors)/(outsideOpinion.contributors+1);
                float total_plaus = (currentTrust.dataPlausibility + outsideOpinion.outPlaus*outsideOpinion.contributors)/(outsideOpinion.contributors+1);
                dist_mean = (total_belief + total_plaus)/2;
                sigma = (total_plaus-total_belief)/6;
                sample = getSample(dist_mean, sigma);
            }

            if (sample > rerouteThreshold){
                traciVehicle->changeRoute(road_id, 9999);
                // Send echo
                //repeat the received traffic update once in 2 seconds plus some random delay
                wsm->setSenderAddress(myId);
                wsm->setSerial(3);
                scheduleAt(simTime() + 2 + uniform(0.01,0.2), wsm->dup());
            }

            //printf("Generating normal distribution between %f and %f based on %f messages\n", currentTrust.dataBelief, currentTrust.dataPlausibility, currentTrust.numMessages);
            printf("Generated sample %f with %f mean and %f std. dev\n", sample, dist_mean, sigma);


            //iter = reports.find(road_id);

            /* Rachel: I think all of this is for opportunistic "polling" so I'm commenting it out
             if (iter != reports.end()){ // Road ID already in map
                //printf("Vehicle %d receives report of %s Accident on: %s\n", myId, sender_id.c_str(), road_id.c_str());

                // Received old message; don't reroute
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

                // Received verification message
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
                */
            }
            else { // put new thing in map
                reports[road_id] = std::make_pair(sender_id, time_sent);
                // Send echo
                //repeat the received traffic update once in 2 seconds plus some random delay
                wsm->setSenderAddress(myId);
                wsm->setSerial(3);
                scheduleAt(simTime() + 2 + uniform(0.01,0.2), wsm->dup());
            }

        //}

    }
    else if (psc_cat == "Info") {
        if (psc_type == "Weather") { // can check here for benign Info updates

            //printf("%s %s\n", psc_cat.c_str(), psc_type.c_str());

            std::string thisData = wsm->getWsmData();
            std::string data_sender = thisData.substr(0, thisData.find(delimiter1));
            std::string temp = thisData.substr(thisData.find(delimiter1) + 2, thisData.length() - data_sender.length() - 2);

            std::string data_road = temp.substr(0, temp.find(delimiter1));
            std::string data_state = temp.substr(temp.find(delimiter1) + 2, temp.length() - data_road.length() - 2);

            std::string sender_id = data_sender.substr(data_sender.find(delimiter2) + 2, data_sender.length()-dataField1.length());
            std::string road_id = data_road.substr(data_road.find(delimiter2) + 2, data_road.length()-(dataField2.length()-2));
            std::string state_weather = data_state.substr(data_state.find(delimiter2) + 2, data_state.length()-(dataField3.length()-2));

            // Check if you can verify the new message
            // If you can, verify whether it is true
            updateMatrix(std::stoi(sender_id), true, true); // Assumes all incoming messages are True
            //printf("%s says %s at %s \n", sender_id.c_str(), state_weather.c_str(), road_id.c_str());
        }

        else if (psc_type == "Trust") {
            printf("This is a Trust Matrix message");
            std::map<int, OutsideOpinion> recievedMap;
            //Pull out data string and pass to function to parse
            std::string thisData = wsm->getWsmData();
            recievedMap = parseTrust(thisData);

        }
    }
    else {
        //printf("Unrecognized message: %s %s \n", psc_cat.c_str(), psc_type.c_str());
    }
}

double Waymart::getSample(float mean, float sigma){
    double sample;
    std::default_random_engine generator;

    std::normal_distribution<double> distribution(mean, sigma);
    sample = distribution(generator);
    return sample;
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

    if (timeFromUpdate % 60 == updateTime) {

        // Perform Processing
        // Don't process entries that come in while we're processing, or we may be stuck forever
        int left = toProcess.size();

        //Just to make sure that data is being pulled correctly
        std::string data;
        data = createTrustString();
        printf("Node: %d Data: %s\n", myId, data);


        while (left > 0 ) {
            Backlog current = toProcess.front();
            outsideIter = outOpinionMap.find(current.subjectId);

            if (outsideIter == outOpinionMap.end()) {
                // new entry
                OutsideOpinion newOp;
                newOp.outBelief = current.foreignBelief;
                newOp.outPlaus = current.foreignPlaus;
                newOp.contributors = 1;
                outOpinionMap[current.subjectId] = newOp;
            }
            else {
                // update existing entry
                OutsideOpinion oldOp = outsideIter->second;
                OutsideOpinion newOp;
                newOp.outBelief = ((oldOp.outBelief * oldOp.contributors) + current.foreignBelief) / (oldOp.contributors + 1);
                newOp.outPlaus = ((oldOp.outPlaus * oldOp.contributors) + current.foreignPlaus) / (oldOp.contributors + 1);
                newOp.contributors = oldOp.contributors + 1;
                outsideIter->second = newOp;
            }

            // pop it
            toProcess.pop();

            // decrease left
            left --;
        }

    }
    else {
        timeFromUpdate ++;
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

// Checkable denotes whether we personally could see if the data was True or False
// If the data was checkable, Verified denotes whether it was successfully verified as True
void Waymart::updateMatrix(int nodeId, bool checkable, bool verified){
    trustIter = trustMap.find(nodeId);
    if(trustIter == trustMap.end()){
        addEntry(nodeId, checkable, verified);
    }
    else {
        modifyEntry(nodeId, checkable, verified);
    }

    //for (auto it = trustMap.cbegin(); it != trustMap.cend(); it++) {
        //std::cout << "Vehicle " << myId << ": Key: " << (it->first) << "; Belief: " << (it->second.dataBelief) << "; Plausibility: " << (it->second.dataPlausibility) << "\n";
    //}
}

void Waymart::addEntry(int nodeId, bool checkable, bool verified){

    Trust temp;
    //temp.dataTrust = (float)((rand()%20)+80)/100;

    temp.numMessages = 5;
    temp.numTrue = 1;
    temp.numFalse = 2;

    // To be used if we implement a different mode of "hand waving" initialization
    /*
    temp.numMessages = 1;

    if (checkable) {
        if (verified) {
            temp.numTrue = 1;
            temp.numFalse = 0;
        }
        else {
            temp.numTrue = 0;
            temp.numFalse = 1;
        }
    }
    else {
        temp.numTrue = 0;
        temp.numFalse = 0;
    }
    */

    temp.dataBelief = temp.numTrue/temp.numMessages;
    temp.dataPlausibility = 1 - temp.numFalse/temp.numMessages;
    trustMap[nodeId] = temp;
}

void Waymart::modifyEntry(int nodeId, bool checkable, bool verified){
    Trust myStruct = trustMap[nodeId];
    float count = ++ myStruct.numMessages;
    float numTrue = myStruct.numTrue;
    float numFalse = myStruct.numFalse;

    if (checkable) {
        if (verified) {
            float numTrue = ++ myStruct.numTrue;
        }
        else {
            float numFalse = ++ myStruct.numFalse;
        }
    }

    float bel = numTrue/count;
    float pls = 1 - numFalse/count;

    myStruct.dataPlausibility = pls;
    myStruct.dataBelief = bel;
    //printf("New values: %f messages sent; %f true; %f false; %f belief; %f plaus\n",
    //        myStruct.numMessages, myStruct.numTrue, myStruct.numFalse, myStruct.dataBelief, myStruct.dataPlausibility);
    trustMap[nodeId] = myStruct;
}

std::map<int, OutsideOpinion> Waymart::parseTrust(std::string data){
    //Put the trust details into the map
    
    std::map<int, OutsideOpinion> recievedMap;


    std::string nodeId;
    std::string dataPls;
    std::string dataBel;
    // = data.substr(0, thisPSC.find(delimiter1));
    //= data.substr(thisPSC.find(delimiter1) + 2, thisPSC.length() - psc_cat.length() - 2);
    size_t data_pos = 0;
    size_t entry_pos = 0;
    std::string entry;

    while ((data_pos = data.find(delimiter1)) != std::string::npos) {
        entry = data.substr(0, data_pos);
        
        //parse each of the entries here
        entry_pos = entry.find(delimiter2);
        nodeId = entry.substr(0, entry_pos);
        entry.erase(0, entry_pos + delimiter2.length());
        entry_pos = entry.find(delimiter2);
        dataPls = entry.substr(0, entry_pos);
        entry.erase(0, entry_pos + delimiter2.length());
        dataBel = entry.substr(0, entry_pos);

        data.erase(0, data_pos + delimiter1.length());

        //Enter data into the map
        OutsideOpinion newEntry;
        newEntry.outBelief = stof(dataBel);
        newEntry.outPlaus = stof(dataPls);
        newEntry.contributors = 1;
        recievedMap[nodeId] = newEntry;
    }
    
    return recievedMap;
}

std::string Waymart::createTrustString(){
    //Iterate through the map to create the string to pass back

    std::string output = "";

    for (auto it = trustMap.cbegin(); it != trustMap.cend(); it++) {
        output = output + it->first + delimiter2 + it->second.dataPlausibility + delimiter2 + it->second.dataBelief + delimiter1;
    }
    return output;
}
