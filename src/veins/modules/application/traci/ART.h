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

#ifndef ART_H
#define ART_H

#include "veins/modules/application/ieee80211p/BaseWaveApplLayer.h"
#include <string.h>
#include <stdlib.h>
#include <random>

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
 * @author Rachel Eaton, Alex Fulton, Jamie Thorpe : adding functionality for implementing ART,
 *             sending more data in WSM's, sending basic info messages
 *
 */

// Internal opinion about a particular vehicle (ART)
struct Trust {
	float dataPlausibility;
	float dataBelief;
	float numMessages;
	float numTrue;
	float numFalse;
};

// External opinion about a particular vehicle (ART)
struct OutsideOpinion {
    float outBelief;
    float outPlaus;
    int contributors;
};

// Entries of external opinion that need to be processed (ART)
struct Backlog {
    int subjectId;
    float foreignBelief;
    float foreignPlaus;
};

class ART : public BaseWaveApplLayer {
	public:
		virtual void initialize(int stage);

	protected:
		simtime_t lastDroveAt;
		bool sentMessage;
		bool sentFakeMessage;
		int currentSubscribedServiceId;

		// Track the number of outgoing messages
		int messageCount;

		// Formatting all incoming message types
		// Includes basic info, accident alerts, and opinion stuctures for ART
		std::string infoWeather;
		std::string infoTrust;
		std::string alertAccident;
		std::string dataField1;
		std::string dataField2;
		std::string dataField3;
		std::string dataField4;
		std::string delimiter1;
		std::string delimiter2;

		//Track whether accident messages have been received yet
		int accidentMessageCount;
		std::map<int, int> recievedMap;
		std::map<int, int>::iterator recievedIter;

		// Selection of trustworthiness from normal distribution
		std::default_random_engine generator;


		// For internal opinions about each vehicle
		std::map<int, Trust> trustMap;
		std::map<int, Trust>::iterator trustIter;
		int trustUpdateTime;
		int timeFromTrustUpdate;

        // For external opinions about each vehicle
        std::map<int, OutsideOpinion> outOpinionMap;
        std::map<int, OutsideOpinion>::iterator outsideIter;

        // For storing outside opinions until they are processed
        // Queue is used so that input can be processed in the order in which it was received
        int updateTime;
        int timeFromUpdate;
        std::queue<Backlog> toProcess;

		// For tracking what has been echoed already, so echo doesn't continue indefinitely
		int timeFromMessage;
		std::map<std::string, std::pair<std::string, std::string>> reports;
		std::map<std::string, std::pair<std::string, std::string>>::iterator iter;

        // So attacker knows an attack has started
		bool attackStarted;
		std::string attackPosition;

	protected:
        virtual void onWSM(WaveShortMessage* wsm);
        virtual void onWSA(WaveServiceAdvertisment* wsa);

        virtual void handleSelfMsg(cMessage* msg);
		virtual void handlePositionUpdate(cObject* obj);

		// ART-related functions below
		virtual void updateMatrix(int nodeId, bool checkable, bool verified);
		virtual void addEntry(int nodeId, bool checkable, bool verified);
		virtual void modifyEntry(int nodeId, bool checkable, bool verified);
		virtual double getSample(float mean, float sigma);

		virtual void parseTrust(std::string data);
		virtual std::string createTrustString();

};

#endif
