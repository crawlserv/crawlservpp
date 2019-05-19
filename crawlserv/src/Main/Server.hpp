/*
 * Server.h
 *
 * The command-and-control server. Uses the mongoose and RapidJSON libraries to implement a HTTP server for receiving JSON-formatted
 *  commands and sending JSON-formatted replies from/to the crawlserv_frontend.
 *
 *  Also handles all threads for the different modules as well as worker threads for query testing.
 *
 *  Created on: Oct 7, 2018
 *      Author: ans
 */

#ifndef MAIN_SERVER_HPP_
#define MAIN_SERVER_HPP_

// hard-coded constants
#define MAIN_SERVER_DIR_CACHE "cache" // directory for file cache
#define MAIN_SERVER_DIR_COOKIES "cookies" // directory for cookies
#define MAIN_SERVER_SLEEP_ON_SQL_ERROR_SEC 5 // server-side sleep on mySQL error

// macros for exception handling of worker threads
#define MAIN_SERVER_WORKER_BEGIN	try {
#define MAIN_SERVER_WORKER_END(X)	} \
									catch(const std::exception& e) { \
										(X) = ServerCommandResponse::failed( \
												e.what() \
										); \
									}

#include "Database.hpp"
#include "Exception.hpp"
#include "WebServer.hpp"

#include "../Data/File.hpp"
#include "../Data/Compression/Gzip.hpp"
#include "../Data/Compression/Zlib.hpp"
#include "../Data/ImportExport/Text.hpp"
#include "../Helper/DateTime.hpp"
#include "../Helper/FileSystem.hpp"
#include "../Helper/Json.hpp"
#include "../Helper/Strings.hpp"
#include "../Module/Thread.hpp"
#include "../Module/Analyzer/Algo/All.hpp"
#include "../Module/Crawler/Thread.hpp"
#include "../Module/Extractor/Thread.hpp"
#include "../Module/Parser/Thread.hpp"
#include "../Query/JsonPath.hpp"
#include "../Query/JsonPointer.hpp"
#include "../Query/RegEx.hpp"
#include "../Query/XPath.hpp"
#include "../Timer/SimpleHR.hpp"
#include "../Struct/AlgoThreadProperties.hpp"
#include "../Struct/ConfigProperties.hpp"
#include "../Struct/QueryProperties.hpp"
#include "../Struct/ServerCommandResponse.hpp"
#include "../Struct/ServerSettings.hpp"
#include "../Struct/ThreadDatabaseEntry.hpp"
#include "../Struct/ThreadOptions.hpp"
#include "../Struct/UrlListProperties.hpp"
#include "../Struct/WebsiteProperties.hpp"
#include "../Wrapper/Database.hpp"

#include "../_extern/jsoncons/include/jsoncons/json.hpp"
#include "../_extern/jsoncons/include/jsoncons_ext/jsonpath/json_query.hpp"
#include "../_extern/rapidjson/include/rapidjson/document.h"
#include "../_extern/rapidjson/include/rapidjson/prettywriter.h"

#include <boost/lexical_cast.hpp>

#include <algorithm>	// std::count, std::count_if, std::find_if
#include <chrono>		// std::chrono
#include <exception>	// std::exception
#include <functional>	// std::bind, std::placeholders
#include <iostream>		// std::cout, std::flush
#include <locale>		// std::locale
#include <memory>		// std::make_unique, std::unique_ptr
#include <mutex>		// std::lock_guard, std::mutex
#include <queue>		// std::queue
#include <sstream>		// std::ostringstream
#include <string>		// std::string, std::to_string
#include <thread>		// std::thread
#include <vector>		// std::vector

namespace crawlservpp::Main {

