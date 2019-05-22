/*
 *
 * ---
 *
 *  Copyright (C) 2019 Anselm Schmidt (ans[ät]ohai.su)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version in addition to the terms of any
 *  licences already herein identified.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * ---
 *
 * Thread.hpp
 *
 * Implementation of the Thread interface for parser threads.
 *
 *  Created on: Oct 11, 2018
 *      Author: ans
 */

#ifndef MODULE_PARSER_THREAD_HPP_
#define MODULE_PARSER_THREAD_HPP_

#include "Config.hpp"
#include "Database.hpp"

#include "../Thread.hpp"

#include "../../Helper/DateTime.hpp"
#include "../../Helper/Json.hpp"
#include "../../Helper/Strings.hpp"
#include "../../Main/Exception.hpp"
#include "../../Query/Container.hpp"
#include "../../Struct/DataEntry.hpp"
#include "../../Struct/QueryProperties.hpp"
#include "../../Struct/QueryStruct.hpp"
#include "../../Struct/ThreadOptions.hpp"
#include "../../Struct/ThreadStatus.hpp"
#include "../../Timer/Simple.hpp"

#include <algorithm>	// std::find, std::find_if
#include <chrono>		// std::chrono
#include <functional>	// std::bind
#include <iomanip>		// std::setprecision
#include <ios>			// std::fixed
#include <locale>		// std::locale
#include <queue>		// std::queue
#include <sstream>		// std::ostringstream
#include <stdexcept>	// std::logic_error, std::runtime_error
#include <string>		// std::string, std::to_string
#include <thread>		// std::this_thread
#include <utility>		// std::pair
#include <vector>		// std::vector

namespace crawlservpp::Module::Parser {

	class Thread: public Module::Thread, private Query::Container, private Config {
		// for convenience
		typedef Helper::DateTime::Exception DateTimeException;
		typedef Helper::DateTime::LocaleException LocaleException;
		typedef Helper::Json::Exception JsonException;
		typedef Struct::DataEntry DataEntry;
		typedef Struct::QueryProperties QueryProperties;
		typedef Struct::QueryStruct QueryStruct;
		typedef Struct::ThreadOptions ThreadOptions;
		typedef Struct::ThreadStatus ThreadStatus;
		typedef Query::Container::Exception QueryException;
		typedef Wrapper::DatabaseLock<Database> DatabaseLock;

		typedef std::pair<unsigned long, std::string> IdString;

	public:
		// constructors
		Thread(
				Main::Database& dbBase,
				const ThreadOptions& threadOptions,
				const ThreadStatus& threadStatus
		);

		Thread(Main::Database& dbBase, const ThreadOptions& threadOptions);

		// destructor
		virtual ~Thread();

		// sub-class for Parser exceptions
		MAIN_EXCEPTION_CLASS();

	protected:
		// database for the thread
		Database database;

		// table names for locking
		std::string parsingTable;
		std::string targetTable;

		// cache
		std::queue<IdString> urls;
		std::string cacheLockTime;
		std::queue<DataEntry> results;
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
		bool idFromUrlOnly;			// ID is exclusively parsed from URL
		unsigned long lastUrl;		// last URL
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

		// private helper functions
		bool parseXml(const std::string& content);
		bool parseJsonRapid(const std::string& content);
		bool parseJsonCons(const std::string& content);
		void resetParsingState();
		void logWarnings(std::queue<std::string>& warnings);
	};

} /* crawlservpp::Module::Parser */

#endif /* MODULE_PARSER_THREAD_HPP_ */
