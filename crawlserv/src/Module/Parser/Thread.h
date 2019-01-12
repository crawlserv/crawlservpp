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
#include "../../Struct/IdString.h"
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
#include <vector>

namespace crawlservpp::Module::Parser {
	class Thread: public crawlservpp::Module::Thread, public crawlservpp::Query::Container {
	public:
		Thread(crawlservpp::Global::Database& database, unsigned long crawlerId, const std::string& crawlerStatus, bool crawlerPaused,
				const crawlservpp::Struct::ThreadOptions& threadOptions, unsigned long crawlerLast);
		Thread(crawlservpp::Global::Database& database, const crawlservpp::Struct::ThreadOptions& threadOptions);
		virtual ~Thread();

	protected:
		Database database;

		bool onInit(bool resumed) override;
		bool onTick() override;
		void onPause() override;
		void onUnpause() override;
		void onClear(bool interrupted) override;

	private:
		// hide functions not to be used by thread
		void start();
		void pause();
		void unpause();
		void stop();
		void interrupt();

		// configuration
		Config config;
		bool idFromUrl;

		std::vector<crawlservpp::Query::Container::QueryStruct> queriesId;
		std::vector<crawlservpp::Query::Container::QueryStruct> queriesDateTime;
		std::vector<crawlservpp::Query::Container::QueryStruct> queriesFields;

		// timing
		unsigned long long tickCounter;
		std::chrono::steady_clock::time_point startTime;
		std::chrono::steady_clock::time_point pauseTime;
		std::chrono::steady_clock::time_point idleTime;

		// parsing state
		crawlservpp::Struct::IdString currentUrl;	// currently parsed URL
		std::string lockTime;	// last locking time for currently parsed URL

		// initializing function
		void initTargetTable();
		void initQueries() override;

		// parsing functions
		bool parsingUrlSelection();
		unsigned long parsing();
		bool parsingContent(const crawlservpp::Struct::IdString& content, const std::string& parsedId);
	};
}

#endif /* MODULE_PARSER_THREAD_H_ */
