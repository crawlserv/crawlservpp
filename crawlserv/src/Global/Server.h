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

#ifndef GLOBAL_SERVER_H_
#define GLOBAL_SERVER_H_

//#define GLOBAL_SERVER_DEBUG_HTTP_REQUEST

#include "../Global/Database.h"
#include "../Helper/DateTime.h"
#include "../Query/RegEx.h"
#include "../Query/XPath.h"
#include "../Timer/SimpleHR.h"
#include "../Struct/ServerCommandArgument.h"
#include "../Struct/ServerSettings.h"
#include "../Struct/ThreadDatabaseEntry.h"

#include "../Module/Thread.h"
#include "../Module/Crawler/Thread.h"
#include "../Module/Parser/Thread.h"

#include "../_extern/rapidjson/document.h"
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
#include "../_extern/mongoose.h"
#include "../_extern/rapidjson/prettywriter.h"

#ifdef GLOBAL_SERVER_DEBUG_HTTP_REQUEST
#include <iostream>
#endif

namespace crawlservpp::Global {
	class Server {
	public:
		Server(const crawlservpp::Struct::DatabaseSettings& databaseSettings, const crawlservpp::Struct::ServerSettings& serverSettings);
		virtual ~Server();

		bool tick();

		const std::string& getStatus() const;
		unsigned long getUpTime() const;

	protected:
		crawlservpp::Struct::ServerSettings settings;
		crawlservpp::Struct::DatabaseSettings dbSettings;
		crawlservpp::Global::Database database;
		mg_mgr eventManager;
		std::string status;
		std::string allowed;
		bool running;
		std::chrono::steady_clock::time_point uptimeStart;

		// threads
		std::vector<crawlservpp::Module::Crawler::Thread *> crawlers;
		std::vector<crawlservpp::Module::Parser::Thread *> parsers;
		//std::vector<Module::Extractor::Thread *> extractors;
		//std::vector<Module::Analyzer::Thread *> analyzers;
		std::vector<std::thread *> workers;
		std::vector<bool> workersRunning;
		std::mutex workersLock;

		// run server command
		std::string cmd(const char * body, struct mg_connection * connection, bool& threadStartedTo);

		// set server status
		void setStatus(const std::string& statusString);

		// event handler
		static void eventHandler(struct mg_connection * connection, int event, void * data);

	private:
		// static helper functions for the class
		static bool cmdCheckNameSpace(const std::string& nameSpace);
		static std::string getIP(mg_connection * connection);

		// sub-class for server responses
		class CmdResponse {
		public:
			// constructor initializing a successful empty response
			CmdResponse() { this->fail = false; this->confirm = false; this->id = 0; }

			// constructor initializing a successful response with text
			CmdResponse(const std::string& response) : CmdResponse() { this->text = response; }

			// constructor initializing a succesful response with text and id
			CmdResponse(const std::string& response, unsigned long newId) : CmdResponse(response) { this->id = newId; }

			// constructor initializing a possibly failed response with text
			CmdResponse(bool failed, const std::string& response) : CmdResponse(response) { this->fail = failed; }

			// constructor initializing a possibly failed or possibly to be confirmed response with text
			CmdResponse(bool failed, bool toBeConfirmed, const std::string& response) : CmdResponse(failed, response) {
				this->confirm = toBeConfirmed;
			}

			bool fail;			// command failed
			bool confirm;		// command needs to be confirmed
			std::string text;	// text of response
			unsigned long id;	// [can be used by the command to return an id]
		};

		// server commands used by Server::cmd(...) only
		crawlservpp::Global::Server::CmdResponse cmdKill(const rapidjson::Document& json, const std::string& ip);
		crawlservpp::Global::Server::CmdResponse cmdAllow(const rapidjson::Document& json, const std::string& ip);
		crawlservpp::Global::Server::CmdResponse cmdDisallow(const rapidjson::Document& json, const std::string& ip);

		crawlservpp::Global::Server::CmdResponse cmdLog(const rapidjson::Document& json);
		crawlservpp::Global::Server::CmdResponse cmdClearLog(const rapidjson::Document& jsonts, const std::string& ip);

		crawlservpp::Global::Server::CmdResponse cmdStartCrawler(const rapidjson::Document& json, const std::string& ip);
		crawlservpp::Global::Server::CmdResponse cmdPauseCrawler(const rapidjson::Document& json, const std::string& ip);
		crawlservpp::Global::Server::CmdResponse cmdUnPauseCrawler(const rapidjson::Document& json, const std::string& ip);
		crawlservpp::Global::Server::CmdResponse cmdStopCrawler(const rapidjson::Document& json, const std::string& ip);

		crawlservpp::Global::Server::CmdResponse cmdStartParser(const rapidjson::Document& json, const std::string& ip);
		crawlservpp::Global::Server::CmdResponse cmdPauseParser(const rapidjson::Document& json, const std::string& ip);
		crawlservpp::Global::Server::CmdResponse cmdUnPauseParser(const rapidjson::Document& json, const std::string& ip);
		crawlservpp::Global::Server::CmdResponse cmdStopParser(const rapidjson::Document& json, const std::string& ip);
		crawlservpp::Global::Server::CmdResponse cmdResetParsingStatus(const rapidjson::Document& json);

		//TODO: Server commands for extractor
		crawlservpp::Global::Server::CmdResponse cmdResetExtractingStatus(const rapidjson::Document& json);

		//TODO: Server commands for analyzer
		crawlservpp::Global::Server::CmdResponse cmdResetAnalyzingStatus(const rapidjson::Document& json);

		crawlservpp::Global::Server::CmdResponse cmdAddWebsite(const rapidjson::Document& json);
		crawlservpp::Global::Server::CmdResponse cmdUpdateWebsite(const rapidjson::Document& json);
		crawlservpp::Global::Server::CmdResponse cmdDeleteWebsite(const rapidjson::Document& json, const std::string& ip);
		crawlservpp::Global::Server::CmdResponse cmdDuplicateWebsite(const rapidjson::Document& json);

		crawlservpp::Global::Server::CmdResponse cmdAddUrlList(const rapidjson::Document& json);
		crawlservpp::Global::Server::CmdResponse cmdUpdateUrlList(const rapidjson::Document& json);
		crawlservpp::Global::Server::CmdResponse cmdDeleteUrlList(const rapidjson::Document& json, const std::string& ip);

		crawlservpp::Global::Server::CmdResponse cmdAddQuery(const rapidjson::Document& json);
		crawlservpp::Global::Server::CmdResponse cmdUpdateQuery(const rapidjson::Document& json);
		crawlservpp::Global::Server::CmdResponse cmdDeleteQuery(const rapidjson::Document& json);
		crawlservpp::Global::Server::CmdResponse cmdDuplicateQuery(const rapidjson::Document& json);
		void cmdTestQuery(unsigned long index, const char * body, struct mg_connection * connection);

		crawlservpp::Global::Server::CmdResponse cmdAddConfig(const rapidjson::Document& json);
		crawlservpp::Global::Server::CmdResponse cmdUpdateConfig(const rapidjson::Document& json);
		crawlservpp::Global::Server::CmdResponse cmdDeleteConfig(const rapidjson::Document& json);
		crawlservpp::Global::Server::CmdResponse cmdDuplicateConfig(const rapidjson::Document& json);
	};
}

#endif /* GLOBAL_SERVER_H_ */
