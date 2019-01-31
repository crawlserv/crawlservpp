/*
 * Thread.h
 *
 * Implementation of the Thread interface for parser threads.
 *
 *  Created on: Oct 11, 2018
 *      Author: ans
 */

#ifndef MODULE_PARSER_THREAD_H_
#define MODULE_PARSER_THREAD_H_

#include "Config.h"
#include "Database.h"

#include "../Thread.h"

#include "../../Helper/DateTime.h"
#include "../../Helper/Json.h"
#include "../../Helper/Strings.h"
#include "../../Parsing/XML.h"
#include "../../Query/Container.h"
#include "../../Struct/ThreadOptions.h"
#include "../../Timer/StartStop.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <exception>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace crawlservpp::Module::Parser {
	class Thread: public crawlservpp::Module::Thread, public crawlservpp::Query::Container {
	public:
		Thread(crawlservpp::Main::Database& database, unsigned long crawlerId, const std::string& crawlerStatus, bool crawlerPaused,
				const crawlservpp::Struct::ThreadOptions& threadOptions, unsigned long crawlerLast);
		Thread(crawlservpp::Main::Database& database, const crawlservpp::Struct::ThreadOptions& threadOptions);
		virtual ~Thread();

	protected:
		// database for the thread
		Database database;

		// implemented thread functions
		bool onInit(bool resumed) override;
		bool onTick() override;
		void onPause() override;
		void onUnpause() override;
		void onClear(bool interrupted) override;

		// shadow pause function not to be used by thread
		void pause();

	private:
		// hide other functions not to be used by thread
		void start();
		void unpause();
		void stop();
		void interrupt();

		// configuration
		Config config;
		bool idFromUrl;

		// queries
		std::vector<crawlservpp::Query::Container::QueryStruct> queriesSkip;
		std::vector<crawlservpp::Query::Container::QueryStruct> queriesId;
		std::vector<crawlservpp::Query::Container::QueryStruct> queriesDateTime;
		std::vector<crawlservpp::Query::Container::QueryStruct> queriesFields;

		// timing
		unsigned long long tickCounter;
		std::chrono::steady_clock::time_point startTime;
		std::chrono::steady_clock::time_point pauseTime;
		std::chrono::steady_clock::time_point idleTime;

		// parsing state
		std::pair<unsigned long, std::string> currentUrl;	// currently parsed URL
		std::string lockTime;	// last locking time for currently parsed URL

		// initializing function
		void initTargetTable();
		void initQueries() override;

		// parsing functions
		bool parsingUrlSelection();
		unsigned long parsing();
		bool parsingContent(const std::pair<unsigned long, std::string>& content, const std::string& parsedId, bool& skipUrl);
	};
}

#endif /* MODULE_PARSER_THREAD_H_ */
