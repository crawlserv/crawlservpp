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

#include "Database.h"

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
#include "../Struct/ServerSettings.h"
#include "../Struct/ThreadDatabaseEntry.h"

#include "../_extern/mongoose/mongoose.h"
#include "../_extern/rapidjson/include/rapidjson/document.h"
#include "../_extern/rapidjson/include/rapidjson/prettywriter.h"

#include <boost/lexical_cast.hpp>
#include <experimental/filesystem>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <exception>
#include <functional>
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
	public:
		Server(const crawlservpp::Struct::DatabaseSettings& databaseSettings, const crawlservpp::Struct::ServerSettings& serverSettings);
		virtual ~Server();

		bool tick();

		const std::string& getStatus() const;
		unsigned long getUpTime() const;

	private:
		crawlservpp::Struct::ServerSettings settings;
		crawlservpp::Struct::DatabaseSettings dbSettings;
		Database database;
		mg_mgr eventManager;
		std::string status;
		std::string allowed;
		bool running;
		std::chrono::steady_clock::time_point uptimeStart;

		// threads
		std::vector<crawlservpp::Module::Crawler::Thread *> crawlers;
		std::vector<crawlservpp::Module::Parser::Thread *> parsers;
		//std::vector<crawlservpp::Module::Extractor::Thread *> extractors;
		std::vector<crawlservpp::Module::Analyzer::Thread *> analyzers;
		std::vector<std::thread *> workers;
		std::vector<bool> workersRunning;
		std::mutex workersLock;

		// run server command
		std::string cmd(const char * body, struct mg_connection * connection, bool& threadStartedTo);

		// set server status
		void setStatus(const std::string& statusString);

		// event handler
		static void eventHandler(struct mg_connection * connection, int event, void * data);

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

		// static helper functions for the class
		static std::string getIP(mg_connection * connection);
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
		void cmdTestQuery(unsigned long index, const char * body, struct mg_connection * connection);

		ServerCommandResponse cmdAddConfig(const rapidjson::Document& json);
		ServerCommandResponse cmdUpdateConfig(const rapidjson::Document& json);
		ServerCommandResponse cmdDeleteConfig(const rapidjson::Document& json);
		ServerCommandResponse cmdDuplicateConfig(const rapidjson::Document& json);
	};
}

#endif /* MAIN_SERVER_H_ */
