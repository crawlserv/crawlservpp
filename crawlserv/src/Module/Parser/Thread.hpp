/*
 * Thread.hpp
 *
 * Implementation of the Thread interface for parser threads.
 *
 *  Created on: Oct 11, 2018
 *      Author: ans
 */

#ifndef MODULE_PARSER_THREAD_HPP_
#define MODULE_PARSER_THREAD_HPP_

#define RAPIDJSON_HAS_STDSTRING 1

#include "Config.hpp"
#include "Database.hpp"

#include "../Thread.hpp"

#include "../../Helper/DateTime.hpp"
#include "../../Helper/Json.hpp"
#include "../../Helper/Strings.hpp"
#include "../../Main/Exception.hpp"
#include "../../Parsing/XML.hpp"
#include "../../Query/Container.hpp"
#include "../../Query/RegEx.hpp"
#include "../../Query/XPath.hpp"
#include "../../Struct/ThreadOptions.hpp"
#include "../../Struct/ParsingEntry.hpp"
#include "../../Struct/QueryProperties.hpp"
#include "../../Timer/Simple.hpp"

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

	class Thread: public Module::Thread, public Query::Container, public Config {
		// for convenience
		typedef Main::Exception Exception;
		typedef Parsing::XML::Exception XMLException;
		typedef Struct::ParsingEntry ParsingEntry;
		typedef Struct::QueryProperties QueryProperties;
		typedef Struct::ThreadOptions ThreadOptions;
		typedef Query::RegEx::Exception RegExException;
		typedef Query::XPath::Exception XPathException;
		typedef Wrapper::DatabaseLock<Database> DatabaseLock;

		typedef std::pair<unsigned long, std::string> IdString;

	public:
		// constructors
		Thread(
				Main::Database& database,
				unsigned long crawlerId,
				const std::string& crawlerStatus,
				bool crawlerPaused,
				const ThreadOptions& threadOptions,
				unsigned long crawlerLast
		);
		Thread(Main::Database& database, const ThreadOptions& threadOptions);

		// destructor
		virtual ~Thread();

	protected:
		// database for the thread
		Database database;

		// table names for locking
		std::string parsingTable;
		std::string targetTable;

		// cache
		std::queue<IdString> urls;
		std::string cacheLockTime;
		std::queue<ParsingEntry> results;
		std::queue<IdString> finished;

		// implemented thread functions
		void onInit() override;
		void onTick() override;
		void onPause() override;
		void onUnpause() override;
		void onClear() override;

		// shadow pause function not to be used by thread
		void pause();

	private:
		// hide other functions not to be used by thread
		void start();
		void unpause();
		void stop();
		void interrupt();

		// configuration
		bool idFromUrl;

		// queries
		std::vector<QueryStruct> queriesSkip;
		std::vector<QueryStruct> queriesId;
		std::vector<QueryStruct> queriesDateTime;
		std::vector<QueryStruct> queriesFields;
		unsigned long lastUrl;

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
		void parsingCheckUrls();
		unsigned long parsingNext();
		bool parsingContent(const IdString& content, const std::string& parsedId);
		void parsingUrlFinished();
		void parsingSaveResults(bool warped);
	};

} /* crawlservpp::Module::Parser */

#endif /* MODULE_PARSER_THREAD_HPP_ */
