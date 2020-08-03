/*
 *
 * ---
 *
 *  Copyright (C) 2020 Anselm Schmidt (ans[ät]ohai.su)
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
 * Server.hpp
 *
 * The command-and-control server.
 *
 * Uses the mongoose and RapidJSON libraries to implement a HTTP server
 *  for receiving JSON-formatted commands and sending JSON-formatted
 *  replies from/to the crawlserv_frontend.
 *
 *  Also handles all threads for the different modules as well as
 *   specific worker threads for specific server tasks.
 *
 *  Created on: Oct 7, 2018
 *      Author: ans
 */

#ifndef MAIN_SERVER_HPP_
#define MAIN_SERVER_HPP_

/*
 * DEBUGGING
 */

// directives that allow to deactivate whole components for debugging purposes ONLY
#ifndef NDEBUG
	//#define MAIN_SERVER_DEBUG_NOCRAWLERS
	//#define MAIN_SERVER_DEBUG_NOPARSERS
	//#define MAIN_SERVER_DEBUG_NOEXTRACTORS
	//#define MAIN_SERVER_DEBUG_NOANALYZERS
#endif

/*
 * MACROS
 */

// macro for server commands

//NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define MAIN_SERVER_CMD(X, Y)	if(name == (X)) { \
										response = Y(); \
										return true; \
									}


// macros for exception handling of worker threads

//NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define MAIN_SERVER_WORKER_BEGIN	try {
//NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define MAIN_SERVER_WORKER_END(X)	} \
									catch(const std::exception& e) { \
										(X) = ServerCommandResponse::failed( \
												e.what() \
										); \
									}

/*
 * INCLUDES
 */

#include "Database.hpp"
#include "Exception.hpp"
#include "WebServer.hpp"

#include "../Data/Compression/Gzip.hpp"
#include "../Data/Compression/Zlib.hpp"
#include "../Data/File.hpp"
#include "../Data/ImportExport/Text.hpp"
#include "../Helper/DateTime.hpp"
#include "../Helper/FileSystem.hpp"
#include "../Helper/Json.hpp"
#include "../Helper/Strings.hpp"
#include "../Module/Analyzer/Algo/All.hpp"
#include "../Module/Crawler/Thread.hpp"
#include "../Module/Extractor/Thread.hpp"
#include "../Module/Parser/Thread.hpp"
#include "../Module/Thread.hpp"
#include "../Query/JsonPath.hpp"
#include "../Query/JsonPointer.hpp"
#include "../Query/RegEx.hpp"
#include "../Query/XPath.hpp"
#include "../Struct/AlgoThreadProperties.hpp"
#include "../Struct/ConfigProperties.hpp"
#include "../Struct/NetworkSettings.hpp"
#include "../Struct/QueryProperties.hpp"
#include "../Struct/ServerCommandResponse.hpp"
#include "../Struct/ServerSettings.hpp"
#include "../Struct/ThreadDatabaseEntry.hpp"
#include "../Struct/ThreadOptions.hpp"
#include "../Struct/UrlListProperties.hpp"
#include "../Struct/WebsiteProperties.hpp"
#include "../Timer/SimpleHR.hpp"
#include "../Wrapper/Database.hpp"

#include "../_extern/jsoncons/include/jsoncons/json.hpp"
#include "../_extern/jsoncons/include/jsoncons_ext/jsonpath/json_query.hpp"
#include "../_extern/rapidjson/include/rapidjson/document.h"
#include "../_extern/rapidjson/include/rapidjson/prettywriter.h"

#include <boost/lexical_cast.hpp>

#include <algorithm>	// std::count, std::count_if, std::find_if
#include <chrono>		// std::chrono
#include <cstddef>		// std::size_t
#include <cstdint>		// std::uint32_t, std::uint64_t
#include <exception>	// std::exception
#include <iostream>		// std::cout, std::flush
#include <locale>		// std::locale
#include <memory>		// std::make_unique, std::unique_ptr
#include <mutex>		// std::lock_guard, std::mutex
#include <optional>		// std::nullopt
#include <queue>		// std::queue
#include <sstream>		// std::ostringstream
#include <string>		// std::string, std::to_string
#include <string_view>	// std::string_view
#include <thread>		// std::thread
#include <utility>		// std::pair
#include <vector>		// std::vector

