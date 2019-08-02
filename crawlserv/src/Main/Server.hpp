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
#define MAIN_SERVER_DIR_CACHE "cache"			// directory for file cache
#define MAIN_SERVER_DIR_COOKIES "cookies"		// directory for cookies
#define MAIN_SERVER_SLEEP_ON_SQL_ERROR_SEC 5	// server-side sleep on mySQL error#

// macro for basic server commands
#define MAIN_SERVER_BASIC_CMD(X, Y)	if(name == X) { \
										response = Y(); \
										return true; \
									}


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

		// pointer to information for basic commands
		rapidjson::Document cmdJson;
		std::string cmdIp;

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
		bool cmd(const std::string& name, ServerCommandResponse& response);

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

		// server commands used by Server::cmd(...) only
		ServerCommandResponse cmdKill();
		ServerCommandResponse cmdAllow();
		ServerCommandResponse cmdDisallow();

		ServerCommandResponse cmdLog();
		ServerCommandResponse cmdClearLogs();

		ServerCommandResponse cmdStartCrawler();
		ServerCommandResponse cmdPauseCrawler();
		ServerCommandResponse cmdUnpauseCrawler();
		ServerCommandResponse cmdStopCrawler();

		ServerCommandResponse cmdStartParser();
		ServerCommandResponse cmdPauseParser();
		ServerCommandResponse cmdUnpauseParser();
		ServerCommandResponse cmdStopParser();
		ServerCommandResponse cmdResetParsingStatus();

		ServerCommandResponse cmdStartExtractor();
		ServerCommandResponse cmdPauseExtractor();
		ServerCommandResponse cmdUnpauseExtractor();
		ServerCommandResponse cmdStopExtractor();
		ServerCommandResponse cmdResetExtractingStatus();

		ServerCommandResponse cmdStartAnalyzer();
		ServerCommandResponse cmdPauseAnalyzer();
		ServerCommandResponse cmdUnpauseAnalyzer();
		ServerCommandResponse cmdStopAnalyzer();
		ServerCommandResponse cmdResetAnalyzingStatus();

		ServerCommandResponse cmdPauseAll();
		ServerCommandResponse cmdUnpauseAll();

		ServerCommandResponse cmdAddWebsite();
		ServerCommandResponse cmdUpdateWebsite();
		ServerCommandResponse cmdDeleteWebsite();
		ServerCommandResponse cmdDuplicateWebsite();

		ServerCommandResponse cmdAddUrlList();
		ServerCommandResponse cmdUpdateUrlList();
		ServerCommandResponse cmdDeleteUrlList();

		ServerCommandResponse cmdAddQuery();
		ServerCommandResponse cmdUpdateQuery();
		ServerCommandResponse cmdDeleteQuery();
		ServerCommandResponse cmdDuplicateQuery();

		ServerCommandResponse cmdAddConfig();
		ServerCommandResponse cmdUpdateConfig();
		ServerCommandResponse cmdDeleteConfig();
		ServerCommandResponse cmdDuplicateConfig();

		ServerCommandResponse cmdWarp();

		ServerCommandResponse cmdDownload();

		void cmdImport(ConnectionPtr connection, unsigned long threadIndex, const std::string& message);
		void cmdMerge(ConnectionPtr connection, unsigned long threadIndex, const std::string& message);
		void cmdExport(ConnectionPtr connection, unsigned long threadIndex, const std::string& message);

		void cmdTestQuery(ConnectionPtr connection, unsigned long threadIndex, const std::string& message);

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

		// private static helper functions
		static unsigned int getAlgoFromConfig(const rapidjson::Document& json);
		static std::string generateReply(const ServerCommandResponse& response, const std::string& msgBody);
	};

} /* crawlservpp::Main */

#endif /* MAIN_SERVER_HPP_ */