	class Server final {
		// for convenience
		typedef Database::IncorrectPathException IncorrectPathException;
		typedef Database::PrivilegesException PrivilegesException;
		typedef Database::StorageEngineException StorageEngineException;
		typedef Helper::Json::Exception JsonException;
		typedef Parsing::XML::Exception XMLException;
		typedef Query::JsonPath::Exception JSONPathException;
		typedef Query::JsonPointer::Exception JSONPointerException;
		typedef Query::RegEx::Exception RegExException;
		typedef Query::XPath::Exception XPathException;
		typedef Struct::AlgoThreadProperties AlgoThreadProperties;
		typedef Struct::ConfigProperties ConfigProperties;
		typedef Struct::DatabaseSettings DatabaseSettings;
		typedef Struct::QueryProperties QueryProperties;
		typedef Struct::ServerCommandResponse ServerCommandResponse;
		typedef Struct::ServerSettings ServerSettings;
		typedef Struct::ThreadDatabaseEntry ThreadDatabaseEntry;
		typedef Struct::ThreadOptions ThreadOptions;
		typedef Struct::UrlListProperties UrlListProperties;
		typedef Struct::WebsiteProperties WebsiteProperties;
		typedef struct mg_connection * ConnectionPtr;

	public:
		// constructor
		Server(const DatabaseSettings& databaseSettings, const ServerSettings& serverSettings);

		// destructor
		virtual ~Server();

		// getters
		const std::string& getStatus() const;
		unsigned long getUpTime() const;
		unsigned long getActiveThreads() const;
		unsigned long getActiveWorkers() const;

		// command function
		bool tick();

		// not moveable, not copyable
		Server(Server&) = delete;
		Server(Server&&) = delete;
		Server& operator=(Server&) = delete;
		Server& operator=(Server&&) = delete;

	private:
		// settings
		ServerSettings settings;
		DatabaseSettings dbSettings;

		// database
		Database database;

		// status
		std::string status;
		std::string allowed;
		bool running;
		std::chrono::steady_clock::time_point uptimeStart;
		bool offline;

		// threads
		std::vector<std::unique_ptr<Module::Crawler::Thread>> crawlers;
		std::vector<std::unique_ptr<Module::Parser::Thread>> parsers;
		std::vector<std::unique_ptr<Module::Extractor::Thread>> extractors;
		std::vector<std::unique_ptr<Module::Analyzer::Thread>> analyzers;
		std::vector<std::thread> workers;
		std::vector<bool> workersRunning;
		mutable std::mutex workersLock;

		// access to hard-coded constants
		const std::string dirCache;
		const std::string dirCookies;

		/*  NOTE:	The web server needs to be declared after/destroyed before the database and any data,
					because it is doing one last poll on destruction!
		*/

		// web server
		WebServer webServer;

		// run server command
		std::string cmd(
				ConnectionPtr connection,
				const std::string& msgBody,
				bool& threadStartedTo,
				bool& fileDownloadTo
		);

		// set server status
		void setStatus(const std::string& statusString);

		// event handlers
		void onAccept(ConnectionPtr connection);
		void onRequest(
				ConnectionPtr connection,
				const std::string& method,
				const std::string& body,
				void * data
		);

		// static helper function for the class
		static unsigned int getAlgoFromConfig(const rapidjson::Document& json);

		// server commands used by Server::cmd(...) only
		ServerCommandResponse cmdKill(const rapidjson::Document& json, const std::string& ip);
		ServerCommandResponse cmdAllow(const rapidjson::Document& json, const std::string& ip);
		ServerCommandResponse cmdDisallow(const std::string& ip);

		ServerCommandResponse cmdLog(const rapidjson::Document& json);
		ServerCommandResponse cmdClearLog(const rapidjson::Document& jsonts, const std::string& ip);

		ServerCommandResponse cmdStartCrawler(const rapidjson::Document& json, const std::string& ip);
		ServerCommandResponse cmdPauseCrawler(const rapidjson::Document& json, const std::string& ip);
		ServerCommandResponse cmdUnpauseCrawler(const rapidjson::Document& json, const std::string& ip);
		ServerCommandResponse cmdStopCrawler(const rapidjson::Document& json, const std::string& ip);