namespace crawlservpp::Main {

	/*
	 * CONSTANTS
	 */

	///@name Constants
	///@{

	//! The name of the (sub-)directory for the file cache.
	constexpr std::string_view cacheDir{"cache"};

	//! The name of the (sub-)directory for cookies.
	constexpr std::string_view cookieDir{"cookies"};

	//! The name of the (sub-)directory for debugging.
	constexpr std::string_view debugDir{"debug"};

	//! The number of seconds for the server to sleep when a MySQL error occured.
	constexpr auto sleepOnSqlErrorS{5};

	//! The timeout in milliseconds for the polling of the web server.
	constexpr auto webServerPollTimeOutMs{1000};

	//! The HTTP status code for GET replies indicating the status of the server.
	constexpr auto statusHttpCode{200};

	//! The HTTP content type for GET replies indicating the status of the server.
	constexpr auto statusHttpContentType{"text/plain"};

	//! The HTTP status code for POST replies.
	constexpr auto replyHttpCode{200};

	//! The HTTP content type for POST replies.
	constexpr auto replyHttpContentType{"application/json"};

	//! The HTTP status code for OPTIONS replies.
	constexpr auto optionsHttpCode{200};

	//! The minimum length of namespaces.
	constexpr auto minNameSpaceLength{3};

	//! The minimum length of namespaces, as string.
	constexpr std::string_view minNameSpaceLengthString{"three"};

	//! The beginning of URLs using the HTTP protocol.
	constexpr std::string_view httpString{"http://"};

	//! The beginning of URLs using the HTTPS protocol.
	constexpr std::string_view httpsString{"https://"};

	//! The number of XML warnings by default.
	constexpr auto xmlWarningsDefault{25};

	///@}

	/*
	 * DECLARATION
	 */

	//! The command-and-control server.
	/*!
	 * Uses the Main::WebServer and the @c rapidJSON
	 *  library to implement a HTTP server for
	 *  receiving JSON-formatted commands and
	 *  sending JSON-formatted replies from/to the
	 *  @c crawlserv_frontend(s).
	 *
	 * Also handles the threads for the different modules
	 *  as well as worker threads for specific server
	 *  tasks.
	 *
	 * For more information about the @c rapidJSON library,
	 *  see its <a href="https://github.com/Tencent/rapidjson">
	 *  GitHub repository</a>.
	 *
	 * \sa Main::WebServer
	 */
	class Server final {
		// for convenience
		using DatabaseException = Database::Exception;
		using IncorrectPathException = Database::IncorrectPathException;
		using PrivilegesException = Database::PrivilegesException;
		using StorageEngineException = Database::StorageEngineException;
		using WrongArgumentsException = Database::WrongArgumentsException;

		using DateTimeException = Helper::DateTime::Exception;
		using JsonException = Helper::Json::Exception;
		using LocaleException = Helper::DateTime::LocaleException;

		using XMLException = Parsing::XML::Exception;

		using JSONPathException = Query::JsonPath::Exception;
		using JSONPointerException = Query::JsonPointer::Exception;
		using RegExException = Query::RegEx::Exception;
		using XPathException = Query::XPath::Exception;

		using AlgoThreadProperties = Struct::AlgoThreadProperties;
		using ConfigProperties = Struct::ConfigProperties;
		using DatabaseSettings = Struct::DatabaseSettings;
		using NetworkSettings = Struct::NetworkSettings;
		using QueryProperties = Struct::QueryProperties;
		using ServerCommandResponse = Struct::ServerCommandResponse;
		using ServerSettings = Struct::ServerSettings;
		using ThreadDatabaseEntry = Struct::ThreadDatabaseEntry;
		using ThreadOptions = Struct::ThreadOptions;
		using UrlListProperties = Struct::UrlListProperties;
		using WebsiteProperties = Struct::WebsiteProperties;

