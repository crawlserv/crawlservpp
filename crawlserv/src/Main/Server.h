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

#ifndef MAIN_SERVER_H_
#define MAIN_SERVER_H_

// for debug-purposes only: write HTTP requests to console
//#define MAIN_SERVER_DEBUG_HTTP_REQUEST

// server-side sleep on mySQL error (in seconds)
#define MAIN_SERVER_SLEEP_ON_SQL_ERROR 5

#include "Database.h"
#include "WebServer.h"

#include "../Helper/DateTime.h"
#include "../Helper/Strings.h"
#include "../Module/Thread.h"
#include "../Module/Analyzer/Thread.h"
#include "../Module/Analyzer/Algo/Enum.h"
#include "../Module/Analyzer/Algo/MarkovText.h"
#include "../Module/Analyzer/Algo/MarkovTweet.h"
#include "../Module/Crawler/Thread.h"
#include "../Module/Parser/Thread.h"
#include "../Query/RegEx.h"
#include "../Query/XPath.h"
#include "../Timer/SimpleHR.h"
#include "../Struct/ConfigProperties.h"
#include "../Struct/QueryProperties.h"
#include "../Struct/ServerSettings.h"
#include "../Struct/ThreadDatabaseEntry.h"
#include "../Struct/UrlListProperties.h"
#include "../Struct/WebsiteProperties.h"

#include "../_extern/rapidjson/include/rapidjson/document.h"
#include "../_extern/rapidjson/include/rapidjson/prettywriter.h"

#include <boost/lexical_cast.hpp>
#include <experimental/filesystem>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <exception>
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
	// for convenience
	typedef crawlservpp::Parsing::XML::Exception XMLException;
	typedef crawlservpp::Query::RegEx::Exception RegExException;
	typedef crawlservpp::Query::XPath::Exception XPathException;

	class Server final {
	public:
		// constructor
		Server(const crawlservpp::Struct::DatabaseSettings& databaseSettings, const crawlservpp::Struct::ServerSettings& serverSettings);

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
		crawlservpp::Struct::ServerSettings settings;
		crawlservpp::Struct::DatabaseSettings dbSettings;

		// database
		Database database;

		// web server
		//  Needs to be declared after/destroyed before database, because it is doing one last poll on destruction!
		WebServer webServer;

		// status
		std::string status;
		std::string allowed;
		bool running;
		std::chrono::steady_clock::time_point uptimeStart;
		bool offline;

		// threads
		std::vector<std::unique_ptr<crawlservpp::Module::Crawler::Thread>> crawlers;
		std::vector<std::unique_ptr<crawlservpp::Module::Parser::Thread>> parsers;
		//std::vector<std::unique_ptr<crawlservpp::Module::Extractor::Thread>> extractors;
		std::vector<std::unique_ptr<crawlservpp::Module::Analyzer::Thread>> analyzers;
		std::vector<std::thread> workers;
		std::vector<bool> workersRunning;
		std::mutex workersLock;

		// run server command
		std::string cmd(WebServer::Connection connection, const std::string& msgBody, bool& threadStartedTo);

		// set server status
		void setStatus(const std::string& statusString);

		struct ServerCommandResponse {
			// constructor initializing a successful empty response
			ServerCommandResponse() { this->fail = false; this->confirm = false; this->id = 0; }

			// constructor initializing a successful response with text
			ServerCommandResponse(const std::string& response) : ServerCommandResponse() { this->text = response; }

			// constructor initializing a succesful response with text and id
			ServerCommandResponse(const std::string& response, unsigned long newId) : ServerCommandResponse(response) { this->id = newId; }

			// constructor initializing a possibly failed response with text
			ServerCommandResponse(bool failed, const std::string& response) : ServerCommandResponse(response) { this->fail = failed; }

			// constructor initializing a possibly failed or possibly to be confirmed response with text
			ServerCommandResponse(bool failed, bool toBeConfirmed, const std::string& response) : ServerCommandResponse(failed, response) {
				this->confirm = toBeConfirmed;
			}

			bool fail;			// command failed
			bool confirm;		// command needs to be confirmed
			std::string text;	// text of response
			unsigned long id;	// [can be used by the command to return an id]
		};

		// event handlers
		void onAccept(WebServer::Connection connection);
		void onRequest(WebServer::Connection connection, const std::string& method,	const std::string& body);

		// static helper function for the class
		static unsigned int getAlgoFromConfig(const rapidjson::Document& json);

		// server commands used by Server::cmd(...) only
		ServerCommandResponse cmdKill(const rapidjson::Document& json, const std::string& ip);
		ServerCommandResponse cmdAllow(const rapidjson::Document& json, const std::string& ip);
		ServerCommandResponse cmdDisallow(const rapidjson::Document& json, const std::string& ip);

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
		void cmdTestQuery(WebServer::Connection connection, unsigned long index, const std::string& message);

		ServerCommandResponse cmdAddConfig(const rapidjson::Document& json);
		ServerCommandResponse cmdUpdateConfig(const rapidjson::Document& json);
		ServerCommandResponse cmdDeleteConfig(const rapidjson::Document& json);
		ServerCommandResponse cmdDuplicateConfig(const rapidjson::Document& json);
	};
}

#endif /* MAIN_SERVER_H_ */
