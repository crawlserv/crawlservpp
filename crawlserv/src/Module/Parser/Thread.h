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
#include "../../Struct/TableLockProperties.h"
#include "../../Struct/ThreadOptions.h"
#include "../../Struct/ParsingEntry.h"
#include "../../Struct/QueryProperties.h"
#include "../../Struct/UrlProperties.h"
#include "../../Timer/Simple.h"
#include "../../Wrapper/TableLock.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <exception>
#include <locale>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace crawlservpp::Module::Parser {
	class Thread: public Module::Thread, public Query::Container {
		// for convenience
		typedef Parsing::XML::Exception XMLException;
		typedef Struct::ParsingEntry ParsingEntry;
		typedef Struct::QueryProperties QueryProperties;
		typedef Struct::TableLockProperties TableLockProperties;
		typedef Struct::ThreadOptions ThreadOptions;
		typedef Struct::UrlProperties UrlProperties;
		typedef Query::RegEx::Exception RegExException;
		typedef Wrapper::TableLock<Wrapper::Database> TableLock;
		typedef std::pair<unsigned long, std::string> IdString;

	public:
		// constructors
		Thread(Main::Database& database, unsigned long crawlerId, const std::string& crawlerStatus, bool crawlerPaused,
				const ThreadOptions& threadOptions, unsigned long crawlerLast);
		Thread(Main::Database& database, const ThreadOptions& threadOptions);

		// destructor
		virtual ~Thread();

	protected:
		// constant string for table aliases
		const std::string targetTableAlias;

		// database for the thread
		Database database;

		// table name for locking
		std::string parsingTable;
		std::string targetTable;

		// cache
		std::queue<UrlProperties> urls;
		std::queue<ParsingEntry> results;
		std::queue<IdString> finished;

		// implemented thread functions
		void onInit(bool resumed) override;
		void onTick() override;
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
		std::vector<QueryStruct> queriesSkip;
		std::vector<QueryStruct> queriesId;
		std::vector<QueryStruct> queriesDateTime;
		std::vector<QueryStruct> queriesFields;

		// timing
		unsigned long long tickCounter;
		std::chrono::steady_clock::time_point startTime;
		std::chrono::steady_clock::time_point pauseTime;
		std::chrono::steady_clock::time_point idleTime;

		// parsing state
		bool idle;					// waiting for new URLs to be crawled
		std::string lockTime;		// last locking time for currently parsed URL

		// properties used for progress calculation
		unsigned long idFirst;		// ID of the first URL fetched
		unsigned long idDist;		// distance between the IDs of first and last URL fetched
		float posFirstF;			// position of the first URL fetched as float
		unsigned long posDist;		// distance between the positions of first and last URL fetched
		unsigned long total;		// number of total URLs in URL list

		// initializing function
		void initTargetTable();
		void initQueries() override;

		// parsing functions
		void parsingUrlSelection();
		void parsingFetchUrls();
		unsigned long parsingNext();
		bool parsingContent(const std::pair<unsigned long, std::string>& content, const std::string& parsedId);
		void parsingUrlFinished();
		void parsingSaveResults();
	};
}

#endif /* MODULE_PARSER_THREAD_H_ */