		using ConnectionPtr = mg_connection *;
		using StringString = std::pair<std::string, std::string>;
		using Queries = std::vector<std::pair<std::string, std::vector<StringString>>>;

	public:
		///@name Construction and Destruction
		///@{

		Server(
				const ServerSettings& serverSettings,
				const DatabaseSettings& databaseSettings,
				const NetworkSettings& networkSettings
		);
		virtual ~Server();

		///@}
		///@name Getters
		///@{

		const std::string& getStatus() const;
		std::int64_t getUpTime() const;
		std::size_t getActiveThreads() const;
		std::size_t getActiveWorkers() const;

		///@}
		///@name Server Ticks
		///@{

		bool tick();

		///@}
		/**@name Copy and Move
		 * The class is neither copyable, nor movable.
		 */
		///@{

		//! Deleted copy constructor.
		Server(Server&) = delete;

		//! Deleted copy assignment operator.
		Server& operator=(Server&) = delete;

		//! Deleted move constructor.
		Server(Server&&) = delete;

		//! Deleted move assignment operator.
		Server& operator=(Server&&) = delete;

		///@}

	private:
		// settings
		ServerSettings settings;
		DatabaseSettings dbSettings;
		NetworkSettings netSettings;

		// database
		Database database;

		// status
		std::string status;
		std::string allowed;
		bool running{true};
		std::chrono::steady_clock::time_point uptimeStart;
		bool offline{true};

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

		/*
		 * NOTE: The web server needs to be declared after/destroyed before the database and any data,
		 * 		  because it is doing one last poll on destruction!
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
		void setStatus(const std::string& statusMsg);

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
		ServerCommandResponse cmdMoveQuery();
		ServerCommandResponse cmdDeleteQuery();
		ServerCommandResponse cmdDuplicateQuery();

		ServerCommandResponse cmdAddConfig();
		ServerCommandResponse cmdUpdateConfig();
		ServerCommandResponse cmdDeleteConfig();
		ServerCommandResponse cmdDuplicateConfig();

		ServerCommandResponse cmdWarp();

		ServerCommandResponse cmdDownload();

		void cmdImport(ConnectionPtr connection, std::size_t threadIndex, const std::string& message);
		void cmdMerge(ConnectionPtr connection, std::size_t threadIndex, const std::string& message);
		void cmdExport(ConnectionPtr connection, std::size_t threadIndex, const std::string& message);

		void cmdDeleteUrls(ConnectionPtr connection, std::size_t threadIndex, const std::string& message);

		void cmdTestQuery(ConnectionPtr connection, std::size_t threadIndex, const std::string& message);

		// private helper functions
		bool getArgument(
				const std::string& name,
				std::string& out,
				bool optional,
				bool notEmpty,
				std::string& outError
		);
		bool getArgument(
				const std::string& name,
				std::uint64_t& out,
				std::string& outError
		);
		bool getArgument(
				const std::string& name,
				bool& out,
				bool optional,
				std::string& outError
		);
		static bool getArgument(
				const rapidjson::Document& json,
				const std::string& name,
				std::string& out,
				bool optional,
				bool notEmpty,
				std::string& outError
		);
		static bool getArgument(
				const rapidjson::Document& json,
				const std::string& name,
				std::uint64_t& out,
				std::string& outError
		);
		static bool getArgument(
				const rapidjson::Document& json,
				const std::string& name,
				bool& out,
				bool optional,
				std::string& outError
		);
		static void correctDomain(std::string& inOut);
		static bool workerBegin(
				const std::string& message,
				rapidjson::Document& json,
				ServerCommandResponse& response
		);
		void workerEnd(
				std::size_t threadIndex,
				ConnectionPtr connection,
				const std::string& message,
				const ServerCommandResponse& response
		);
		static std::uint32_t getAlgoFromConfig(const rapidjson::Document& json);
		static std::string generateReply(
				const ServerCommandResponse& response,
				const std::string& msgBody
		);
		static std::string dateTimeTest(
				const std::string& input,
				const std::string& format,
				const std::string& locale
		);
	};

} /* namespace crawlservpp::Main */

#endif /* MAIN_SERVER_HPP_ */