		ServerCommandResponse cmdStartParser(const rapidjson::Document& json, const std::string& ip);
		ServerCommandResponse cmdPauseParser(const rapidjson::Document& json, const std::string& ip);
		ServerCommandResponse cmdUnpauseParser(const rapidjson::Document& json, const std::string& ip);
		ServerCommandResponse cmdStopParser(const rapidjson::Document& json, const std::string& ip);
		ServerCommandResponse cmdResetParsingStatus(const rapidjson::Document& json);

		ServerCommandResponse cmdStartExtractor(const rapidjson::Document& json, const std::string& ip);
		ServerCommandResponse cmdPauseExtractor(const rapidjson::Document& json, const std::string& ip);
		ServerCommandResponse cmdUnpauseExtractor(const rapidjson::Document& json, const std::string& ip);
		ServerCommandResponse cmdStopExtractor(const rapidjson::Document& json, const std::string& ip);
		ServerCommandResponse cmdResetExtractingStatus(const rapidjson::Document& json);

		ServerCommandResponse cmdStartAnalyzer(const rapidjson::Document& json, const std::string& ip);
		ServerCommandResponse cmdPauseAnalyzer(const rapidjson::Document& json, const std::string& ip);
		ServerCommandResponse cmdUnpauseAnalyzer(const rapidjson::Document& json, const std::string& ip);
		ServerCommandResponse cmdStopAnalyzer(const rapidjson::Document& json, const std::string& ip);
		ServerCommandResponse cmdResetAnalyzingStatus(const rapidjson::Document& json);

		ServerCommandResponse cmdPauseAll(const std::string& ip);
		ServerCommandResponse cmdUnpauseAll(const std::string& ip);

		ServerCommandResponse cmdAddWebsite(const rapidjson::Document& json);
		ServerCommandResponse cmdUpdateWebsite(const rapidjson::Document& json);
		ServerCommandResponse cmdDeleteWebsite(const rapidjson::Document& json, const std::string& ip);
		ServerCommandResponse cmdDuplicateWebsite(const rapidjson::Document& json);

		ServerCommandResponse cmdAddUrlList(const rapidjson::Document& json);
		ServerCommandResponse cmdUpdateUrlList(const rapidjson::Document& json);
		ServerCommandResponse cmdDeleteUrlList(const rapidjson::Document& json, const std::string& ip);

		void cmdImport(ConnectionPtr connection, unsigned long threadIndex, const std::string& message);
		void cmdMerge(ConnectionPtr connection, unsigned long threadIndex, const std::string& message);
		void cmdExport(ConnectionPtr connection, unsigned long threadIndex, const std::string& message);

		ServerCommandResponse cmdAddQuery(const rapidjson::Document& json);
		ServerCommandResponse cmdUpdateQuery(const rapidjson::Document& json);
		ServerCommandResponse cmdDeleteQuery(const rapidjson::Document& json);
		ServerCommandResponse cmdDuplicateQuery(const rapidjson::Document& json);

		void cmdTestQuery(ConnectionPtr connection, unsigned long threadIndex, const std::string& message);

		ServerCommandResponse cmdAddConfig(const rapidjson::Document& json);
		ServerCommandResponse cmdUpdateConfig(const rapidjson::Document& json);
		ServerCommandResponse cmdDeleteConfig(const rapidjson::Document& json);
		ServerCommandResponse cmdDuplicateConfig(const rapidjson::Document& json);

		ServerCommandResponse cmdWarp(const rapidjson::Document& json);

		ServerCommandResponse cmdDownload(const rapidjson::Document& json);

		// private helper functions
		static bool workerBegin(
				const std::string& message,
				rapidjson::Document& json,
				ServerCommandResponse& response
		);
		void workerEnd(
				unsigned long threadIndex,
				ConnectionPtr connection,
				const std::string& message,
				const ServerCommandResponse& response
		);
		static std::string generateReply(const ServerCommandResponse& response, const std::string& msgBody);
	};

} /* crawlservpp::Main */

#endif /* MAIN_SERVER_HPP_ */
