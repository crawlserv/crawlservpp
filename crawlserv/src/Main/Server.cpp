/*
 * Server.cpp
 *
 * The command-and-control server. Uses the mongoose and RapidJSON libraries to implement a HTTP server for receiving
 *  JSON-formatted commands and sending JSON-formatted replies from/to the crawlserv_frontend.
 *
 *  Also handles all threads for the different modules as well as worker threads for query testing.
 *
 *  Created on: Oct 7, 2018
 *      Author: ans
 */

#include "../Main/Server.h"

namespace crawlservpp::Main {

// constructor
Server::Server(const crawlservpp::Struct::DatabaseSettings& databaseSettings,
		const crawlservpp::Struct::ServerSettings& serverSettings) : settings(serverSettings), dbSettings(databaseSettings),
				database(databaseSettings, "server"), allowed(serverSettings.allowedClients), running(true), offline(true) {
	// create cookies directory if it does not exist
	if(!std::experimental::filesystem::is_directory("cookies") || !std::experimental::filesystem::exists("cookies")) {
		std::experimental::filesystem::create_directory("cookies");
	}

	// connect to database and initialize it
	this->database.setSleepOnError(MAIN_SERVER_SLEEP_ON_SQL_ERROR_SECONDS);
	this->database.connect();
	this->database.initializeSql();
	this->database.prepare();
	this->offline = false;

	// set callbacks (suppressing wrong error messages by Eclipse IDE)
	this->webServer.setAcceptCallback( // @suppress("Invalid arguments")
			std::bind(&Server::onAccept, this, std::placeholders::_1));
	this->webServer.setRequestCallback( // @suppress("Invalid arguments")
			std::bind(&Server::onRequest, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

	// initialize mongoose embedded web server, bind it to port and set CORS string
	this->webServer.initHTTP(serverSettings.port);
	this->webServer.setCORS("*");

	// set initial status
	this->setStatus("crawlserv is ready");

	// load threads from database
	std::vector<Struct::ThreadDatabaseEntry> threads = this->database.getThreads();
	for(auto i = threads.begin(); i != threads.end(); ++i) {
		if(i->module == "crawler") {
			// load crawler thread
			this->crawlers.push_back(std::make_unique<crawlservpp::Module::Crawler::Thread>(
					this->database, i->id, i->status, i->paused, i->options, i->last));
			this->crawlers.back()->crawlservpp::Module::Thread::start();

			// write to log
			std::ostringstream logStrStr;
			logStrStr << "crawler #" << i->id << " continued.";
			this->database.log(logStrStr.str());
		}
		else if(i->module == "parser") {
			// load parser thread
			this->parsers.push_back(std::make_unique<crawlservpp::Module::Parser::Thread>(
					this->database, i->id, i->status, i->paused, i->options, i->last));
			this->parsers.back()->crawlservpp::Module::Thread::start();

			// write to log
			std::ostringstream logStrStr;
			logStrStr << "parser #" << i->id << " continued.";
			this->database.log(logStrStr.str());
		}
		else if(i->module == "extractor") {
			// load extractor thread
			/*
			this->extractors.push_back(std::make_unique<crawlservpp::Module::Extractor::Thread>(
					this->database, i->id, i->status, i->paused, i->options, i->last));
			this->extractors.back()->crawlservpp::Module::Thread::start();
			*/

			// write to log
			std::ostringstream logStrStr;
			logStrStr << "extractor #" << i->id << " continued.";
			this->database.log(logStrStr.str());
		}
		else if(i->module == "analyzer") {
			// get and parse config JSON to determine algorithm
			rapidjson::Document configJson;
			std::string config = this->database.getConfiguration(i->options.config);
			if(configJson.Parse(config.c_str()).HasParseError()) throw std::runtime_error("Could not parse configuration JSON.");
			if(!configJson.IsArray()) throw std::runtime_error("Parsed configuration JSON is not an array.");

			switch(Server::getAlgoFromConfig(configJson)) {
			case crawlservpp::Module::Analyzer::Algo::Enum::markovText:
				this->analyzers.push_back(std::make_unique<crawlservpp::Module::Analyzer::Algo::MarkovText>(
						this->database, i->id, i->status, i->paused, i->options, i->last));
				break;
			case crawlservpp::Module::Analyzer::Algo::Enum::markovTweet:
				this->analyzers.push_back(std::make_unique<crawlservpp::Module::Analyzer::Algo::MarkovTweet>(
						this->database, i->id, i->status, i->paused, i->options, i->last));
				break;
			default:
				this->database.log("WARNING: Unknown algorithm ignored when loading threads.");
				continue;
			}
			this->analyzers.back()->crawlservpp::Module::Thread::start();

			// write to log
			std::ostringstream logStrStr;
			logStrStr << "analyzer #" << i->id << " continued.";
			this->database.log(logStrStr.str());
		}
		else throw std::runtime_error("Unknown thread module \'" + i->module + "\'");
	}

	// save start time for up-time calculation
	this->uptimeStart = std::chrono::steady_clock::now();

	// start logging
	this->database.log("started.");
}

// destructor
Server::~Server() {
	// interrupt module threads
	for(auto i = this->crawlers.begin(); i != this->crawlers.end(); ++i) (*i)->crawlservpp::Module::Thread::sendInterrupt();
	for(auto i = this->parsers.begin(); i != this->parsers.end(); ++i) (*i)->crawlservpp::Module::Thread::sendInterrupt();
	//for(auto i = this->extractors.begin(); i != this->extractors.end(); ++i) (*i)->crawlservpp::Module::Thread::sendInterrupt();
	for(auto i = this->analyzers.begin(); i != this->analyzers.end(); ++i) (*i)->crawlservpp::Module::Thread::sendInterrupt();

	// wait for module threads
	for(auto i = this->crawlers.begin(); i != this->crawlers.end(); ++i) {
		if(*i) {
			// get thread ID (for logging)
			unsigned long id = (*i)->getId();

			// wait for thread
			(*i)->crawlservpp::Module::Thread::finishInterrupt();

			// log interruption
			std::ostringstream logStrStr;
			logStrStr << "crawler #" << id << " interrupted.";
			try {
				this->database.log(logStrStr.str());
			}
			catch(const Database::Exception& e) {
				std::cout << std::endl << logStrStr.str()
						<< std::endl << "Could not write to log: " << e.what() << std::flush;
			}
			catch(...) {
				std::cout << std::endl << logStrStr.str()
						<< "ERROR: Unknown exception in Server::~Server()" << std::flush;
			}
		}
	}
	for(auto i = this->parsers.begin(); i != this->parsers.end(); ++i) {
		if(*i) {
			// get thread ID (for logging)
			unsigned long id = (*i)->getId();

			// wait for thread
			(*i)->crawlservpp::Module::Thread::finishInterrupt();

			// log interruption
			std::ostringstream logStrStr;
			logStrStr << "parser #" << id << " interrupted.";
			try {
				this->database.log(logStrStr.str());
			}
			catch(const Database::Exception& e) {
				std::cout << std::endl << logStrStr.str()
						<< std::endl << "Could not write to log: " << e.what() << std::flush;
			}
			catch(...) {
				std::cout << std::endl << logStrStr.str()
						<< std::endl << "ERROR: Unknown exception in Server::~Server()" << std::flush;
			}
		}
	}
	this->parsers.clear();
	/*for(auto i = this->extractors.begin(); i != this->extractors.end(); ++i) {
		if(*i) {
			// get thread ID (for logging)
			unsigned long id = (*i)->getId();

			// wait for thread
			(*i)->crawlservpp::Module::Thread::finishInterrupt();

			// log interruption
			std::ostringstream logStrStr;
			logStrStr << "extractor #" << id << " interrupted.";
			try {
				this->database.log(logStrStr.str());
			}
			catch(const Database::Exception& e) {
				std::cout << std::endl << logStrStr.str()
						<< std::endl << "Could not write to log: " << e.what() << std::flush;
			}
			catch(...) {
				std::cout << std::endl << logStrStr.str()
						<< std::endl << "ERROR: Unknown exception in Server::~Server()" << std::flush;
			}
		}
	}*/
	for(auto i = this->analyzers.begin(); i != this->analyzers.end(); ++i) {
		if(*i) {
			// get thread ID (for logging)
			unsigned long id = (*i)->getId();

			// wait for thread
			(*i)->crawlservpp::Module::Thread::finishInterrupt();

			// log interruption
			std::ostringstream logStrStr;
			logStrStr << "analyzer #" << id << " interrupted.";
			try {
				this->database.log(logStrStr.str());
			}
			catch(const Database::Exception& e) {
				std::cout << std::endl << logStrStr.str()
						<< std::endl << "Could not write to log: " << e.what() << std::flush;
			}
			catch(...) {
				std::cout << std::endl << logStrStr.str()
						<< std::endl << "ERROR: Unknown exception in Server::~Server()" << std::flush;
			}
		}
	}

	// wait for worker threads
	for(auto i = this->workers.begin(); i != this->workers.end(); ++i) if(i->joinable()) i->join();

	// log shutdown message with server up-time
	try {
		this->database.log("shuts down after up-time of "
				+ crawlservpp::Helper::DateTime::secondsToString(this->getUpTime()) + ".");
	}
	catch(const Database::Exception& e) {
		std::cout << "server shuts down after up-time of"
				<< crawlservpp::Helper::DateTime::secondsToString(this->getUpTime()) << "."
				<< std::endl << "Could not write to log: " << e.what() << std::flush;
	}
	catch(...) {
		std::cout << "server shuts down after up-time of"
				<< crawlservpp::Helper::DateTime::secondsToString(this->getUpTime()) << "."
				<< std::endl << "ERROR: Unknown exception in Server::~Server()" << std::flush;
	}
}

// server tick, return whether server is still running
bool Server::tick() {
	// poll web server
	try {
		this->webServer.poll(1000);
	}
	catch(const Database::Exception& e) {
		// try to re-connect once on database exception
		try {
			this->database.checkConnection();
			this->database.log("re-connected to database after error: " + e.whatStr());
		}
		catch(const Database::Exception& e) {
			// database is offline
			this->offline = true;
		}
	}

	// check whether a thread was terminated
	for(unsigned long n = 1; n <= this->crawlers.size(); n++) {
		if(this->crawlers.at(n - 1)->isTerminated()) {
			this->crawlers.at(n - 1)->crawlservpp::Module::Thread::stop();
			this->crawlers.erase(this->crawlers.begin() + (n - 1));
			n--;
		}
	}
	for(unsigned long n = 1; n <= this->parsers.size(); n++) {
		if(this->parsers.at(n - 1)->isTerminated()) {
			this->parsers.at(n - 1)->crawlservpp::Module::Thread::stop();
			this->parsers.erase(this->parsers.begin() + (n - 1));
			n--;
		}
	}
	/*for(unsigned long n = 1; n <= this->extractors.size(); n++) {
		if(this->extractors.at(n - 1)->isTerminated()) {
			this->extractors.at(n - 1)->crawlservpp::Module::Thread::stop();
			this->extractors.erase(this->extractors.begin() + (n - 1));
			n--;
		}
	}*/
	for(unsigned long n = 1; n <= this->analyzers.size(); n++) {
		if(this->analyzers.at(n - 1)->isTerminated()) {
			this->analyzers.at(n - 1)->crawlservpp::Module::Thread::stop();
			this->analyzers.erase(this->analyzers.begin() + (n - 1));
			n--;
		}
	}

	// check whether worker threads were terminated
	if(!(this->workers.empty())) {
		std::lock_guard<std::mutex> workersLocked(this->workersLock);
		for(unsigned long n = 1; n <= this->workers.size(); n++) {
			if(!(this->workersRunning.at(n - 1))) {
				// join and remove thread
				std::thread& worker = this->workers.at(n - 1);
				if(worker.joinable()) worker.join();
				this->workers.erase(this->workers.begin() + (n - 1));
				this->workersRunning.erase(this->workersRunning.begin() + (n - 1));
				n--;
			}
		}
	}

	// try to re-connect to database if it is offline
	if(this->offline) {
		try {
			this->database.checkConnection();
			this->offline = false;
		}
		catch(const Database::Exception &e) {}
	}

	return this->running;
}

// get status of server
const std::string& Server::getStatus() const {
	return this->status;
}

// get up-time of server in seconds
unsigned long Server::getUpTime() const {
	return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - this->uptimeStart).count();
}

// perform a server command
std::string Server::cmd(ConnectionPtr connection, const std::string& msgBody, bool& threadStartedTo) {
	Server::ServerCommandResponse response;

	if(this->offline) {
		// database offline: return error
		response = Server::ServerCommandResponse(true, "Database is offline.");
	}
	else {
		// check connection and get IP
		if(!connection) throw std::runtime_error("Server::cmd(): No connection specified");
		std::string ip(WebServer::getIP(connection));

		// parse JSON
		rapidjson::Document json;
		if(json.Parse(msgBody.c_str()).HasParseError())
			response = Server::ServerCommandResponse(true, "Could not parse JSON.");
		else if(!json.IsObject())
			response = Server::ServerCommandResponse(true, "Parsed JSON is not an object.");
		else {
			// get server command
			if(json.HasMember("cmd")) {
				if(json["cmd"].IsString()) {
					try {
						// handle server commands
						std::string command = json["cmd"].GetString();
						if(command == "kill") response = this->cmdKill(json, ip);
						else if(command == "allow") response = this->cmdAllow(json, ip);
						else if(command == "disallow") response = this->cmdDisallow(json, ip);

						else if(command == "log") response = this->cmdLog(json);
						else if(command == "clearlogs") response = this->cmdClearLog(json, ip);

						else if(command == "startcrawler") response = this->cmdStartCrawler(json, ip);
						else if(command == "pausecrawler") response = this->cmdPauseCrawler(json, ip);
						else if(command == "unpausecrawler") response = this->cmdUnpauseCrawler(json, ip);
						else if(command == "stopcrawler") response = this->cmdStopCrawler(json, ip);

						else if(command == "startparser") response = this->cmdStartParser(json, ip);
						else if(command == "pauseparser") response = this->cmdPauseParser(json, ip);
						else if(command == "unpauseparser") response = this->cmdUnpauseParser(json, ip);
						else if(command == "stopparser") response = this->cmdStopParser(json, ip);
						else if(command == "resetparsingstatus") response = this->cmdResetParsingStatus(json);

						//TODO: server commands for extractor
						else if(command == "resetextractingstatus") response = this->cmdResetExtractingStatus(json);

						else if(command == "startanalyzer") response = this->cmdStartAnalyzer(json, ip);
						else if(command == "pauseanalyzer") response = this->cmdPauseAnalyzer(json, ip);
						else if(command == "unpauseanalyzer") response = this->cmdUnpauseAnalyzer(json, ip);
						else if(command == "stopanalyzer") response = this->cmdStopAnalyzer(json, ip);
						else if(command == "resetanalyzingstatus") response = this->cmdResetAnalyzingStatus(json);

						else if(command == "addwebsite") response = this->cmdAddWebsite(json);
						else if(command == "updatewebsite") response = this->cmdUpdateWebsite(json);
						else if(command == "deletewebsite") response = this->cmdDeleteWebsite(json, ip);
						else if(command == "duplicatewebsite") response = this->cmdDuplicateWebsite(json);

						else if(command == "addurllist") response = this->cmdAddUrlList(json);
						else if(command == "updateurllist") response = this->cmdUpdateUrlList(json);
						else if(command == "deleteurllist") response = this->cmdDeleteUrlList(json, ip);

						else if(command == "addquery") response = this->cmdAddQuery(json);
						else if(command == "updatequery") response = this->cmdUpdateQuery(json);
						else if(command == "deletequery") response = this->cmdDeleteQuery(json);
						else if(command == "duplicatequery") response = this->cmdDuplicateQuery(json);
						else if(command == "testquery") {
							// create a worker thread for testing query (so large queries do not block the server)
							{
								std::lock_guard<std::mutex> workersLocked(this->workersLock);
								this->workersRunning.push_back(true);
								this->workers.push_back(std::thread(
										&Server::cmdTestQuery, this, connection, this->workers.size(), msgBody));
							}
							threadStartedTo = true;
						}

						else if(command == "addconfig") response = this->cmdAddConfig(json);
						else if(command == "updateconfig") response = this->cmdUpdateConfig(json);
						else if(command == "deleteconfig") response = this->cmdDeleteConfig(json);
						else if(command == "duplicateconfig") response = this->cmdDuplicateConfig(json);

						else if(!command.empty())
							// unknown command: debug the command and its arguments
							response = Server::ServerCommandResponse(true, "Unknown command \'" + command + "\'.");

						else response = Server::ServerCommandResponse(true, "Empty command.");
					}
					catch(std::exception &e) {
						// exceptions caused by server commands should not kill the server
						//  (and are attributed to the frontend)
						this->database.log("frontend", e.what());
						response = response = Server::ServerCommandResponse(true, e.what());
					}
				}
				else response = Server::ServerCommandResponse(true, "Invalid command: Name is not a string.");
			}
			else response = Server::ServerCommandResponse(true, "No command specified.");
		}
	}

	// return the reply
	rapidjson::StringBuffer replyBuffer;
	rapidjson::Writer<rapidjson::StringBuffer> reply(replyBuffer);
	reply.StartObject();
	if(response.fail) {
		reply.Key("fail");
		reply.Bool(true);
		reply.Key("debug");
		reply.String(msgBody.c_str());
	}
	else if(response.confirm) {
		reply.Key("confirm");
		reply.Bool(true);
	}
	else if(response.id) {
		reply.Key("id");
		reply.Uint64(response.id);
	}
	reply.Key("text");
	reply.String(response.text.c_str(), response.text.size());
	reply.EndObject();
	return replyBuffer.GetString();
}

// set status of server
void Server::setStatus(const std::string& statusMsg) {
	this->status = statusMsg;
}

// handle accept event
void Server::onAccept(ConnectionPtr connection) {
	// check connection and get IP
	if(!connection) throw std::runtime_error("Server::onAccept(): No connection specified");
	std::string ip(WebServer::getIP(connection));

	// check authorization
	if(this->allowed != "*") {
		if(this->allowed.find(ip) == std::string::npos) {
			this->webServer.close(connection);
			if(this->offline) std::cout << std::endl << "server rejected client " + ip + "." << std::flush;
			else {
				try {
					this->database.log("rejected client " + ip + ".");
				}
				catch(const Database::Exception& e) {
					// try to re-connect once on database exception
					try {
						this->database.checkConnection();
						this->database.log("re-connected to database after error: " + e.whatStr());
						this->database.log("rejected client " + ip + ".");
					}
					catch(const Database::Exception& e) {
						std::cout << std::endl << "server rejected client " + ip + "." << std::flush;
						this->offline = true;
					}
				}
			}
		}
		else {
			if(this->offline) std::cout << std::endl << "server accepted client " + ip + "." << std::flush;
			else try {
				this->database.log("accepted client " + ip + ".");
			}
			catch(const Database::Exception& e) {
				// try to re-connect once on database exception
				try {
					this->database.checkConnection();
					this->database.log("re-connected to database after error: " + e.whatStr());
					this->database.log("accepted client " + ip + ".");
				}
				catch(const Database::Exception& e) {
					std::cout << std::endl << "server rejected client " + ip + "." << std::flush;
					this->offline = true;
				}
			}
		}
	}
}

// handle request event
void Server::onRequest(ConnectionPtr connection, const std::string& method,
		const std::string& body) {
	// check connection and get IP
	if(!connection) throw std::runtime_error("Server::onRequest(): No connection specified");
	std::string ip(WebServer::getIP(connection));

	// check authorization
	if(this->allowed != "*") {
		if(this->allowed.find(ip) == std::string::npos) {
			this->database.log("Client " + ip + " rejected.");
			this->webServer.close(connection);
		}
	}

#ifdef MAIN_SERVER_DEBUG_HTTP_REQUEST
	// debug HTTP request
	std::cout << std::endl << message << std::endl;
#endif

	// check for GET request
	if(method == "GET") this->webServer.send(connection, 200, "text/plain", this->getStatus());
	// check for POST request
	else if(method == "POST") {
		// parse JSON
		bool threadStarted = false;
		std::string reply = this->cmd(connection, body, threadStarted);

		// send reply
		if(!threadStarted) this->webServer.send(connection, 200, "application/json", reply);
	}
	else if(method == "OPTIONS") {
		this->webServer.send(connection, 200, "", "");
	}
}

// static helper function: get algorithm ID from configuration JSON
unsigned int Server::getAlgoFromConfig(const rapidjson::Document& json) {
	unsigned int result = 0;

	// go through all array items i.e. configuration entries
	for(auto i = json.Begin(); i != json.End(); i++) {
		bool algoItem = false;

		if(i->IsObject()) {
			// go through all item properties
			for(auto j = i->MemberBegin(); j != i->MemberEnd(); ++j) {
				if(j->name.IsString()) {
					std::string itemName = j->name.GetString();
					if(itemName == "name") {
						if(j->value.IsString()) {
							if(std::string(j->value.GetString()) == "_algo") {
								algoItem = true;
								if(result) break;
							}
							else break;
						}
					}
					else if(itemName == "value") {
						if(j->value.IsUint()) {
							result = j->value.GetUint();
							if(algoItem) break;
						}
					}
				}
			}
			if(algoItem) break;
			else result = 0;
		}
	}

	return result;
}

// server command kill: kill the server
Server::ServerCommandResponse Server::cmdKill(const rapidjson::Document& json,
		const std::string& ip) {
	// kill needs to be confirmed
	if(json.HasMember("confirmed")) {
		// kill server
		this->running = false;

		// kill is a logged command
		this->database.log("killed by " + ip + ".");

		// send bye message
		return Server::ServerCommandResponse("Bye bye.");
	}

	return Server::ServerCommandResponse(false, true, "Are you sure to kill the server?");
}

// server command allow(ip): allow acces for the specified IP(s)
Server::ServerCommandResponse Server::cmdAllow(const rapidjson::Document& json,
		const std::string& ip) {
	// get argument
	if(!json.HasMember("ip"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'ip\' is missing).");
	if(!json["ip"].IsString())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'ip\' is not a string).");
	std::string toAllow = json["ip"].GetString();

	// allow needs to be confirmed
	if(!json.HasMember("confirmed")) {
		return Server::ServerCommandResponse(false, true, "Do you really want to allow " + toAllow + " access to the server?");
	}

	// add IP(s)
	this->allowed += "," + toAllow;

	// allow is a logged command
	this->database.log(toAllow + " allowed by " + ip + ".");
	return Server::ServerCommandResponse("Allowed IPs: " + this->allowed + ".");
}

// server command disallow: revoke access from all except the initial IP(s) specified by the configuration file
Server::ServerCommandResponse Server::cmdDisallow(const rapidjson::Document& json,
		const std::string& ip) {
	// reset alled IP(s)
	this->allowed = this->settings.allowedClients;

	// disallow is a logged command
	this->database.log("Allowed IPs reset by " + ip + ".");

	return Server::ServerCommandResponse("Allowed IP(s): " + this->allowed + ".");
}

// server command log(entry): write a log entry by the frontend into the database
Server::ServerCommandResponse Server::cmdLog(const rapidjson::Document& json) {
	// get argument
	if(!json.HasMember("entry"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'entry\' is missing).");
	if(!json["entry"].IsString())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'entry\' is not a string).");
	std::string entry = json["entry"].GetString();

	// write log entry
	this->database.log("frontend", entry);
	return Server::ServerCommandResponse("Wrote log entry: " + entry);
}

// server command clearlog([module]):	remove all log entries or the log entries of a specific module
// 										(or all log entries when module is empty/not given)
Server::ServerCommandResponse Server::cmdClearLog(const rapidjson::Document& json,
		const std::string& ip) {
	// check whether the clearing of logs is allowed
	if(!(this->settings.logsDeletable))return Server::ServerCommandResponse(true, "Not allowed.");

	// get argument
	std::string module;
	if(json.HasMember("module") && json["module"].IsString()) module = json["module"].GetString();

	// clearlog needs to be confirmed
	if(!json.HasMember("confirmed")) {
		std::ostringstream replyStrStr;
		replyStrStr.imbue(std::locale(""));
		replyStrStr << "Are you sure to delete " << this->database.getNumberOfLogEntries(module) << " log entries?";
		return Server::ServerCommandResponse(false, true, replyStrStr.str());
	}

	this->database.clearLogs(module);

	// clearlog is a logged command
	if(!module.empty()) {
		this->database.log("Logs of " + module + " cleared by " + ip + ".");
		return Server::ServerCommandResponse("Logs of " + module + " cleared.");
	}

	this->database.log("All logs cleared by " + ip + ".");
	return Server::ServerCommandResponse("All logs cleared.");
}

// server command startcrawler(website, urllist, config): start a crawler using the specified website, URL list and configuration
Server::ServerCommandResponse Server::cmdStartCrawler(const rapidjson::Document& json,
		const std::string& ip) {
	// get arguments
	Struct::ThreadOptions options;
	if(!json.HasMember("website"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'website\' is missing).");
	if(!json["website"].IsUint64())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'website\' is not a valid number).");
	options.website = json["website"].GetUint64();

	if(!json.HasMember("urllist"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'urllist\' is missing).");
	if(!json["urllist"].IsUint64())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'urllist\' is not a valid number).");
	options.urlList = json["urllist"].GetUint64();

	if(!json.HasMember("config"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'config\' is missing).");
	if(!json["config"].IsUint64())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'config\' is not a valid number).");
	options.config = json["config"].GetUint64();

	// check arguments
	if(!(this->database.isWebsite(options.website))) {
		std::ostringstream errStrStr;
		errStrStr << "Website #" << options.website << " not found.";
		return Server::ServerCommandResponse(true, errStrStr.str());
	}
	if(!(this->database.isUrlList(options.website, options.urlList))) {
		std::ostringstream errStrStr;
		errStrStr << "URL list #" << options.urlList << " for website #" << options.website << " not found.";
		return Server::ServerCommandResponse(true, errStrStr.str());
	}
	if(!(this->database.isConfiguration(options.website, options.config))) {
		std::ostringstream errStrStr;
		errStrStr << "Configuration #" << options.config << " for website #" << options.website << " not found.";
		return Server::ServerCommandResponse(true, errStrStr.str());
	}

	// create and start crawler
	this->crawlers.push_back(std::make_unique<crawlservpp::Module::Crawler::Thread>(this->database, options));
	this->crawlers.back()->crawlservpp::Module::Thread::start();
	unsigned long id = this->crawlers.back()->crawlservpp::Module::Thread::getId();

	// startcrawler is a logged command
	std::ostringstream logStrStr;
	logStrStr << "crawler #" << id << " started by " << ip << ".";
	this->database.log(logStrStr.str());

	return Server::ServerCommandResponse("Crawler has been started.");
}

// server command pausecrawler(id): pause a crawler by its ID
Server::ServerCommandResponse Server::cmdPauseCrawler(const rapidjson::Document& json,
		const std::string& ip) {
	// get argument
	if(!json.HasMember("id"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'id\' is missing).");
	if(!json["id"].IsUint64())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'id\' is not a valid number).");
	unsigned long id = json["id"].GetUint64();

	// find crawler
	auto i = std::find_if(this->crawlers.begin(), this->crawlers.end(),
			[&id](std::unique_ptr<crawlservpp::Module::Crawler::Thread>& p) {
		return p->crawlservpp::Module::Thread::getId() == id;
	});
	if(i == this->crawlers.end()) {
		std::ostringstream errStrStr;
		errStrStr << "Could not find crawler #" << id << ".";
		return Server::ServerCommandResponse(true, errStrStr.str());
	}

	// pause crawler
	(*i)->crawlservpp::Module::Thread::pause();

	// pausecrawler is a logged command
	std::ostringstream logStrStr;
	logStrStr << "crawler #" << id << "] paused by " << ip << ".";
	this->database.log(logStrStr.str());

	return Server::ServerCommandResponse("Crawler is pausing.");
}

// server command unpausecrawler(id): unpause a crawler by its ID
Server::ServerCommandResponse Server::cmdUnpauseCrawler(const rapidjson::Document& json,
		const std::string& ip) {
	// get argument
	if(!json.HasMember("id"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'id\' is missing).");
	if(!json["id"].IsUint64())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'id\' is not a valid number).");
	unsigned long id = json["id"].GetUint64();

	// find crawler
	auto i = std::find_if(this->crawlers.begin(), this->crawlers.end(),
			[&id](std::unique_ptr<crawlservpp::Module::Crawler::Thread>& p) {
		return p->crawlservpp::Module::Thread::getId() == id;
	});
	if(i == this->crawlers.end()) {
		std::ostringstream errStrStr;
		errStrStr << "Could not find crawler #" << id << ".";
		return Server::ServerCommandResponse(true, errStrStr.str());
	}

	// unpause crawler
	(*i)->crawlservpp::Module::Thread::unpause();

	// unpausecrawler is a logged command
	std::ostringstream logStrStr;
	logStrStr << "crawler #" << id << " unpaused by " << ip << ".";
	this->database.log(logStrStr.str());

	return Server::ServerCommandResponse("Crawler is unpausing.");
}

// server command stopcrawler(id): stop a crawler by its ID
Server::ServerCommandResponse Server::cmdStopCrawler(const rapidjson::Document& json,
		const std::string& ip) {
	// get argument
	if(!json.HasMember("id"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'id\' is missing).");
	if(!json["id"].IsUint64())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'id\' is not a valid number).");
	unsigned long id = json["id"].GetUint64();

	// find crawler
	auto i = std::find_if(this->crawlers.begin(), this->crawlers.end(),
			[&id](std::unique_ptr<crawlservpp::Module::Crawler::Thread>& p) {
		return p->crawlservpp::Module::Thread::getId() == id;
	});
	if(i == this->crawlers.end()) {
		std::ostringstream errStrStr;
		errStrStr << "Could not find crawler #" << id << ".";
		return Server::ServerCommandResponse(true, errStrStr.str());
	}

	// stop and delete crawler
	(*i)->crawlservpp::Module::Thread::stop();
	this->crawlers.erase(i);

	// stopcrawler is a logged command
	std::ostringstream logStrStr;
	logStrStr << "crawler #" << id << " stopped by " << ip << ".";
	this->database.log(logStrStr.str());

	return Server::ServerCommandResponse("Crawler stopped.");
}

// server command startparser(website, urllist, config): start a parser using the specified website, URL list and configuration
Server::ServerCommandResponse Server::cmdStartParser(const rapidjson::Document& json,
		const std::string& ip) {
	// get arguments
	Struct::ThreadOptions options;
	if(!json.HasMember("website"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'website\' is missing).");
	if(!json["website"].IsUint64())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'website\' is not a valid number).");
	options.website = json["website"].GetUint64();

	if(!json.HasMember("urllist"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'urllist\' is missing).");
	if(!json["urllist"].IsUint64())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'urllist\' is not a valid number).");
	options.urlList = json["urllist"].GetUint64();

	if(!json.HasMember("config"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'config\' is missing).");
	if(!json["config"].IsUint64())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'config\' is not a valid number).");
	options.config = json["config"].GetUint64();

	// check arguments
	if(!(this->database.isWebsite(options.website))) {
		std::ostringstream errStrStr;
		errStrStr << "Website #" << options.website << " not found.";
		return Server::ServerCommandResponse(true, errStrStr.str());
	}
	if(!(this->database.isUrlList(options.website, options.urlList))) {
		std::ostringstream errStrStr;
		errStrStr << "URL list #" << options.urlList << " for website #" << options.website << " not found.";
		return Server::ServerCommandResponse(true, errStrStr.str());
	}
	if(!(this->database.isConfiguration(options.website, options.config))) {
		std::ostringstream errStrStr;
		errStrStr << "Configuration #" << options.config << " for website #" << options.website << " not found.";
		return Server::ServerCommandResponse(true, errStrStr.str());
	}

	// create and start parser
	this->parsers.push_back(std::make_unique<crawlservpp::Module::Parser::Thread>(this->database, options));
	this->parsers.back()->crawlservpp::Module::Thread::start();
	unsigned long id = this->parsers.back()->crawlservpp::Module::Thread::getId();

	// startparser is a logged command
	std::ostringstream logStrStr;
	logStrStr << "parser #" << id << " started by " << ip << ".";
	this->database.log(logStrStr.str());

	return Server::ServerCommandResponse("Parser has been started.");
}

// server command pauseparser(id): pause a parser by its ID
Server::ServerCommandResponse Server::cmdPauseParser(const rapidjson::Document& json,
		const std::string& ip) {
	// get argument
	if(!json.HasMember("id"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'id\' is missing).");
	if(!json["id"].IsUint64())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'id\' is not a valid number).");
	unsigned long id = json["id"].GetUint64();

	// find parser
	auto i = std::find_if(this->parsers.begin(), this->parsers.end(),
			[&id](std::unique_ptr<crawlservpp::Module::Parser::Thread>& p) {
		return p->crawlservpp::Module::Thread::getId() == id;
	});
	if(i == this->parsers.end()) {
		std::ostringstream errStrStr;
		errStrStr << "Could not find parser #" << id << ".";
		return Server::ServerCommandResponse(true, errStrStr.str());
	}

	// pause parser
	(*i)->crawlservpp::Module::Thread::pause();

	// pauseparser is a logged command
	std::ostringstream logStrStr;
	logStrStr << "parser #" << id << " paused by " << ip << ".";
	this->database.log(logStrStr.str());

	return Server::ServerCommandResponse("Parser is pausing.");
}

// server command unpauseparser(id): unpause a parser by its ID
Server::ServerCommandResponse Server::cmdUnpauseParser(const rapidjson::Document& json,
		const std::string& ip) {
	// get argument
	if(!json.HasMember("id"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'id\' is missing).");
	if(!json["id"].IsUint64())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'id\' is not a valid number).");
	unsigned long id = json["id"].GetUint64();

	// find parser
	auto i = std::find_if(this->parsers.begin(), this->parsers.end(),
			[&id](std::unique_ptr<crawlservpp::Module::Parser::Thread>& p) {
		return p->crawlservpp::Module::Thread::getId() == id;
	});
	if(i == this->parsers.end()) {
		std::ostringstream errStrStr;
		errStrStr << "Could not find parser #" << id << ".";
		return Server::ServerCommandResponse(true, errStrStr.str());
	}

	// unpause parser
	(*i)->crawlservpp::Module::Thread::unpause();

	// unpauseparser is a logged command
	std::ostringstream logStrStr;
	logStrStr << "parser #" << id << " unpaused by " << ip << ".";
	this->database.log(logStrStr.str());

	return Server::ServerCommandResponse("Parser is unpausing.");
}

// server command stopparser(id): stop a parser by its ID
Server::ServerCommandResponse Server::cmdStopParser(const rapidjson::Document& json,
		const std::string& ip) {
	// get argument
	if(!json.HasMember("id"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'id\' is missing).");
	if(!json["id"].IsUint64())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'id\' is not a valid number).");
	unsigned long id = json["id"].GetUint64();

	// find parser
	auto i = std::find_if(this->parsers.begin(), this->parsers.end(),
			[&id](std::unique_ptr<crawlservpp::Module::Parser::Thread>& p) {
		return p->crawlservpp::Module::Thread::getId() == id;
	});
	if(i == this->parsers.end()) {
		std::ostringstream errStrStr;
		errStrStr << "Could not find parser #" << id << ".";
		return Server::ServerCommandResponse(true, errStrStr.str());
	}

	// stop and delete parser
	(*i)->crawlservpp::Module::Thread::stop();
	this->parsers.erase(i);

	// stopparser is a logged command
	std::ostringstream logStrStr;
	logStrStr << "parser #" << id << " stopped by " << ip << ".";
	this->database.log(logStrStr.str());

	return Server::ServerCommandResponse("Parser stopped.");
}

// server command resetparsingstatus(urllist): reset the parsing status of a ID-specificed URL list
Server::ServerCommandResponse Server::cmdResetParsingStatus(const rapidjson::Document& json) {
	// get argument
	if(!json.HasMember("urllist"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'urllist\' is missing).");
	if(!json["urllist"].IsUint64())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'urllist\' is not a valid number).");
	unsigned long listId = json["urllist"].GetUint64();

	// resetparsingstatus needs to be confirmed
	if(!json.HasMember("confirmed"))
		return Server::ServerCommandResponse(false, true,
				"Are you sure that you want to reset the parsing status of this URL list?");

	// reset parsing status
	this->database.resetParsingStatus(listId);
	return Server::ServerCommandResponse("Parsing status reset.");
}

// server command resetextractingstatus(urllist): reset the parsing status of a ID-specificed URL list
Server::ServerCommandResponse Server::cmdResetExtractingStatus(const rapidjson::Document& json) {
	// get argument
	if(!json.HasMember("urllist"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'urllist\' is missing).");
	if(!json["urllist"].IsUint64())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'urllist\' is not a valid number).");
	unsigned long listId = json["urllist"].GetUint64();

	// resetextractingstatus needs to be confirmed
	if(!json.HasMember("confirmed"))
			return Server::ServerCommandResponse(false, true,
					"Are you sure that you want to reset the extracting status of this URL list?");

	// reset extracting status
	this->database.resetExtractingStatus(listId);
	return Server::ServerCommandResponse("Extracting status reset.");
}

// server command startanalyzer(website, urllist, config): start an analyzer using the specified website, URL list, module
//	and configuration
Server::ServerCommandResponse Server::cmdStartAnalyzer(const rapidjson::Document& json,
		const std::string& ip) {
	// get arguments
	Struct::ThreadOptions options;
	if(!json.HasMember("website"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'website\' is missing).");
	if(!json["website"].IsUint64())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'website\' is not a valid number).");
	options.website = json["website"].GetUint64();

	if(!json.HasMember("urllist"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'urllist\' is missing).");
	if(!json["urllist"].IsUint64())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'urllist\' is not a valid number).");
	options.urlList = json["urllist"].GetUint64();

	if(!json.HasMember("config"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'config\' is missing).");
	if(!json["config"].IsUint64())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'config\' is not a valid number).");
	options.config = json["config"].GetUint64();

	// check arguments
	if(!(this->database.isWebsite(options.website))) {
		std::ostringstream errStrStr;
		errStrStr << "Website #" << options.website << " not found.";
		return Server::ServerCommandResponse(true, errStrStr.str());
	}
	if(!(this->database.isUrlList(options.website, options.urlList))) {
		std::ostringstream errStrStr;
		errStrStr << "URL list #" << options.urlList << " for website #" << options.website << " not found.";
		return Server::ServerCommandResponse(true, errStrStr.str());
	}
	if(!(this->database.isConfiguration(options.website, options.config))) {
		std::ostringstream errStrStr;
		errStrStr << "Configuration #" << options.config << " for website #" << options.website << " not found.";
		return Server::ServerCommandResponse(true, errStrStr.str());
	}

	// get configuration
	std::string config = this->database.getConfiguration(options.config);

	// check configuration JSON
	rapidjson::Document configJson;
	if(configJson.Parse(config.c_str()).HasParseError())
		return Server::ServerCommandResponse(true, "Could not parse analyzing configuration.");
	else if(!configJson.IsArray())
		return Server::ServerCommandResponse(true, "Parsed analyzing configuration is not an array.");

	// get algorithm from configuration
	unsigned int algo = Server::getAlgoFromConfig(configJson);
	if(!algo) return Server::ServerCommandResponse(true, "Analyzing configuration does not include an algorithm.");

	// create and start analyzer
	switch(algo) {
	case crawlservpp::Module::Analyzer::Algo::Enum::markovText:
		this->analyzers.push_back(std::make_unique<crawlservpp::Module::Analyzer::Algo::MarkovText>(this->database, options));
		break;
	case crawlservpp::Module::Analyzer::Algo::Enum::markovTweet:
		this->analyzers.push_back(std::make_unique<crawlservpp::Module::Analyzer::Algo::MarkovTweet>(this->database, options));
		break;
	default:
		std::ostringstream errStrStr;
		errStrStr << "Algorithm #" << algo << " not found.";
		return Server::ServerCommandResponse(true, errStrStr.str());
	}
	this->analyzers.back()->crawlservpp::Module::Thread::start();
	unsigned long id = this->analyzers.back()->crawlservpp::Module::Thread::getId();

	// startanalyzer is a logged command
	std::ostringstream logStrStr;
	logStrStr << "analyzer #" << id << " started by " << ip << ".";
	this->database.log(logStrStr.str());

	return Server::ServerCommandResponse("Analyzer has been started.");
}

// server command pauseparser(id): pause a parser by its ID
Server::ServerCommandResponse Server::cmdPauseAnalyzer(const rapidjson::Document& json,
		const std::string& ip) {
	// get argument
	if(!json.HasMember("id"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'id\' is missing).");
	if(!json["id"].IsUint64())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'id\' is not a valid number).");
	unsigned long id = json["id"].GetUint64();

	// find analyzer
	auto i = std::find_if(this->analyzers.begin(), this->analyzers.end(),
			[&id](std::unique_ptr<crawlservpp::Module::Analyzer::Thread>& p) {
		return p->crawlservpp::Module::Thread::getId() == id;
	});
	if(i == this->analyzers.end()) {
		std::ostringstream errStrStr;
		errStrStr << "Could not find analyzer #" << id << ".";
		return Server::ServerCommandResponse(true, errStrStr.str());
	}

	// pause analyzer
	if((*i)->crawlservpp::Module::Thread::pause()) {

		// pauseanalyzer is a logged command
		std::ostringstream logStrStr;
		logStrStr << "analyzer #" << id << " paused by " << ip << ".";
		this->database.log(logStrStr.str());

		return Server::ServerCommandResponse("Analyzer is pausing.");
	}

	// analyzer is not pausable
	return Server::ServerCommandResponse(true, "This algorithm cannot be paused at this moment.");
}

// server command unpauseanalyzer(id): unpause a parser by its ID
Server::ServerCommandResponse Server::cmdUnpauseAnalyzer(const rapidjson::Document& json,
		const std::string& ip) {
	// get argument
	if(!json.HasMember("id"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'id\' is missing).");
	if(!json["id"].IsUint64())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'id\' is not a valid number).");
	unsigned long id = json["id"].GetUint64();

	// find analyzer
	auto i = std::find_if(this->analyzers.begin(), this->analyzers.end(),
			[&id](std::unique_ptr<crawlservpp::Module::Analyzer::Thread>& p) {
		return p->crawlservpp::Module::Thread::getId() == id;
	});
	if(i == this->analyzers.end()) {
		std::ostringstream errStrStr;
		errStrStr << "Could not find analyzer #" << id << ".";
		return Server::ServerCommandResponse(true, errStrStr.str());
	}

	// unpause analyzer
	(*i)->crawlservpp::Module::Thread::unpause();

	// unpauseanalyzer is a logged command
	std::ostringstream logStrStr;
	logStrStr << "analyzer #" << id << " unpaused by " << ip << ".";
	this->database.log(logStrStr.str());

	return Server::ServerCommandResponse("Analyzer is unpausing.");
}

// server command stopanalyzer(id): stop a parser by its ID
Server::ServerCommandResponse Server::cmdStopAnalyzer(const rapidjson::Document& json,
		const std::string& ip) {
	// get argument
	if(!json.HasMember("id"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'id\' is missing).");
	if(!json["id"].IsUint64())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'id\' is not a valid number).");
	unsigned long id = json["id"].GetUint64();

	// find analyzer
	auto i = std::find_if(this->analyzers.begin(), this->analyzers.end(),
			[&id](std::unique_ptr<crawlservpp::Module::Analyzer::Thread>& p) {
		return p->crawlservpp::Module::Thread::getId() == id;
	});
	if(i == this->analyzers.end()) {
		std::ostringstream errStrStr;
		errStrStr << "Could not find analyzer #" << id << ".";
		return Server::ServerCommandResponse(true, errStrStr.str());
	}

	// stop and delete analyzer
	(*i)->crawlservpp::Module::Thread::stop();
	this->analyzers.erase(i);

	// stopanalyzer is a logged command
	std::ostringstream logStrStr;
	logStrStr << "analyzer #" << id << " stopped by " << ip << ".";
	this->database.log(logStrStr.str());

	return Server::ServerCommandResponse("Analyzer stopped.");
}

// server command resetanalyzingstatus(urllist): reset the parsing status of a ID-specificed URL list
Server::ServerCommandResponse Server::cmdResetAnalyzingStatus(const rapidjson::Document& json) {
	// get argument
	if(!json.HasMember("urllist"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'urllist\' is missing).");
	if(!json["urllist"].IsUint64())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'urllist\' is not a valid number).");
	unsigned long listId = json["urllist"].GetUint64();

	// resetanalyzingstatus needs to be confirmed
	if(!json.HasMember("confirmed"))
			return Server::ServerCommandResponse(false, true,
					"Are you sure that you want to reset the analyzing status of this URL list?");

	// reset analyzing status
	this->database.resetAnalyzingStatus(listId);
	return Server::ServerCommandResponse("Analyzing status reset.");
}

// server command addwebsite(domain, namespace, name): add a website
Server::ServerCommandResponse Server::cmdAddWebsite(const rapidjson::Document& json) {
	crawlservpp::Struct::WebsiteProperties properties;

	// get arguments
	if(!json.HasMember("domain"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'domain\' is missing).");
	if(!json["domain"].IsString())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'domain\' is not a string).");
	properties.domain = json["domain"].GetString();

	if(!json.HasMember("namespace"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'namespace\' is missing).");
	if(!json["namespace"].IsString())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'namespace\' is not a string).");
	properties.nameSpace = json["namespace"].GetString();

	if(!json.HasMember("name"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'name\' is missing).");
	if(!json["name"].IsString())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'name\' is not a string).");
	properties.name = json["name"].GetString();

	// check namespace
	if(properties.nameSpace.length() < 4)
		return Server::ServerCommandResponse(true, "Website namespace has to be at least 4 characters long.");
	if(!(crawlservpp::Helper::Strings::checkSQLName(properties.nameSpace)))
		return Server::ServerCommandResponse(true, "Invalid character(s) in website namespace.");

	// check name
	if(properties.name.empty()) return Server::ServerCommandResponse(true, "Name is empty.");

	// correct and check domain name (remove protocol from start and slash from the end)
	while(properties.domain.length() > 6 && properties.domain.substr(0, 7) == "http://")
		properties.domain = properties.domain.substr(7);
	while(properties.domain.length() > 7 && properties.domain.substr(0, 8) == "https://")
		properties.domain = properties.domain.substr(8);
	while(!properties.domain.empty() && properties.domain.back() == '/')
		properties.domain.pop_back();
	if(properties.domain.empty())
		return Server::ServerCommandResponse(true, "Domain is empty.");

	// add website to database
	unsigned long id = this->database.addWebsite(properties);
	if(!id) return Server::ServerCommandResponse(true, "Could not add website to database.");

	return Server::ServerCommandResponse("Website added.", id);
}

// server command updatewebsite(id, domain, namespace, name): edit a website
Server::ServerCommandResponse Server::cmdUpdateWebsite(const rapidjson::Document& json) {
	crawlservpp::Struct::WebsiteProperties properties;

	// get arguments
	if(!json.HasMember("id"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'id\' is missing).");
	if(!json["id"].IsUint64())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'id\' is not a valid number).");
	unsigned long id = json["id"].GetUint64();

	if(!json.HasMember("domain"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'domain\' is missing).");
	if(!json["domain"].IsString())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'domain\' is not a string).");
	properties.domain = json["domain"].GetString();

	if(!json.HasMember("namespace"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'namespace\' is missing).");
	if(!json["namespace"].IsString())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'namespace\' is not a string).");
	properties.nameSpace = json["namespace"].GetString();

	if(!json.HasMember("name"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'name\' is missing).");
	if(!json["name"].IsString())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'name\' is not a string).");
	properties.name = json["name"].GetString();

	// check name
	if(properties.name.empty()) return Server::ServerCommandResponse(true, "Name is empty.");

	// check namespace name
	if(properties.nameSpace.length() < 4)
		return Server::ServerCommandResponse(true, "Website namespace has to be at least 4 characters long.");
	if(!(crawlservpp::Helper::Strings::checkSQLName(properties.nameSpace)))
		return Server::ServerCommandResponse(true, "Invalid character(s) in website namespace.");

	// correct and check domain name (remove protocol from start and slash from the end)
	while(properties.domain.length() > 6 && properties.domain.substr(0, 7) == "http://")
		properties.domain = properties.domain.substr(7);
	while(properties.domain.length() > 7 && properties.domain.substr(0, 8) == "https://")
		properties.domain = properties.domain.substr(8);
	while(!properties.domain.empty() && properties.domain.back() == '/')
		properties.domain.pop_back();
	if(properties.domain.empty())
		Server::ServerCommandResponse(true, "Domain is empty.");

	// check website
	if(!(this->database.isWebsite(id))) {
		std::ostringstream errStrStr;
		errStrStr << "Website #" << id << " not found.";
		return Server::ServerCommandResponse(true, errStrStr.str());
	}

	// check whether threads are using the website
	for(auto i = this->crawlers.begin(); i != this->crawlers.end(); ++i)
		if((*i)->getWebsite() == id)
			return Server::ServerCommandResponse(true, "Website cannot be changed while crawler is active.");
	for(auto i = this->parsers.begin(); i != this->parsers.end(); ++i)
		if((*i)->getWebsite() == id)
			return Server::ServerCommandResponse(true, "Website cannot be changed while parser is active.");
	/*for(auto i = this->extractors.begin(); i != this->extractors.end(); ++i)
		if((*i)->getWebsite() == id)
			return Server::ServerCommandResponse(true, "Website cannot be changed while extractor is active.");*/
	for(auto i = this->analyzers.begin(); i != this->analyzers.end(); ++i)
		if((*i)->getWebsite() == id)
			return Server::ServerCommandResponse(true, "Website cannot be changed while analyzer is active.");

	// update website in database
	this->database.updateWebsite(id, properties);
	return Server::ServerCommandResponse("Website updated.");
}

// server command deletewebsite(id): delete a website and all associated data from the database by its ID
Server::ServerCommandResponse Server::cmdDeleteWebsite(const rapidjson::Document& json,
		const std::string& ip) {
	// check whether the deletion of data is allowed
	if(!(this->settings.dataDeletable)) return Server::ServerCommandResponse(true, "Not allowed.");

	// get argument
	if(!json.HasMember("id")) return Server::ServerCommandResponse(true, "Invalid arguments (\'id\' is missing).");
	if(!json["id"].IsUint64()) return Server::ServerCommandResponse(true, "Invalid arguments (\'id\' is not a valid number).");
	unsigned long id = json["id"].GetUint64();

	// check website
	if(!(this->database.isWebsite(id))) {
		std::ostringstream errStrStr;
		errStrStr << "Website #" << id << " not found.";
		return Server::ServerCommandResponse(true, errStrStr.str());
	}

	// check whether threads are using the website
	for(auto i = this->crawlers.begin(); i != this->crawlers.end(); ++i)
		if((*i)->getWebsite() == id)
			return Server::ServerCommandResponse(true, "Website cannot be deleted while crawler is active.");
	for(auto i = this->parsers.begin(); i != this->parsers.end(); ++i)
		if((*i)->getWebsite() == id)
			return Server::ServerCommandResponse(true, "Website cannot be deleted while parser is active.");
	/*for(auto i = this->extractors.begin(); i != this->extractors.end(); ++i)
		if((*i)->getWebsite() == id)
			return Server::ServerCommandResponse(true, "Website cannot be deleted while extractor is active.");*/
	for(auto i = this->analyzers.begin(); i != this->analyzers.end(); ++i)
		if((*i)->getWebsite() == id)
			return Server::ServerCommandResponse(true, "Website cannot be deleted while analyzer is active.");

	// deletewebsite needs to be confirmed
	if(!json.HasMember("confirmed"))
		return Server::ServerCommandResponse(false, true, "Do you really want to delete this website?\n"
				"!!! All associated data will be lost !!!");

	// delete website from database
	this->database.deleteWebsite(id);

	// deletewebsite is a logged command
	std::ostringstream logStrStr;
	logStrStr << "Website #" << id << " deleted by " << ip << ".";
	this->database.log(logStrStr.str());
	return Server::ServerCommandResponse("Website deleted.");
}

// server command duplicatewebsite(id): Duplicate a website by its ID (no processed data will be duplicated)
Server::ServerCommandResponse Server::cmdDuplicateWebsite(const rapidjson::Document& json) {
	// get argument
	if(!json.HasMember("id"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'id\' is missing).");
	if(!json["id"].IsUint64())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'id\' is not a valid number).");
	unsigned long id = json["id"].GetUint64();

	// check website
	if(!(this->database.isWebsite(id))) {
		std::ostringstream errStrStr;
		errStrStr << "Website #" << id << " not found.";
		return Server::ServerCommandResponse(true, errStrStr.str());
	}

	// duplicate website configuration
	unsigned long newId = this->database.duplicateWebsite(id);
	if(!newId) return Server::ServerCommandResponse(true, "Could not add duplicate to database.");

	return Server::ServerCommandResponse("Website duplicated.", newId);
}

// server command addurllist(website, namespace, name): add a URL list to the ID-specified website
Server::ServerCommandResponse Server::cmdAddUrlList(const rapidjson::Document& json) {
	crawlservpp::Struct::UrlListProperties properties;

	// get arguments
	if(!json.HasMember("website"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'website\' is missing).");
	if(!json["website"].IsUint64())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'website\' is not a valid number).");
	unsigned long website = json["website"].GetUint64();

	if(!json.HasMember("namespace"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'namespace\' is missing).");
	if(!json["namespace"].IsString())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'namespace\' is not a string).");
	properties.nameSpace = json["namespace"].GetString();

	if(!json.HasMember("name"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'name\' is missing).");
	if(!json["name"].IsString())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'name\' is not a string).");
	properties.name = json["name"].GetString();

	// check namespace
	if(properties.nameSpace.length() < 4)
		return Server::ServerCommandResponse(true, "Namespace of URL list has to be at least 4 characters long.");
	if(!(crawlservpp::Helper::Strings::checkSQLName(properties.nameSpace)))
		return Server::ServerCommandResponse(true, "Invalid character(s) in namespace of URL list.");
	if(properties.nameSpace == "config")
		return Server::ServerCommandResponse(true, "Namespace of URL list cannot be \'config\'.");

	// check name
	if(properties.name.empty()) return Server::ServerCommandResponse(true, "Name is empty.");

	// check website
	if(!(this->database.isWebsite(website))) {
		std::ostringstream errStrStr;
		errStrStr << "Website #" << website << " not found.";
		return Server::ServerCommandResponse(true, errStrStr.str());
	}

	// add URL list to database
	unsigned long id = this->database.addUrlList(website, properties);
	if(!id) return Server::ServerCommandResponse(true, "Could not add URL list to database.");

	return Server::ServerCommandResponse("URL list added.", id);
}

// server command updateurllist(id, namespace, name): edit a URL list
Server::ServerCommandResponse Server::cmdUpdateUrlList(const rapidjson::Document& json) {
	crawlservpp::Struct::UrlListProperties properties;

	// get arguments
	if(!json.HasMember("id"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'id\' is missing).");
	if(!json["id"].IsUint64())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'id\' is not a valid number).");
	unsigned long id = json["id"].GetUint64();

	if(!json.HasMember("namespace"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'namespace\' is missing).");
	if(!json["namespace"].IsString())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'namespace\' is not a string).");
	properties.nameSpace = json["namespace"].GetString();

	if(!json.HasMember("name"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'name\' is missing).");
	if(!json["name"].IsString())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'name\' is not a string).");
	properties.name = json["name"].GetString();

	// check namespace
	if(properties.nameSpace.length() < 4)
		return Server::ServerCommandResponse(true, "Namespace of URL list has to be at least 4 characters long.");
	if(!(crawlservpp::Helper::Strings::checkSQLName(properties.nameSpace)))
		return Server::ServerCommandResponse(true, "Invalid character(s) in namespace of URL list.");;
	if(properties.nameSpace == "config")
		return Server::ServerCommandResponse(true, "Namespace of URL list cannot be \'config\'.");

	// check name
	if(properties.name.empty()) return Server::ServerCommandResponse(true, "Name is empty.");

	// check URL list
	if(!(this->database.isUrlList(id))) {
		std::ostringstream errStrStr;
		errStrStr << "URL list #" << id << " not found.";
		return Server::ServerCommandResponse(true, errStrStr.str());
	}

	// check whether threads are using the URL list
	for(auto i = this->crawlers.begin(); i != this->crawlers.end(); ++i)
		if((*i)->getUrlList() == id)
			return Server::ServerCommandResponse(true, "URL list cannot be changed while crawler is active.");
	for(auto i = this->parsers.begin(); i != this->parsers.end(); ++i)
		if((*i)->getUrlList() == id)
			return Server::ServerCommandResponse(true, "URL list cannot be changed while parser is active.");
	/*for(auto i = this->extractors.begin(); i != this->extractors.end(); ++i)
		if((*i)->getUrlList() == id)
			return Server::ServerCommandResponse(true, "URL list cannot be changed while extractor is active.");*/
	for(auto i = this->analyzers.begin(); i != this->analyzers.end(); ++i)
		if((*i)->getUrlList() == id)
			return Server::ServerCommandResponse(true, "URL list cannot be changed while analyzer is active.");

	// update URL list in database
	this->database.updateUrlList(id, properties);
	return Server::ServerCommandResponse("URL list updated.");
}

// server command deleteurllist(id): delete a URL list and all associated data from the database by its ID
Server::ServerCommandResponse Server::cmdDeleteUrlList(const rapidjson::Document& json,
		const std::string& ip) {
	// check whether the deletion of data is allowed
	if(!(this->settings.dataDeletable)) return Server::ServerCommandResponse(true, "Not allowed.");

	// get argument
	if(!json.HasMember("id"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'id\' is missing).");
	if(!json["id"].IsUint64())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'id\' is not a valid number).");
	unsigned long id = json["id"].GetUint64();

	// check URL list
	if(!(this->database.isUrlList(id))) {
		std::ostringstream errStrStr;
		errStrStr << "URL list #" << id << " not found.";
		Server::ServerCommandResponse(true, errStrStr.str());
	}

	// deleteurllist needs to be confirmed
	if(!json.HasMember("confirmed"))
		return Server::ServerCommandResponse(false, true, "Do you really want to delete this URL list?\n"
				"!!! All associated data will be lost !!!");

	// delete URL list from database
	this->database.deleteUrlList(id);

	// deleteurllist is a logged command
	std::ostringstream logStrStr;
	logStrStr << "URL list #" << id << " deleted by " << ip << ".";
	this->database.log(logStrStr.str());
	return Server::ServerCommandResponse("URL list deleted.");
}

// server command addquery(website, name, query, type, resultbool, resultsingle, resultmulti, textonly): add a query
Server::ServerCommandResponse Server::cmdAddQuery(const rapidjson::Document& json) {
	crawlservpp::Struct::QueryProperties properties;

	// get arguments
	if(!json.HasMember("website"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'website\' is missing).");
	if(!json["website"].IsUint64())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'website\' is not a valid number).");
	unsigned long website = json["website"].GetUint64();

	if(!json.HasMember("name"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'name\' is missing).");
	if(!json["name"].IsString())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'name\' is not a string).");
	properties.name = json["name"].GetString();

	if(!json.HasMember("query"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'query\' is missing).");
	if(!json["query"].IsString())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'query\' is not a string).");
	properties.text = json["query"].GetString();

	if(!json.HasMember("type"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'type\' is missing).");
	if(!json["type"].IsString())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'type\' is not a string).");
	properties.type = json["type"].GetString();

	if(!json.HasMember("resultbool"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'resultbool\' is missing).");
	if(!json["resultbool"].IsBool())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'resultbool\' is not a boolean).");
	properties.resultBool = json["resultbool"].GetBool();

	if(!json.HasMember("resultsingle"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'resultsingle\' is missing).");
	if(!json["resultsingle"].IsBool())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'resultsingle\' is not a boolean).");
	properties.resultSingle = json["resultsingle"].GetBool();

	if(!json.HasMember("resultmulti"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'resultmulti\' is missing).");
	if(!json["resultmulti"].IsBool())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'resultmulti\' is not a boolean).");
	properties.resultMulti = json["resultmulti"].GetBool();

	if(!json.HasMember("textonly"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'textonly\' is missing).");
	if(!json["textonly"].IsBool())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'textonly\' is not a boolean).");
	properties.textOnly = json["textonly"].GetBool();

	// check name
	if(properties.name.empty()) return Server::ServerCommandResponse(true, "Name is empty.");

	// check query text
	if(properties.text.empty()) return Server::ServerCommandResponse(true, "Query text is empty.");

	// check query type
	if(properties.type.empty()) return Server::ServerCommandResponse(true, "Query type is empty.");
	if(properties.type != "regex" && properties.type != "xpath")
		return Server::ServerCommandResponse(true, "Unknown query type: \'" + properties.type + "\'.");

	// check result type
	if(!properties.resultBool && !properties.resultSingle && !properties.resultMulti)
		return Server::ServerCommandResponse(true, "No result type selected.");

	// check website
	if(website && !(this->database.isWebsite(website))) {
		std::ostringstream errStrStr;
		errStrStr << "Website #" << website << " not found.";
		return Server::ServerCommandResponse(true, errStrStr.str());
	}

	// add query to database
	unsigned long id = this->database.addQuery(website, properties);
	if(!id) return Server::ServerCommandResponse("Could not add query to database.");

	return Server::ServerCommandResponse("Query added.", id);
}

// server command updatequery(id, name, query, type, resultbool, resultsingle, resultmulti, textonly): edit a query
Server::ServerCommandResponse Server::cmdUpdateQuery(const rapidjson::Document& json) {
	crawlservpp::Struct::QueryProperties properties;

	// get arguments
	if(!json.HasMember("id"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'id\' is missing).");
	if(!json["id"].IsUint64())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'id\' is not a valid number).");

	unsigned long id = json["id"].GetUint64();

	if(!json.HasMember("name"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'name\' is missing).");
	if(!json["name"].IsString())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'name\' is not a string).");
	properties.name = json["name"].GetString();

	if(!json.HasMember("query"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'query\' is missing).");
	if(!json["query"].IsString())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'query\' is not a string).");
	properties.text = json["query"].GetString();

	if(!json.HasMember("type"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'type\' is missing).");
	if(!json["type"].IsString())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'type\' is not a string).");
	properties.type = json["type"].GetString();

	if(!json.HasMember("resultbool"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'resultbool\' is missing).");
	if(!json["resultbool"].IsBool())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'resultbool\' is not a boolean).");
	properties.resultBool = json["resultbool"].GetBool();

	if(!json.HasMember("resultsingle"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'resultsingle\' is missing).");
	if(!json["resultsingle"].IsBool())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'resultsingle\' is not a boolean).");
	properties.resultSingle = json["resultsingle"].GetBool();

	if(!json.HasMember("resultmulti"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'resultmulti\' is missing).");
	if(!json["resultmulti"].IsBool())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'resultmulti\' is not a boolean).");
	properties.resultMulti = json["resultmulti"].GetBool();

	if(!json.HasMember("textonly"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'textonly\' is missing).");
	if(!json["textonly"].IsBool())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'textonly\' is not a boolean).");
	properties.textOnly = json["textonly"].GetBool();

	// check name
	if(properties.name.empty())
		return Server::ServerCommandResponse(true, "Name is empty.");

	// check query text
	if(properties.text.empty())
		return Server::ServerCommandResponse(true, "Query text is empty.");

	// check query type
	if(properties.type.empty())
		return Server::ServerCommandResponse(true, "Query type is empty.");
	if(properties.type != "regex" && properties.type != "xpath")
		return Server::ServerCommandResponse(true, "Unknown query type: \'" + properties.type + "\'.");

	// check result type
	if(!properties.resultBool && !properties.resultSingle && !properties.resultMulti)
		return Server::ServerCommandResponse(true, "No result type selected.");

	// check query
	if(!(this->database.isQuery(id))) {
		std::ostringstream errStrStr;
		errStrStr << "Query #" << id << " not found.";
		return Server::ServerCommandResponse(true, errStrStr.str());
	}

	// update query in database
	this->database.updateQuery(id, properties);

	return Server::ServerCommandResponse("Query updated.");
}

// server command deletequery(id): delete a query from the database by its ID
Server::ServerCommandResponse Server::cmdDeleteQuery(const rapidjson::Document& json) {
	// check whether the deletion of data is allowed
	if(!(this->settings.dataDeletable)) return Server::ServerCommandResponse(true, "Not allowed.");

	// get argument
	if(!json.HasMember("id"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'id\' is missing).");
	if(!json["id"].IsUint64())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'id\' is not a valid number).");
	unsigned long id = json["id"].GetUint64();

	// check query
	if(!(this->database.isQuery(id))) {
		std::ostringstream errStrStr;
		errStrStr << "Query #" << id << " not found.";
		return Server::ServerCommandResponse(true, errStrStr.str());
	}

	if(!json.HasMember("confirmed"))
		return Server::ServerCommandResponse(false, true, "Do you really want to delete this query?");

	// delete URL list from database
	this->database.deleteQuery(id);

	return Server::ServerCommandResponse("Query deleted.");
}

// server command duplicatequery(id): Duplicate a query by its ID
Server::ServerCommandResponse Server::cmdDuplicateQuery(const rapidjson::Document& json) {
	// get argument
	if(!json.HasMember("id"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'id\' is missing).");
	if(!json["id"].IsUint64())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'id\' is not a valid number).");
	unsigned long id = json["id"].GetUint64();

	// check query
	if(!(this->database.isQuery(id))) {
		std::ostringstream errStrStr;
		errStrStr << "Query #" << id << " not found.";
		return Server::ServerCommandResponse(true, errStrStr.str());
	}

	// duplicate query
	unsigned long newId = this->database.duplicateQuery(id);
	if(!newId) return Server::ServerCommandResponse(true, "Could not add duplicate to database.");

	return Server::ServerCommandResponse("Query duplicated.", newId);
}

// server command testquery(query, type, resultbool, resultsingle, resultmulti, textonly, text): test query on text
void Server::cmdTestQuery(ConnectionPtr connection, unsigned long index, const std::string& message) {
	Server::ServerCommandResponse response;

	// parse JSON (again for thread)
	rapidjson::Document json;
	if(json.Parse(message.c_str()).HasParseError())
		response = Server::ServerCommandResponse(true, "Could not parse JSON.");
	else if(!json.IsObject())
		response = Server::ServerCommandResponse(true, "Parsed JSON is not an object.");
	else {
		// get arguments
		if(!json.HasMember("query"))
			response = Server::ServerCommandResponse(true, "Invalid arguments (\'query\' is missing).");
		else if(!json["query"].IsString())
			response = Server::ServerCommandResponse(true, "Invalid arguments (\'query\' is not a string).");
		else if(!json.HasMember("type"))
			response = Server::ServerCommandResponse(true, "Invalid arguments (\'type\' is missing).");
		else if(!json["type"].IsString())
			response = Server::ServerCommandResponse(true, "Invalid arguments (\'type\' is not a string).");
		else if(!json.HasMember("resultbool"))
			response = Server::ServerCommandResponse(true, "Invalid arguments (\'resultbool\' is missing).");
		else if(!json["resultbool"].IsBool())
			response = Server::ServerCommandResponse(true, "Invalid arguments (\'resultbool\' is not a boolean).");
		else if(!json.HasMember("resultsingle"))
			response = Server::ServerCommandResponse(true, "Invalid arguments (\'resultsingle\' is missing).");
		else if(!json["resultsingle"].IsBool())
			response = Server::ServerCommandResponse(true, "Invalid arguments (\'resultsingle\' is not a boolean).");
		else if(!json.HasMember("resultmulti"))
			response = Server::ServerCommandResponse(true, "Invalid arguments (\'resultmulti\' is missing).");
		else if(!json["resultmulti"].IsBool())
			response = Server::ServerCommandResponse(true, "Invalid arguments (\'resultmulti\' is not a boolean).");
		else if(!json.HasMember("textonly"))
			response = Server::ServerCommandResponse(true, "Invalid arguments (\'textonly\' is missing).");
		else if(!json["textonly"].IsBool())
			response = Server::ServerCommandResponse(true, "Invalid arguments (\'textonly\' is not a boolean).");
		else if(!json.HasMember("text"))
			response = Server::ServerCommandResponse(true, "Invalid arguments (\'text\' is missing).");
		else if(!json["text"].IsString())
			response = Server::ServerCommandResponse(true, "Invalid arguments (\'text\' is not a string).");
		else {
			std::string query = json["query"].GetString();
			std::string type = json["type"].GetString();
			bool resultBool = json["resultbool"].GetBool();
			bool resultSingle = json["resultsingle"].GetBool();
			bool resultMulti = json["resultmulti"].GetBool();
			bool textOnly = json["textonly"].GetBool();
			std::string text = json["text"].GetString();

			// check query text, query type and result type
			if(query.empty())
				response = Server::ServerCommandResponse(true, "Query text is empty.");
			else if(type.empty())
				response = Server::ServerCommandResponse(true, "Query type is empty.");
			else if(type != "regex" && type != "xpath")
				response = Server::ServerCommandResponse(true, "Unknown query type: \'" + type + "\'.");
			else if(!resultBool && !resultSingle && !resultMulti)
				response = Server::ServerCommandResponse(true, "No result type selected.");
			else {
				// test query
				std::string result;
				if(type == "regex") {
					// test RegEx expression on text
					try {
						Timer::SimpleHR timer;
						Query::RegEx regExTest(query, resultBool || resultSingle, resultMulti);
						result = "COMPILING TIME: " + timer.tickStr() + "\n";
						if(resultBool) {
							// get boolean result (does at least one match exist?)
							result += "BOOLEAN RESULT (" + timer.tickStr() + "): "
								+ std::string(regExTest.getBool(text) ? "true" : "false") + "\n";
						}
						if(resultSingle) {
							// get first result (first full match)
							std::string tempResult;
							regExTest.getFirst(text, tempResult);
							if(tempResult.empty()) result += "FIRST RESULT (" + timer.tickStr() + "): [empty]\n";
							else result += "FIRST RESULT (" + timer.tickStr() + "): " + tempResult + "\n";
						}
						if(resultMulti) {
							// get all results (all full matches)
							std::vector<std::string> tempResult;
							regExTest.getAll(text, tempResult);
							result += "ALL RESULTS (" + timer.tickStr() + "):";

							if(tempResult.empty()) result += " [empty]\n";
							else {
								unsigned long n = 0;
								std::ostringstream resultStrStr;
								resultStrStr << '\n';
								for(auto i = tempResult.begin(); i != tempResult.end(); ++i) {
									resultStrStr << '[' << (n + 1) << "] " << tempResult.at(n) << '\n';
									n++;
								}
								result += resultStrStr.str();
							}
						}
					}
					catch(const RegExException& e) {
						response = Server::ServerCommandResponse(true, "RegEx error: " + e.whatStr());
					}
				}
				else {
					// test XPath expression on text
					try {
						Parsing::XML xmlDocumentTest;
						Timer::SimpleHR timer;
						Query::XPath xPathTest(query, textOnly);
						result = "COMPILING TIME: " + timer.tickStr() + "\n";
						try {
							xmlDocumentTest.parse(text);
							result += "PARSING TIME: " + timer.tickStr() + "\n";

							if(resultBool) {
								// get boolean result (does at least one match exist?)
								result += "BOOLEAN RESULT (" + timer.tickStr() + "): "
										+ std::string(xPathTest.getBool(xmlDocumentTest) ? "true" : "false") + "\n";
							}
							if(resultSingle) {
								// get first result (first full match)
								std::string tempResult;
								Timer::SimpleHR timer;
								xPathTest.getFirst(xmlDocumentTest, tempResult);
								if(tempResult.empty()) result += "FIRST RESULT (" + timer.tickStr() + "): [empty]\n";
								else result += "FIRST RESULT (" + timer.tickStr() + "): " + tempResult + "\n";
							}
							if(resultMulti) {
								// get all results (all full matches)
								std::vector<std::string> tempResult;
								Timer::SimpleHR timer;
								xPathTest.getAll(xmlDocumentTest, tempResult);
								result += "ALL RESULTS (" + timer.tickStr() + "):";

								if(tempResult.empty()) result += " [empty]\n";
								else {
									unsigned long n = 0;
									std::ostringstream resultStrStr;
									resultStrStr << '\n';
									for(auto i = tempResult.begin(); i != tempResult.end(); ++i) {
										resultStrStr << '[' << (n + 1) << "] " << tempResult.at(n) << '\n';
										n++;
									}
									result += resultStrStr.str();
								}
							}
						}
						catch(const XMLException& e) {
							response = Server::ServerCommandResponse(true, "XML error: " + e.whatStr());
						}
					}
					catch(const XPathException& e) {
						response = Server::ServerCommandResponse(true, "XPath error - " + e.whatStr());
					}
				}
				if(!response.fail){
					result.pop_back();
					response = Server::ServerCommandResponse(result);
				}
			}
		}
	}

	// create the reply
	rapidjson::StringBuffer replyBuffer;
	rapidjson::Writer<rapidjson::StringBuffer> reply(replyBuffer);
	reply.StartObject();
	if(response.fail) {
		reply.Key("fail");
		reply.Bool(true);
		reply.Key("debug");
		reply.String(message.c_str());
	}
	reply.Key("text");
	reply.String(response.text.c_str(), response.text.size());
	reply.EndObject();
	std::string replyString = replyBuffer.GetString();

	// send reply
	this->webServer.send(connection, 200, "application/json", replyString);

	// set thread status to not running
	{
		std::lock_guard<std::mutex> workersLocked(this->workersLock);
		this->workersRunning.at(index) = false;
	}
}

// server command addconfig(website, module, name, config): Add configuration to database
Server::ServerCommandResponse Server::cmdAddConfig(const rapidjson::Document& json) {
	crawlservpp::Struct::ConfigProperties properties;

	// get arguments
	if(!json.HasMember("website"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'website\' is missing).");
	if(!json["website"].IsUint64())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'website\' is not a valid number).");
	unsigned long website = json["website"].GetUint64();

	if(!json.HasMember("module"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'module\' is missing).");
	if(!json["module"].IsString())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'module\' is not a string).");
	std::string module = json["module"].GetString();

	if(!json.HasMember("name"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'name\' is missing).");
	if(!json["name"].IsString())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'name\' is not a string).");
	properties.name = json["name"].GetString();

	if(!json.HasMember("config"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'config\' is missing).");
	if(!json["config"].IsString())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'config\' is not a string).");
	properties.config = json["config"].GetString();

	// check name
	if(properties.name.empty()) return Server::ServerCommandResponse(true, "Name is empty.");

	// check configuration JSON
	rapidjson::Document configJson;
	if(configJson.Parse(properties.config.c_str()).HasParseError())
		return Server::ServerCommandResponse(true, "Could not parse JSON.");
	else if(!configJson.IsArray())
		return Server::ServerCommandResponse(true, "Parsed JSON is not an array.");

	// check analyzer configuration for algorithm
	if(properties.module == "analyzer") {
		if(!Server::getAlgoFromConfig(configJson))
			return Server::ServerCommandResponse(true, "No algorithm selected.");
	}

	// check website
	if(!(this->database.isWebsite(website))) {
		std::ostringstream errStrStr;
		errStrStr << "Website #" << website << " not found.";
		return Server::ServerCommandResponse(true, errStrStr.str());
	}

	// add configuration to database
	unsigned long id = this->database.addConfiguration(website, properties);
	if(!id) return Server::ServerCommandResponse(true, "Could not add configuration to database.");

	return Server::ServerCommandResponse("Configuration added.", id);
}

// server command updateconfig(id, name, config): Update a configuration in the database by its ID
Server::ServerCommandResponse Server::cmdUpdateConfig(const rapidjson::Document& json) {
	crawlservpp::Struct::ConfigProperties properties;

	// get arguments
	if(!json.HasMember("id"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'id\' is missing).");
	if(!json["id"].IsUint64())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'id\' is not a valid number).");
	unsigned long id = json["id"].GetUint64();

	if(!json.HasMember("name"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'name\' is missing).");
	if(!json["name"].IsString())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'name\' is not a string).");
	properties.name = json["name"].GetString();

	if(!json.HasMember("config"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'config\' is missing).");
	if(!json["config"].IsString())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'config\' is not a string).");
	properties.config = json["config"].GetString();

	// check name
	if(properties.name.empty()) return Server::ServerCommandResponse(true, "Name is empty.");

	// check configuration JSON
	rapidjson::Document configJson;
	if(configJson.Parse(properties.config.c_str()).HasParseError())
		return Server::ServerCommandResponse(true, "Could not parse JSON.");
	else if(!configJson.IsArray())
		return Server::ServerCommandResponse(true, "Parsed JSON is not an array.");

	// check configuration
	if(!(this->database.isConfiguration(id))) {
		std::ostringstream errStrStr;
		errStrStr << "Configuration #" << id << " not found.";
		return Server::ServerCommandResponse(true, errStrStr.str());
	}

	// update configuration in database
	this->database.updateConfiguration(id, properties);
	return Server::ServerCommandResponse("Configuration updated.");
}

// server command deleteconfig(id): delete a configuration from the database by its ID
Server::ServerCommandResponse Server::cmdDeleteConfig(const rapidjson::Document& json) {
	// check whether the deletion of data is allowed
	if(!(this->settings.dataDeletable)) return Server::ServerCommandResponse(true, "Not allowed.");

	// get argument
	if(!json.HasMember("id"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'id\' is missing).");
	if(!json["id"].IsUint64())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'id\' is not a valid number).");
	unsigned long id = json["id"].GetUint64();

	// check configuration
	if(!(this->database.isConfiguration(id))) {
		std::ostringstream errStrStr;
		errStrStr << "Configuration #" << id << " not found.";
		Server::ServerCommandResponse(true, errStrStr.str());
	}

	// deleteconfig needs to be confirmed
	if(!json.HasMember("confirmed"))
		return Server::ServerCommandResponse(false, true, "Do you really want to delete this configuration?");

	// delete configuration from database
	this->database.deleteConfiguration(id);

	return Server::ServerCommandResponse("Configuration deleted.");
}

// server command duplicateconfig(id): Duplicate a configuration by its ID
Server::ServerCommandResponse Server::cmdDuplicateConfig(const rapidjson::Document& json) {
	// get argument
	if(!json.HasMember("id"))
		return Server::ServerCommandResponse(true, "Invalid arguments (\'id\' is missing).");
	if(!json["id"].IsUint64())
		return Server::ServerCommandResponse(true, "Invalid arguments (\'id\' is not a valid number).");
	unsigned long id = json["id"].GetUint64();

	// check configuration
	if(!(this->database.isConfiguration(id))) {
		std::ostringstream errStrStr;
		errStrStr << "Configuration #" << id << " not found.";
		return Server::ServerCommandResponse(true, errStrStr.str());
	}

	// duplicate configuration
	unsigned long newId = this->database.duplicateConfiguration(id);
	if(!newId) return Server::ServerCommandResponse(true, "Could not add duplicate to database.");

	return Server::ServerCommandResponse("Configuration duplicated.", newId);
}

}
