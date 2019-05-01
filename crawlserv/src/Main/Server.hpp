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

#define RAPIDJSON_HAS_STDSTRING 1

// hard-coded constants
#define MAIN_SERVER_DIR_CACHE "cache" // directory for file cache
#define MAIN_SERVER_DIR_COOKIES "cookies" // directory for cookies
#define MAIN_SERVER_SLEEP_ON_SQL_ERROR_SEC 5 // server-side sleep on mySQL error

#include "Database.hpp"
#include "WebServer.hpp"

#include "../Helper/DateTime.hpp"
#include "../Helper/FileSystem.hpp"
#include "../Helper/Json.hpp"
#include "../Helper/Strings.hpp"
#include "../Module/Thread.hpp"
#include "../Module/Analyzer/Algo/All.hpp"
#include "../Module/Crawler/Thread.hpp"
#include "../Module/Parser/Thread.hpp"
#include "../Query/JsonPath.hpp"
#include "../Query/JsonPointer.hpp"
#include "../Query/RegEx.hpp"
#include "../Query/XPath.hpp"
#include "../Timer/SimpleHR.hpp"
#include "../Struct/AlgoThreadProperties.hpp"
#include "../Struct/ConfigProperties.hpp"
#include "../Struct/QueryProperties.hpp"
#include "../Struct/ServerSettings.hpp"
#include "../Struct/ThreadDatabaseEntry.hpp"
#include "../Struct/UrlListProperties.hpp"
#include "../Struct/WebsiteProperties.hpp"

#include "../_extern/jsoncons/include/jsoncons/json.hpp"
#include "../_extern/jsoncons/include/jsoncons_ext/jsonpath/json_query.hpp"
#include "../_extern/rapidjson/include/rapidjson/document.h"
#include "../_extern/rapidjson/include/rapidjson/prettywriter.h"

#include <boost/lexical_cast.hpp>
#include <experimental/filesystem>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <exception>
#include <functional>
#include <iostream>
#include <locale>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

#ifdef MAIN_SERVER_DEBUG_HTTP_REQUEST
#include <iostream>
#endif

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
		typedef Struct::ServerSettings ServerSettings;
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
		//std::vector<std::unique_ptr<Module::Extractor::Thread>> extractors;
		std::vector<std::unique_ptr<Module::Analyzer::Thread>> analyzers;
		std::vector<std::thread> workers;
		std::vector<bool> workersRunning;
		std::mutex workersLock;

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

		struct ServerCommandResponse {
			// constructor initializing a successful empty response
			ServerCommandResponse()
					: fail(false), confirm(false), id(0) {}

			// constructor initializing a successful response with text
			ServerCommandResponse(const std::string& response)
					: fail(false), confirm(false), text(response), id(0) {}

			// constructor initializing a succesful response with text and id
			ServerCommandResponse(const std::string& response, unsigned long newId)
					: fail(false), confirm(false), text(response), id(newId) {}

			// constructor initializing a possibly failed response with text
			ServerCommandResponse(bool failed, const std::string& response)
					: fail(failed), confirm(false), text(response), id(0) {}

			// constructor initializing a possibly failed or possibly to be confirmed response with text
			ServerCommandResponse(bool failed, bool toBeConfirmed, const std::string& response)
					: fail(failed), confirm(toBeConfirmed), text(response), id(0) {}

			bool fail;			// command failed
			bool confirm;		// command needs to be confirmed
			std::string text;	// text of response
			unsigned long id;	// [can be used by the command to return an id]
		};

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

		//TODO: Server commands for extractor
		ServerCommandResponse cmdResetExtractingStatus(const rapidjson::Document& json);

		ServerCommandResponse cmdStartAnalyzer(const rapidjson::Document& json, const std::string& ip);
		ServerCommandResponse cmdPauseAnalyzer(const rapidjson::Document& json, const std::string& ip);
		ServerCommandResponse cmdUnpauseAnalyzer(const rapidjson::Document& json, const std::string& ip);
		ServerCommandResponse cmdStopAnalyzer(const rapidjson::Document& json, const std::string& ip);
		ServerCommandResponse cmdResetAnalyzingStatus(const rapidjson::Document& json);

		ServerCommandResponse cmdAddWebsite(const rapidjson::Document& json);
		ServerCommandResponse cmdUpdateWebsite(const rapidjson::Document& json);
		ServerCommandResponse cmdDeleteWebsite(const rapidjson::Document& json, const std::string& ip);
		ServerCommandResponse cmdDuplicateWebsite(const rapidjson::Document& json);

		ServerCommandResponse cmdAddUrlList(const rapidjson::Document& json);
		ServerCommandResponse cmdUpdateUrlList(const rapidjson::Document& json);
		ServerCommandResponse cmdDeleteUrlList(const rapidjson::Document& json, const std::string& ip);

		ServerCommandResponse cmdAddQuery(const rapidjson::Document& json);
		ServerCommandResponse cmdUpdateQuery(const rapidjson::Document& json);
		ServerCommandResponse cmdDeleteQuery(const rapidjson::Document& json);
		ServerCommandResponse cmdDuplicateQuery(const rapidjson::Document& json);
		void cmdTestQuery(ConnectionPtr connection, unsigned long index, const std::string& message);

		ServerCommandResponse cmdAddConfig(const rapidjson::Document& json);
		ServerCommandResponse cmdUpdateConfig(const rapidjson::Document& json);
		ServerCommandResponse cmdDeleteConfig(const rapidjson::Document& json);
		ServerCommandResponse cmdDuplicateConfig(const rapidjson::Document& json);

		ServerCommandResponse cmdWarp(const rapidjson::Document& json);

		ServerCommandResponse cmdDownload(const rapidjson::Document& json);
	};

} /* crawlservpp::Main */

#endif /* MAIN_SERVER_HPP_ */
