/*
 * Thread.hpp
 *
 * Implementation of the Thread interface for extractor threads.
 *
 *  Created on: May 9, 2019
 *      Author: ans
 */

#ifndef MODULE_EXTRACTOR_THREAD_HPP_
#define MODULE_EXTRACTOR_THREAD_HPP_

#include "Config.hpp"
#include "Database.hpp"

#include "../Thread.hpp"

#include "../../Helper/DateTime.hpp"
#include "../../Helper/Json.hpp"
#include "../../Helper/Strings.hpp"
#include "../../Main/Exception.hpp"
#include "../../Parsing/XML.hpp"
#include "../../Query/Container.hpp"
#include "../../Query/JsonPath.hpp"
#include "../../Query/JsonPointer.hpp"
#include "../../Query/RegEx.hpp"
#include "../../Query/XPath.hpp"
#include "../../Struct/DataEntry.hpp"
#include "../../Struct/QueryProperties.hpp"
#include "../../Struct/ThreadOptions.hpp"
#include "../../Struct/ThreadStatus.hpp"
#include "../../Timer/Simple.hpp"

#include "../../_extern/jsoncons/include/jsoncons/json.hpp"
#include "../../_extern/jsoncons/include/jsoncons_ext/jsonpath/json_query.hpp"
#include "../../_extern/rapidjson/include/rapidjson/document.h"

#include <algorithm>	// std::find
#include <chrono>		// std::chrono
#include <functional>	// std::bind
#include <iomanip>		// std::setprecision
#include <ios>			// std::fixed
#include <locale>		// std::locale
#include <queue>		// std::queue
#include <sstream>		// std::ostringstream
#include <stdexcept>	// std::logic_error, std::runtime_error
#include <string>		// std::string
#include <thread>		// std::this_thread
#include <utility>		// std::pair
#include <vector>		// std::vector

namespace crawlservpp::Module::Extractor {

	class Thread: public Module::Thread, public Query::Container, public Config {
		// for convenience
		typedef Helper::Json::Exception JsonException;
		typedef Parsing::XML::Exception XMLException;
		typedef Struct::DataEntry DataEntry;
		typedef Struct::QueryProperties QueryProperties;
		typedef Struct::ThreadOptions ThreadOptions;
		typedef Struct::ThreadStatus ThreadStatus;
		typedef Query::JsonPath::Exception JsonPathException;
		typedef Query::JsonPointer::Exception JsonPointerException;
		typedef Query::RegEx::Exception RegExException;
		typedef Query::XPath::Exception XPathException;
		typedef Wrapper::DatabaseLock<Database> DatabaseLock;

		typedef std::pair<unsigned long, std::string> IdString;
		typedef std::pair<std::string, std::string> StringString;

	public:
		// constructors
		Thread(
				Main::Database& database,
				const ThreadOptions& threadOptions,
				const ThreadStatus& threadStatus
		);

		Thread(Main::Database& database, const ThreadOptions& threadOptions);

		// destructor
		virtual ~Thread();

		// sub-class for Extractor exceptions
		class Exception : public Main::Exception {
		public:
			Exception(const std::string& description) : Main::Exception(description) {}
			virtual ~Exception() {}
		};

	protected:
		// database for the thread
		Database database;

		// table names for locking
		std::string extractingTable;
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
		std::vector<QueryStruct> queriesId;
		std::vector<QueryStruct> queriesDateTime;
		std::vector<QueryStruct> queriesFields;
		std::vector<QueryStruct> queriesVariables;
		std::vector<QueryStruct> queriesTokens;
		QueryStruct queryPagingIsNextFrom;
		QueryStruct queryPagingNextFrom;
		QueryStruct queryPagingNumberFrom;
		QueryStruct queryExpected;

		unsigned long lastUrl;

		// timing
		unsigned long long tickCounter;
		std::chrono::steady_clock::time_point startTime;
		std::chrono::steady_clock::time_point pauseTime;
		std::chrono::steady_clock::time_point idleTime;

		// parsing data and state
		bool idle;								// waiting for new URLs to be crawled
		std::string lockTime;					// last locking time for currently extracted URL
		Parsing::XML parsedXML;					// parsed XML/HTML
		rapidjson::Document parsedJsonRapid;	// parsed JSON (using RapidJSON)
		jsoncons::json parsedJsonCons;			// parsed JSON (using jsoncons)
		bool xmlParsed;							// XML/HTML has been parsed
		bool jsonParsedRapid;					// JSON has been parsed using RapidJSON
		bool jsonParsedCons;					// JSON has been parsed using jsoncons
		std::string xmlParsingError;			// error while parsing XML/HTML
		std::string jsonParsingError;			// error while parsing JSON

		// properties used for progress calculation
		unsigned long idFirst;		// ID of the first URL fetched
		unsigned long idDist;		// distance between the IDs of first and last URL fetched
		float posFirstF;			// position of the first URL fetched as float
		unsigned long posDist;		// distance between the positions of first and last URL fetched
		unsigned long total;		// number of total URLs in URL list

		// initializing function
		void initTargetTable();
		void initQueries() override;

		// extracting functions
		void extractingUrlSelection();
		void extractingFetchUrls();
		void extractingCheckUrls();
		unsigned long extractingNext();
		unsigned long extractingParse(unsigned long contentId, const std::string& content);
		void extractingUrlFinished();
		void extractingSaveResults(bool warped);

		// private helper functions
		bool parseXml(const std::string& content);
		bool parseJsonRapid(const std::string& content);
		bool parseJsonCons(const std::string& content);
		void resetParsingState();
		void logParsingErrors(unsigned long contentId);
	};

} /* crawlservpp::Module::Extractor */

#endif /* MODULE_EXTRACTOR_THREAD_HPP_ */
