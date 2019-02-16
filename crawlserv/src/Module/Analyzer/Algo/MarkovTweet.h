/*
 * MarkovTweet.h
 *
 *  Markov Chain Tweet Generator algorithm implemented as child of the abstract Analyzer thread class.
 *
 *   This is a semi-serious proof-of-concept class for the crawlserv++ Analyzer module.
 *
 *   It uses the markov chain algorithm to generate random tweet texts from a large text corpus previously parsed.
 *
 *   The implementation of the algorithm itself is done by the slightly modified rawr class, originally by Kelly Rauchenberger
 *   at https://github.com/hatkirby/rawr-ebooks 👌
 *
 *   WARNING: This algorithm may use A LOT of memory when parsing large corpora, adjust your swap size accordingly to prevent
 *   		 	the server from being killed by the operating system!
 *
 *
 *  Created on: Jan 13, 2019
 *      Author: ans
 */

#ifndef MODULE_ANALYZER_ALGO_MARKOVTWEET_H_
#define MODULE_ANALYZER_ALGO_MARKOVTWEET_H_

#include "../Thread.h"

#include "../../../Main/Data.h"
#include "../../../Struct/CorpusProperties.h"
#include "../../../Timer/Simple.h"

#include "../../../_extern/rawr/rawr.h"

#include <chrono>
#include <functional>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

namespace crawlservpp::Module::Analyzer::Algo {
	typedef crawlservpp::Main::Data::Type DataType;
	typedef crawlservpp::Main::Data::Value DataValue;

	class MarkovTweet: public crawlservpp::Module::Analyzer::Thread {
	public:
		MarkovTweet(crawlservpp::Main::Database& dbBase, unsigned long analyzerId, const std::string& analyzerStatus,
						bool analyzerPaused, const crawlservpp::Struct::ThreadOptions& threadOptions, unsigned long analyzerLast);
		MarkovTweet(crawlservpp::Main::Database& dbBase, const crawlservpp::Struct::ThreadOptions& threadOptions);
		virtual ~MarkovTweet();

		// implemented algorithm functions
		void onAlgoInit(bool resumed);
		void onAlgoTick();
		void onAlgoPause();
		void onAlgoUnpause();
		void onAlgoClear(bool interrupted);

		// external access to thread functionality for rawr
		bool _isRunning();
		void _setStatus(const std::string& status);
		void _setProgress(float progress);
		void _log(const std::string& entry);

	private:
		rawr generator;
		unsigned long sources;
	};
}

#endif /* MODULE_ANALYZER_ALGO_MARKOVTWEET_H_ */
