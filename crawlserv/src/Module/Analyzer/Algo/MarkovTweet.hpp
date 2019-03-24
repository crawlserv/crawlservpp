/*
 * MarkovTweet.hpp
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

#ifndef MODULE_ANALYZER_ALGO_MARKOVTWEET_HPP_
#define MODULE_ANALYZER_ALGO_MARKOVTWEET_HPP_

#include "../Thread.hpp"

#include "../../../Main/Data.hpp"
#include "../../../Struct/CorpusProperties.hpp"
#include "../../../Timer/Simple.hpp"

#include "../../../_extern/rawr/rawr.h"

#include <chrono>
#include <functional>
#include <string>
#include <thread>
#include <vector>


namespace crawlservpp::Module::Analyzer::Algo {

	class MarkovTweet: public Module::Analyzer::Thread {
		// for convenience
		typedef Main::Data::Type DataType;
		typedef Main::Data::Value DataValue;
		typedef Struct::CorpusProperties CorpusProperties;
		typedef Struct::ThreadOptions ThreadOptions;

	public:
		MarkovTweet(
				Main::Database& dbBase,
				unsigned long analyzerId,
				const std::string& analyzerStatus,
				bool analyzerPaused,
				const ThreadOptions& threadOptions,
				unsigned long analyzerLast
		);
		MarkovTweet(
				Main::Database& dbBase,
				const ThreadOptions& threadOptions
		);
		virtual ~MarkovTweet();

		// implemented algorithm functions
		void onAlgoInit() override;
		void onAlgoTick() override;
		void onAlgoPause() override;
		void onAlgoUnpause() override;
		void onAlgoClear() override;

		// overwritten configuration functions
		void parseOption() override;
		void checkOptions() override;

		// external access to thread functionality for rawr
		bool _isRunning();
		void _setStatus(const std::string& status);
		void _setProgress(float progress);
		void _log(const std::string& entry);

	private:
		rawr generator;
		unsigned long sources;

		// algorithm options
		unsigned char markovTweetDimension;
		std::string markovTweetLanguage;
		unsigned long markovTweetLength;
		unsigned long markovTweetMax;
		std::string markovTweetResultField;
		unsigned long markovTweetSleep;
		std::string markovTweetSourcesField;
		bool markovTweetSpellcheck;
		bool markovTweetTiming;
	};

}  /* crawlservpp::Module::Analyzer::Algo */

#endif /* MODULE_ANALYZER_ALGO_MARKOVTWEET_HPP_ */
