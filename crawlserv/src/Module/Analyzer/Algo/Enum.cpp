/*
 * Enum.cpp
 *
 * Enumeration of implemented algorithms.
 *
 * WARNING:	Algorithms need also to be included in 'json/algos.json' in 'crawlserv_frontend' in order to be usable by the frontend.
 *
 * 			Remember to add new algorithms to Module::Analyzer::Algo::check(), Server::Server() and Server::cmdStartAnalyzer()!
 * 			Also do not forget to add the necessary #include to the 'Server.h' header file.
 *
 *  Created on: Jan 13, 2019
 *      Author: ans
 */

#include "Enum.h"

namespace crawlservpp::Module::Analyzer::Algo {

// checks value for a valid algorithm id
bool check(unsigned int value) {
	switch(value) {
	case Enum::markovText:
		return true;
	case Enum::markovTweet:
		return true;
	}
	return false;
}

}
