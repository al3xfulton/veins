//
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

#ifndef Waymart_H
#define Waymart_H

#include "veins/modules/application/ieee80211p/BaseWaveApplLayer.h"
#include <string.h>

/**
 * @brief
 * A tutorial demo for TraCI. When the car is stopped for longer than 10 seconds
 * it will send a message out to other cars containing the blocked road id.
 * Receiving cars will then trigger a reroute via TraCI.
 * When channel switching between SCH and CCH is enabled on the MAC, the message is
 * instead send out on a service channel following a WAVE Service Advertisement
 * on the CCH.
 *
 * @author Christoph Sommer : initial DemoApp
 * @author David Eckhoff : rewriting, moving functionality to BaseWaveApplLayer, adding WSA
 *
 */

struct Trust {
	float dataPlausibility;
	float dataBelief;
	float numMessages;
	float numTrue;
	float numFalse;
};

struct OutsideOpinion {
    float outBelief;
    float outPlaus;
    int contributors;
};

struct Backlog {
    int subjectId;
    float foreignBelief;
    float foreignPlaus;
};

struct OperatingVals {
    float opBelief;
    float opPlaus;
};

class Waymart : public BaseWaveApplLayer {
	public:
		virtual void initialize(int stage);
	protected:
		simtime_t lastDroveAt;
		bool sentMessage;
		bool sentFakeMessage;
		int currentSubscribedServiceId;

		std::string infoWeather;
		std::string infoTrust;
		std::string alertAccident;
		std::string dataField1;
		std::string dataField2;
		std::string dataField3;
		std::string delimiter1;
		std::string delimiter2;

		// For internal opinions about each vehicle (and external?)
		std::map<int, Trust> trustMap;
		std::map<int, Trust>::iterator trustIter;

        // For external opinions about each vehicle
        std::map<int, OutsideOpinion> outOpinionMap;
        std::map<int, OutsideOpinion>::iterator outsideIter;

        // For storing outside opinions until they are processed
        // Queue is used so that input can be processed in the order in which it was received
        int updateTime;
        int timeFromUpdate;
        std::queue<Backlog> toProcess;

        // For reflecting the operational Belief and Plausibility values for each vehicle
        // Don't need this when calculating belief/plausibility at time of decision
        //std::map<int, OperatingVals> operationalResults;

		// For tracking Accident messages (used for Simple Verification)
		int timeFromMessage;
		std::map<std::string, std::pair<std::string, std::string>> reports;
		std::map<std::string, std::pair<std::string, std::string>>::iterator iter;

	protected:
        virtual void onWSM(WaveShortMessage* wsm);
        virtual void onWSA(WaveServiceAdvertisment* wsa);

        virtual void handleSelfMsg(cMessage* msg);
		virtual void handlePositionUpdate(cObject* obj);

		virtual void updateMatrix(int nodeId, bool checkable, bool verified);
		virtual void addEntry(int nodeId, bool checkable, bool verified);
		virtual void modifyEntry(int nodeId, bool checkable, bool verified);
		virtual double getSample(float mean, float sigma);

		virtual void parseTrust(std::string data);
		virtual std::string createTrustString();

};

#endif
