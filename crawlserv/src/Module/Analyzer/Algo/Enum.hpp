/*
 * Enum.h
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

#ifndef MODULE_ANALYZER_ALGO_LIST_HPP_
#define MODULE_ANALYZER_ALGO_LIST_HPP_

namespace crawlservpp::Module::Analyzer::Algo {

	/*
	 * DECLARATION
	 */

	enum Enum : unsigned char {
		none = 0,
		markovText = 43,
		markovTweet = 44
	};

	bool check(unsigned int value);

	/*
	 * IMPLEMENTATION
	 */

	// checks value for a valid algorithm id
	inline bool check(unsigned int value) {
		switch(value) {
		case Enum::markovText:
			return true;

		case Enum::markovTweet:
			return true;

		}
		return false;
	}

} /* crawlservpp::Module::Analyzer */

#endif /* MODULE_ANALYZER_ALGO_LIST_HPP_ */
