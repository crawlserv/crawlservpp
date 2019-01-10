/*
 * Server.cpp
 *
 * The command-and-control server. Uses the mongoose and RapidJSON libraries to implement a HTTP server for receiving JSON-formatted
 *  commands and sending JSON-formatted replies from/to the crawlserv_frontend.
 *
 *  Also handles all threads for the different modules as well as worker threads for query testing.
 *
 *  Created on: Oct 7, 2018
 *      Author: ans
 */

#include "Server.h"

// constructor
Server::Server(const DatabaseSettings& databaseSettings, const ServerSettings& serverSettings) : database(databaseSettings) {
	// set default values
	this->dbSettings = databaseSettings;
	this->settings = serverSettings;
	this->allowed = serverSettings.allowedClients;
	this->running = true;
	mg_connection * connection = NULL;

	// create cookies directory if it does not exist
	if(!std::experimental::filesystem::is_directory("cookies") || !std::experimental::filesystem::exists("cookies")) {
		std::experimental::filesystem::create_directory("cookies");
	}

	// connect to database and initialize it
	if(!(this->database.connect())) throw std::runtime_error(this->database.getErrorMessage());
	if(!(this->database.initializeSql())) throw std::runtime_error(this->database.getErrorMessage());
	if(!(this->database.prepare())) throw std::runtime_error(this->database.getErrorMessage());

	// initialize mongoose HTTP server
	mg_mgr_init(&(this->eventManager), NULL);
	connection = mg_bind(&(this->eventManager), serverSettings.port.c_str(), Server::eventHandler);
	if(!connection) {
		mg_mgr_free(&(this->eventManager));
		throw std::runtime_error("Could not bind server to port " + serverSettings.port);
	}
	connection->user_data = this;
	mg_set_protocol_http_websocket(connection);

	// set initial status
	this->setStatus("crawlserv is ready");

	// load threads from database
	std::vector<ThreadDatabaseEntry> threads = this->database.getThreads();
	for(auto i = threads.begin(); i != threads.end(); ++i) {
		if(i->module == "crawler") {
			// load crawler thread
			ThreadCrawler * crawler = new ThreadCrawler(this->database, i->id, i->status, i->paused, i->options, i->last);
			crawler->Thread::start();
			this->crawlers.push_back(crawler);

			// write to log
			std::ostringstream logStrStr;
			logStrStr << "#" << i->id << " continued.";
			this->database.log("crawler", logStrStr.str());
		}
		else if(i->module == "parser") {
			// load parser thread
			ThreadParser * parser = new ThreadParser(this->database, i->id, i->status, i->paused, i->options, i->last);
			parser->Thread::start();
			this->parsers.push_back(parser);

			// write to log
			std::ostringstream logStrStr;
			logStrStr << "#" << i->id << " continued.";
			this->database.log("parser", logStrStr.str());
		}
		else if(i->module == "extractor") {
			// load extractor thread
			//ThreadExtractor * extractor = new ThreadExtractor(this->database, i->id, i->status, i->paused, i->options, i->last);
			//extractor->Thread::start();
			//this->extractors.push_back(extractor);

			// write to log
			std::ostringstream logStrStr;
			logStrStr << "#" << i->id << " continued.";
			this->database.log("extractor", logStrStr.str());
		}
		else if(i->module == "analyzer") {
			// load analyzer thread
			//ThreadAnalyzer * analyzer = new ThreadAnalyzer(this->database, i->id, i->status, i->paused, i->options, i->last);
			//analyzer->Thread::start();
			//this->analyzers.push_back(analyzer);

			// write to log
			std::ostringstream logStrStr;
			logStrStr << "#" << i->id << " continued.";
			this->database.log("analyzer", logStrStr.str());
		}
		else throw std::runtime_error("Unknown thread module \'" + i->module + "\'");
	}

	// save start time for up-time calculation
	this->uptimeStart = std::chrono::steady_clock::now();

	// start logging
	this->database.log("server", "Server started.");
}

// destructor
Server::~Server() {
	// interrupt module threads
	for(auto i = this->crawlers.begin(); i != this->crawlers.end(); ++i) if(*i) (*i)->Thread::sendInterrupt();
	for(auto i = this->parsers.begin(); i != this->parsers.end(); ++i) if(*i) (*i)->Thread::sendInterrupt();
	//for(auto i = this->extractors.begin(); i != this->extractors.end(); ++i) if(*i) (*i)->Thread::sendInterrupt();
	//for(auto i = this->analyzers.begin(); i != this->analyzers.end(); ++i) if(*i) (*i)->Thread::sendInterrupt();

	// wait for module threads and delete them
	for(auto i = this->crawlers.begin(); i != this->crawlers.end(); ++i) {
		if(*i) {
			// get thread id (for logging)
			unsigned long id = (*i)->getId();

			// wait for thread and delete it
			(*i)->Thread::finishInterrupt();
			delete *i;
			*i = NULL;

			// log interruption
			std::ostringstream logStrStr;
			logStrStr << "#" << id << " interrupted.";
			this->database.log("crawler", logStrStr.str());
		}
	}
	for(auto i = this->parsers.begin(); i != this->parsers.end(); ++i) {
		if(*i) {
			// get thread id (for logging)
			unsigned long id = (*i)->getId();

			// wait for thread and delete it
			(*i)->Thread::finishInterrupt();
			delete *i;
			*i = NULL;

			// log interruption
			std::ostringstream logStrStr;
			logStrStr << "#" << id << " interrupted.";
			this->database.log("parser", logStrStr.str());
		}
	}
	/*for(auto i = this->extractors.begin(); i != this->extractors.end(); ++i) {
		if(*i) {
			// get thread id (for logging)
			unsigned long id = (*i)->getId();

			// wait for thread and delete it
			(*i)->Thread::finishInterrupt();
			delete *i;
			*i = NULL;

			// log interruption
			std::ostringstream logStrStr;
			logStrStr << "#" << id << " interrupted.";
			this->database.log("extractor", logStrStr.str());
		}
	}*/
	/*for(auto i = this->analyzers.begin(); i != this->analyzers.end(); ++i) {
		if(*i) {
			// get thread id (for logging)
			unsigned long id = (*i)->getId();

			// wait for thread and delete it
			(*i)->Thread::finishInterrupt();
			delete *i;
			*i = NULL;

			// log interruption
			std::ostringstream logStrStr;
			logStrStr << "#" << id << " interrupted.";
			this->database.log("analyzer", logStrStr.str());
		}
	}*/

	// wait for worker threads and delete them
	for(auto i = this->workers.begin(); i != this->workers.end(); ++i) {
		if(*i) {
			if((*i)->joinable()) (*i)->join();
			delete *i;
		}
	}

	// free mongoose HTTP server
	mg_mgr_free(&(this->eventManager));

	// log shutdown message with server up-time
	this->database.log("server", "Shutting down after up-time of " + DateTime::secondsToString(this->getUpTime()) + ".");
}

// server tick
bool Server::tick() {
	mg_mgr_poll(&(this->eventManager), 1000);

	// check whether a thread was terminated
	for(unsigned long n = 1; n <= this->crawlers.size(); n++) {
		if(this->crawlers.at(n - 1)->isTerminated()) {
			this->crawlers.at(n - 1)->Thread::stop();
			this->crawlers.erase(this->crawlers.begin() + (n - 1));
			n--;
		}
	}
	for(unsigned long n = 1; n <= this->parsers.size(); n++) {
		if(this->parsers.at(n - 1)->isTerminated()) {
			this->parsers.at(n - 1)->Thread::stop();
			this->parsers.erase(this->parsers.begin() + (n - 1));
			n--;
		}
	}
	/*
	for(unsigned long n = 1; n <= this->extractors.size(); n++) {
		if(this->extractors.at(n - 1)->isTerminated()) {
			this->extractors.at(n - 1)->Thread::stop();
			this->extractors.erase(this->extractors.begin() + (n - 1));
			n--;
		}
	}
	for(unsigned long n = 1; n <= this->analyzers.size(); n++) {
		if(this->analyzers.at(n - 1)->isTerminated()) {
			this->analyzers.at(n - 1)->Thread::stop();
			this->analyzers.erase(this->analyzers.begin() + (n - 1));
			n--;
		}
	}
	*/

	// check whether a worker thread was terminated
	{
		std::lock_guard<std::mutex> workersLocked(this->workersLock);
		for(unsigned long n = 0; n < this->workers.size(); n++) {
			std::thread * worker = this->workers.at(n);
			if(worker && !(this->workersRunning.at(n))) {
				// join thread and unset thread pointer
				if(worker->joinable()) worker->join();
				delete worker;
				this->workers.at(n) = NULL;
			}
		}
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
std::string Server::cmd(const char * body, struct mg_connection * connection, bool& threadStartedTo) {
	std::string ip = Server::getIP(connection);
	Server::CmdResponse response;

	// parse JSON
	rapidjson::Document json;
	if(json.Parse(body).HasParseError()) response = Server::CmdResponse(true, "Could not parse JSON.");
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
					else if(command == "unpausecrawler") response = this->cmdUnPauseCrawler(json, ip);
					else if(command == "stopcrawler") response = this->cmdStopCrawler(json, ip);

					else if(command == "startparser") response = this->cmdStartParser(json, ip);
					else if(command == "pauseparser") response = this->cmdPauseParser(json, ip);
					else if(command == "unpauseparser") response = this->cmdUnPauseParser(json, ip);
					else if(command == "stopparser") response = this->cmdStopParser(json, ip);

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
							this->workers.push_back(
									new std::thread(&Server::cmdTestQuery, this, this->workers.size(), body, connection)
							);
						}
						threadStartedTo = true;
					}

					else if(command == "addconfig") response = this->cmdAddConfig(json);
					else if(command == "updateconfig") response = this->cmdUpdateConfig(json);
					else if(command == "deleteconfig") response = this->cmdDeleteConfig(json);
					else if(command == "duplicateconfig") response = this->cmdDuplicateConfig(json);

					else if(command.length()) {
						// unknown command: debug the command and its arguments
						response.fail = true;
						response.text = "Unknown command \'" + command + "\'.";
					}

					else {
						response.fail = true;
						response.text = "Empty command.";
					}
				}
				catch(std::exception &e) {
					// exceptions caused by server commands should not kill the server (and are attributed to the frontend)
					this->database.log("frontend", e.what());
					response.fail = true;
					response.text = e.what();
				}
			}
			else {
				response.fail = true;
				response.text = "Invalid command: Name is not a string.";
			}
		}
		else {
			response.fail = true;
			response.text = "No command specified.";
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
		reply.String(body);
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

// handle server events
void Server::eventHandler(mg_connection * connection, int event, void * data) {
	// get pointer to server
	Server * server = (Server *) connection->user_data;

	if(event == MG_EV_ACCEPT) {
		// check authorization
		if(server->allowed != "*") {
			std::string ip = Server::getIP(connection);
			if(server->allowed.find(ip) == std::string::npos) {
				connection->flags |= MG_F_CLOSE_IMMEDIATELY;
				server->database.log("server", "Client " + ip + " refused.");
			}
			else server->database.log("server", "Client " + ip + " accepted.");
		}
	}
	else if(event == MG_EV_HTTP_REQUEST) {
		// check authorization
		if(server->allowed != "*") {
			std::string ip = Server::getIP(connection);
			if(server->allowed.find(ip) == std::string::npos) {
				connection->flags |= MG_F_CLOSE_IMMEDIATELY;
				server->database.log("server", "Client " + ip + " refused.");
			}
		}

		// parse HTTP request
		http_message * httpMessage = (http_message *) data;

#ifdef SERVER_DEBUG_HTTP_REQUEST
		// debug HTTP request
		std::cout << std::endl << std::string(httpMessage->message.p, 0, httpMessage->message.len) << std::endl;
#endif

		// check for GET request
		if(httpMessage->method.len == 3 &&
				httpMessage->method.p[0] == 'G' && httpMessage->method.p[1] == 'E' && httpMessage->method.p[2] == 'T') {
			std::string status = server->getStatus();
			mg_send_head(connection, 200, status.length(), "Content-Type: text/plain\r\n"
					"Access-Control-Allow-Origin: *\r\n"
					"Access-Control-Allow-Methods: GET, POST\r\n"
					"Access-Control-Allow-Headers: Content-Type");
			mg_printf(connection, "%s", status.c_str());
		}
		// check for POST request
		else if(httpMessage->method.len == 4 &&
				httpMessage->method.p[0] == 'P' && httpMessage->method.p[1] == 'O' && httpMessage->method.p[2] == 'S'
						&& httpMessage->method.p[3] == 'T') {
			std::string replyString;
			char * postBody = new char[httpMessage->message.len + 1];
			for(unsigned long n = 0; n < httpMessage->body.len; n++) postBody[n] = httpMessage->body.p[n];
			postBody[httpMessage->body.len] = '\0';

			// parse JSON
			bool threadStarted = false;
			replyString = server->cmd(postBody, connection, threadStarted);

			if(!threadStarted) {
				// delete buffer for body
				delete[] postBody;

				// send reply
				mg_send_head(connection, 200, replyString.size(), "Content-Type: application/json\r\n"
						"Access-Control-Allow-Origin: *\r\n"
						"Access-Control-Allow-Methods: GET, POST\r\n"
						"Access-Control-Allow-Headers: Content-Type");
				mg_printf(connection, "%s", replyString.c_str());
			}
		}
		else if(httpMessage->method.len == 7 &&
				httpMessage->method.p[0] == 'O' && httpMessage->method.p[1] == 'P' && httpMessage->method.p[2] == 'T'
					&& httpMessage->method.p[3] == 'I' && httpMessage->method.p[4] == 'O' && httpMessage->method.p[5] == 'N'
					&& httpMessage->method.p[6] == 'S') {
			mg_send_head(connection, 200, 0, "Access-Control-Allow-Origin: *\r\n"
					"Access-Control-Allow-Methods: GET, POST\r\n"
					"Access-Control-Allow-Headers: Content-Type");
		}
	}
}

// static helper function: check the validity of a namespace name
bool Server::cmdCheckNameSpace(const std::string& nameSpace) {
	return nameSpace.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789$_") == std::string::npos;
}

// static helper function: get client IP from connection
std::string Server::getIP(mg_connection * connection) {
	char ip[INET6_ADDRSTRLEN];
	mg_sock_to_str(connection->sock, ip, INET6_ADDRSTRLEN, MG_SOCK_STRINGIFY_REMOTE | MG_SOCK_STRINGIFY_IP);
	return std::string(ip);
}

// server command kill: kill the server
Server::CmdResponse Server::cmdKill(const rapidjson::Document& json, const std::string& ip) {
	// kill needs to be confirmed
	if(json.HasMember("confirmed")) {
		// kill server
		this->running = false;

		// kill is a logged command
		this->database.log("server", "Killed by " + ip + ".");

		// send bye message
		return Server::CmdResponse("Bye bye.");
	}

	return Server::CmdResponse(false, true, "Are you sure to kill the server?");
}

// server command allow(ip): allow acces for the specified IP(s)
Server::CmdResponse Server::cmdAllow(const rapidjson::Document& json, const std::string& ip) {
	// get argument
	if(!json.HasMember("ip")) return Server::CmdResponse(true, "Invalid arguments (\'ip\' is missing).");
	if(!json["ip"].IsString()) return Server::CmdResponse(true, "Invalid arguments (\'ip\' is not a string).");
	std::string toAllow = json["ip"].GetString();

	// allow needs to be confirmed
	if(!json.HasMember("confirmed")) {
		return Server::CmdResponse(false, true, "Do you really want to allow " + toAllow + " access to the server?");
	}

	// add IP(s)
	this->allowed += "," + toAllow;

	// allow is a logged command
	this->database.log("server", toAllow + " allowed by " + ip + ".");
	return Server::CmdResponse("Allowed IPs: " + this->allowed + ".");
}

// server command disallow: revoke access from all except the initial IP(s) specified by the configuration file
Server::CmdResponse Server::cmdDisallow(const rapidjson::Document& json, const std::string& ip) {
	// reset alled IP(s)
	this->allowed = this->settings.allowedClients;

	// disallow is a logged command
	this->database.log("server", "Allowed IPs reset by " + ip + ".");

	return Server::CmdResponse("Allowed IP(s): " + this->allowed + ".");
}

// server command log(entry): write a log entry by the frontend into the database
Server::CmdResponse Server::cmdLog(const rapidjson::Document& json) {
	// get argument
	if(!json.HasMember("entry")) return Server::CmdResponse(true, "Invalid arguments (\'entry\' is missing).");
	if(!json["entry"].IsString()) return Server::CmdResponse(true, "Invalid arguments (\'entry\' is not a string).");
	std::string entry = json["entry"].GetString();

	// write log entry
	this->database.log("frontend", entry);
	return Server::CmdResponse("Wrote log entry: " + entry);
}

// server command clearlog([module]): remove all log entries or the log entries of a specific module (or all when module is empty/not given)
Server::CmdResponse Server::cmdClearLog(const rapidjson::Document& json, const std::string& ip) {
	// check whether the clearing of logs is allowed
	if(!(this->settings.logsDeletable)) return Server::CmdResponse(true, "Not allowed.");

	// get argument
	std::string module;
	if(json.HasMember("module") && json["module"].IsString()) module = json["module"].GetString();

	// clearlog needs to be confirmed
	if(!json.HasMember("confirmed")) {
		std::ostringstream replyStrStr;
		replyStrStr.imbue(std::locale(""));
		replyStrStr << "CONFIRM Are you sure to delete " << this->database.getNumberOfLogEntries(module) << " log entries?";
		return Server::CmdResponse(false, true, replyStrStr.str());
	}

	this->database.clearLogs(module);

	// clearlog is a logged command
	if(module.length()) {
		this->database.log("server", "Logs of " + module + " cleared by " + ip + ".");
		return Server::CmdResponse("Logs of " + module + " cleared.");
	}

	this->database.log("server", "All logs cleared by " + ip + ".");
	return Server::CmdResponse("All logs cleared.");
}

// server command startcrawler(website, urllist, config): start a crawler using the specified website, URL list and configuration
Server::CmdResponse Server::cmdStartCrawler(const rapidjson::Document& json, const std::string& ip) {
	// get arguments
	ThreadOptions options;
	if(!json.HasMember("website")) return Server::CmdResponse(true, "Invalid arguments (\'website\' is missing).");
	if(!json["website"].IsInt64()) return Server::CmdResponse(true, "Invalid arguments (\'website\' is not a number).");
	options.website = json["website"].GetInt64();
	if(!json.HasMember("urllist")) return Server::CmdResponse(true, "Invalid arguments (\'urllist\' is missing).");
	if(!json["urllist"].IsInt64()) return Server::CmdResponse(true, "Invalid arguments (\'urllist\' is not a number).");
	options.urlList = json["urllist"].GetInt64();
	if(!json.HasMember("config")) return Server::CmdResponse(true, "Invalid arguments (\'config\' is missing).");
	if(!json["config"].IsInt64()) return Server::CmdResponse(true, "Invalid arguments (\'config\' is not a number).");
	options.config = json["config"].GetInt64();

	// check arguments
	if(!(this->database.isWebsite(options.website))) {
		std::ostringstream errStrStr;
		errStrStr << "Website #" << options.website << " not found.";
		return Server::CmdResponse(true, errStrStr.str());
	}
	if(!(this->database.isUrlList(options.website, options.urlList))) {
		std::ostringstream errStrStr;
		errStrStr << "URL list #" << options.urlList << " for website #" << options.website << " not found.";
		return Server::CmdResponse(true, errStrStr.str());
	}
	if(!(this->database.isConfiguration(options.website, options.config))) {
		std::ostringstream errStrStr;
		errStrStr << "Configuration #" << options.config << " for website #" << options.website << " not found.";
		return Server::CmdResponse(true, errStrStr.str());
	}

	// create and start crawler
	ThreadCrawler * newCrawler = new ThreadCrawler(this->database, options);
	this->crawlers.push_back(newCrawler);
	newCrawler->Thread::start();
	unsigned long id = newCrawler->Thread::getId();

	// startcrawler is a logged command
	std::ostringstream logStrStr;
	logStrStr << "[#" << id << "] started by " << ip << ".";
	this->database.log("crawler", logStrStr.str());

	return Server::CmdResponse("Crawler has been started.");
}

// server command pausecrawler(id): pause a crawler by its ID
Server::CmdResponse Server::cmdPauseCrawler(const rapidjson::Document& json, const std::string& ip) {
	// get argument
	if(!json.HasMember("id")) return Server::CmdResponse(true, "Invalid arguments (\'id\' is missing).");
	if(!json["id"].IsInt64()) return Server::CmdResponse(true, "Invalid arguments (\'id\' is not a number).");
	unsigned long id = json["id"].GetInt64();

	// find crawler
	auto i = std::find_if(this->crawlers.begin(), this->crawlers.end(), [&id](ThreadCrawler * p) { return p->Thread::getId() == id; });
	if(i == this->crawlers.end()) {
		std::ostringstream errStrStr;
		errStrStr << "Could not find crawler #" << id << ".";
		return Server::CmdResponse(true, errStrStr.str());
	}

	// pause crawler
	(*i)->Thread::pause();

	// pausecrawler is a logged command
	std::ostringstream logStrStr;
	logStrStr << "[#" << id << "] paused by " << ip << ".";
	this->database.log("crawler", logStrStr.str());

	return Server::CmdResponse("Crawler is pausing.");
}

// server command unpausecrawler(id): unpause a crawler by its ID
Server::CmdResponse Server::cmdUnPauseCrawler(const rapidjson::Document& json, const std::string& ip) {
	// get argument
	if(!json.HasMember("id")) return Server::CmdResponse(true, "Invalid arguments (\'id\' is missing).");
	if(!json["id"].IsInt64()) return Server::CmdResponse(true, "Invalid arguments (\'id\' is not a number).");
	unsigned long id = json["id"].GetInt64();

	// find crawler
	auto i = std::find_if(this->crawlers.begin(), this->crawlers.end(), [&id](ThreadCrawler * p) { return p->Thread::getId() == id; });
	if(i == this->crawlers.end()) {
		std::ostringstream errStrStr;
		errStrStr << "Could not find crawler #" << id << ".";
		return Server::CmdResponse(true, errStrStr.str());
	}

	// unpause crawler
	(*i)->Thread::unpause();

	// unpausecrawler is a logged command
	std::ostringstream logStrStr;
	logStrStr << "[#" << id << "] unpaused by " << ip << ".";
	this->database.log("crawler", logStrStr.str());

	return Server::CmdResponse("Crawler is unpausing.");
}

// server command stopcrawler(id): stop a crawler by its ID
Server::CmdResponse Server::cmdStopCrawler(const rapidjson::Document& json, const std::string& ip) {
	// get argument
	if(!json.HasMember("id")) return Server::CmdResponse(true, "Invalid arguments (\'id\' is missing).");
	if(!json["id"].IsInt64()) return Server::CmdResponse(true, "Invalid arguments (\'id\' is not a number).");
	unsigned long id = json["id"].GetInt64();

	// find crawler
	auto i = std::find_if(this->crawlers.begin(), this->crawlers.end(), [&id](ThreadCrawler * p) { return p->Thread::getId() == id; });
	if(i == this->crawlers.end()) {
		std::ostringstream errStrStr;
		errStrStr << "Could not find crawler #" << id << ".";
		return Server::CmdResponse(true, errStrStr.str());
	}

	// stop crawler
	(*i)->Thread::stop();
	delete *i;
	this->crawlers.erase(i);

	// stopcrawler is a logged command
	std::ostringstream logStrStr;
	logStrStr << "[#" << id << "] Stopped by " << ip << ".";
	this->database.log("crawler", logStrStr.str());

	return Server::CmdResponse("Crawler stopped.");
}

// server command startparser(website, urllist, config): start a parser using the specified website, URL list and configuration
Server::CmdResponse Server::cmdStartParser(const rapidjson::Document& json, const std::string& ip) {
	// get arguments
	ThreadOptions options;
	if(!json.HasMember("website")) return Server::CmdResponse(true, "Invalid arguments (\'website\' is missing).");
	if(!json["website"].IsInt64()) return Server::CmdResponse(true, "Invalid arguments (\'website\' is not a number).");
	options.website = json["website"].GetInt64();
	if(!json.HasMember("urllist")) return Server::CmdResponse(true, "Invalid arguments (\'urllist\' is missing).");
	if(!json["urllist"].IsInt64()) return Server::CmdResponse(true, "Invalid arguments (\'urllist\' is not a number).");
	options.urlList = json["urllist"].GetInt64();
	if(!json.HasMember("config")) return Server::CmdResponse(true, "Invalid arguments (\'config\' is missing).");
	if(!json["config"].IsInt64()) return Server::CmdResponse(true, "Invalid arguments (\'config\' is not a number).");
	options.config = json["config"].GetInt64();

	// check arguments
	if(!(this->database.isWebsite(options.website))) {
		std::ostringstream errStrStr;
		errStrStr << "Website #" << options.website << " not found.";
		return Server::CmdResponse(true, errStrStr.str());
	}
	if(!(this->database.isUrlList(options.website, options.urlList))) {
		std::ostringstream errStrStr;
		errStrStr << "URL list #" << options.urlList << " for website #" << options.website << " not found.";
		return Server::CmdResponse(true, errStrStr.str());
	}
	if(!(this->database.isConfiguration(options.website, options.config))) {
		std::ostringstream errStrStr;
		errStrStr << "Configuration #" << options.config << " for website #" << options.website << " not found.";
		return Server::CmdResponse(true, errStrStr.str());
	}

	// create and start parser
	ThreadParser * newParser = new ThreadParser(this->database, options);
	this->parsers.push_back(newParser);
	newParser->Thread::start();
	unsigned long id = newParser->Thread::getId();

	// startparser is a logged command
	std::ostringstream logStrStr;
	logStrStr << "[#" << id << "] started by " << ip << ".";
	this->database.log("parser", logStrStr.str());

	return Server::CmdResponse("Parser has been started.");
}

// server command pauseparser(id): pause a parser by its ID
Server::CmdResponse Server::cmdPauseParser(const rapidjson::Document& json, const std::string& ip) {
	// get argument
	if(!json.HasMember("id")) return Server::CmdResponse(true, "Invalid arguments (\'id\' is missing).");
	if(!json["id"].IsInt64()) return Server::CmdResponse(true, "Invalid arguments (\'id\' is not a number).");
	unsigned long id = json["id"].GetInt64();

	// find parser
	auto i = std::find_if(this->parsers.begin(), this->parsers.end(), [&id](ThreadParser * p) { return p->Thread::getId() == id; });
	if(i == this->parsers.end()) {
		std::ostringstream errStrStr;
		errStrStr << "Could not find parser #" << id << ".";
		return Server::CmdResponse(true, errStrStr.str());
	}

	// pause parser
	(*i)->Thread::pause();

	// pauseparser is a logged command
	std::ostringstream logStrStr;
	logStrStr << "[#" << id << "] paused by " << ip << ".";
	this->database.log("parser", logStrStr.str());

	return Server::CmdResponse("Parser is pausing.");
}

// server command unpauseparser(id): unpause a parser by its ID
Server::CmdResponse Server::cmdUnPauseParser(const rapidjson::Document& json, const std::string& ip) {
	// get argument
	if(!json.HasMember("id")) return Server::CmdResponse(true, "Invalid arguments (\'id\' is missing).");
	if(!json["id"].IsInt64()) return Server::CmdResponse(true, "Invalid arguments (\'id\' is not a number).");
	unsigned long id = json["id"].GetInt64();

	// find parser
	auto i = std::find_if(this->parsers.begin(), this->parsers.end(), [&id](ThreadParser * p) { return p->Thread::getId() == id; });
	if(i == this->parsers.end()) {
		std::ostringstream errStrStr;
		errStrStr << "Could not find parser #" << id << ".";
		return Server::CmdResponse(true, errStrStr.str());
	}

	// unpause parser
	(*i)->Thread::unpause();

	// unpauseparser is a logged command
	std::ostringstream logStrStr;
	logStrStr << "[#" << id << "] unpaused by " << ip << ".";
	this->database.log("parser", logStrStr.str());

	return Server::CmdResponse("Parser is unpausing.");
}

// server command stopparser(id): stop a parser by its ID
Server::CmdResponse Server::cmdStopParser(const rapidjson::Document& json, const std::string& ip) {
	// get argument
	if(!json.HasMember("id")) return Server::CmdResponse(true, "Invalid arguments (\'id\' is missing).");
	if(!json["id"].IsInt64()) return Server::CmdResponse(true, "Invalid arguments (\'id\' is not a number).");
	unsigned long id = json["id"].GetInt64();

	// find parser
	auto i = std::find_if(this->parsers.begin(), this->parsers.end(), [&id](ThreadParser * p) { return p->Thread::getId() == id; });
	if(i == this->parsers.end()) {
		std::ostringstream errStrStr;
		errStrStr << "Could not find parser #" << id << ".";
		return Server::CmdResponse(true, errStrStr.str());
	}

	// stop parser
	(*i)->Thread::stop();
	delete *i;
	this->parsers.erase(i);

	// stopparser is a logged command
	std::ostringstream logStrStr;
	logStrStr << "[#" << id << "] Stopped by " << ip << ".";
	this->database.log("parser", logStrStr.str());

	return Server::CmdResponse("Parser stopped.");
}

// server command addwebsite(name, namespace, domain): add a website
Server::CmdResponse Server::cmdAddWebsite(const rapidjson::Document& json) {
	// get arguments
	if(!json.HasMember("name")) return Server::CmdResponse(true, "Invalid arguments (\'name\' is missing).");
	if(!json["name"].IsString()) return Server::CmdResponse(true, "Invalid arguments (\'name\' is not a string).");
	std::string name = json["name"].GetString();
	if(!json.HasMember("namespace")) return Server::CmdResponse(true, "Invalid arguments (\'namespace\' is missing).");
	if(!json["namespace"].IsString()) return Server::CmdResponse(true, "Invalid arguments (\'namespace\' is not a string).");
	std::string nameSpace = json["namespace"].GetString();
	if(!json.HasMember("domain")) return Server::CmdResponse(true, "Invalid arguments (\'domain\' is missing).");
	if(!json["domain"].IsString()) return Server::CmdResponse(true, "Invalid arguments (\'domain\' is not a string).");
	std::string domain = json["domain"].GetString();

	// check name
	if(!name.length()) return Server::CmdResponse(true, "Name is empty.");

	// check namespace
	if(nameSpace.length() < 4) return Server::CmdResponse(true, "Website namespace has to be at least 4 characters long.");
	if(!(this->cmdCheckNameSpace(nameSpace))) return Server::CmdResponse(true, "Invalid character(s) in website namespace.");

	// correct and check domain name (remove protocol from start and slash from the end)
	while(domain.length() > 6 && domain.substr(0, 7) == "http://") domain = domain.substr(7);
	while(domain.length() > 7 && domain.substr(0, 8) == "https://") domain = domain.substr(8);
	while(domain.length() && domain.back() == '/') domain.pop_back();
	if(!domain.length()) return Server::CmdResponse(true, "Domain is empty.");

	// add website to database
	unsigned long id = this->database.addWebsite(name, nameSpace, domain);
	if(!id) return Server::CmdResponse(true, "Could not add website to database.");

	return Server::CmdResponse("Website added.", id);
}

// server command updatewebsite(id, name, namespace, domain): edit a website
Server::CmdResponse Server::cmdUpdateWebsite(const rapidjson::Document& json) {
	// get arguments
	if(!json.HasMember("id")) return Server::CmdResponse(true, "Invalid arguments (\'id\' is missing).");
	if(!json["id"].IsInt64()) return Server::CmdResponse(true, "Invalid arguments (\'id\' is not a number).");
	unsigned long id = json["id"].GetInt64();
	if(!json.HasMember("name")) return Server::CmdResponse(true, "Invalid arguments (\'name\' is missing).");
	if(!json["name"].IsString()) return Server::CmdResponse(true, "Invalid arguments (\'name\' is not a string).");
	std::string name = json["name"].GetString();
	if(!json.HasMember("namespace")) return Server::CmdResponse(true, "Invalid arguments (\'namespace\' is missing).");
	if(!json["namespace"].IsString()) return Server::CmdResponse(true, "Invalid arguments (\'namespace\' is not a string).");
	std::string nameSpace = json["namespace"].GetString();
	if(!json.HasMember("domain")) return Server::CmdResponse(true, "Invalid arguments (\'domain\' is missing).");
	if(!json["domain"].IsString()) return Server::CmdResponse(true, "Invalid arguments (\'domain\' is not a string).");
	std::string domain = json["domain"].GetString();

	// check name
	if(!name.length()) return Server::CmdResponse(true, "Name is empty.");

	// check namespace name
	if(nameSpace.length() < 4) return Server::CmdResponse(true, "Website namespace has to be at least 4 characters long.");
	if(!(this->cmdCheckNameSpace(nameSpace))) return Server::CmdResponse(true, "Invalid character(s) in website namespace.");

	// correct and check domain name (remove protocol from start and slash from the end)
	while(domain.length() > 6 && domain.substr(0, 7) == "http://") domain = domain.substr(7);
	while(domain.length() > 7 && domain.substr(0, 8) == "https://") domain = domain.substr(8);
	while(domain.length() && domain.back() == '/') domain.pop_back();
	if(!domain.length()) Server::CmdResponse(true, "Domain is empty.");

	// check website
	if(!(this->database.isWebsite(id))) {
		std::ostringstream errStrStr;
		errStrStr << "Website #" << id << " not found.";
		return Server::CmdResponse(true, errStrStr.str());
	}

	// check whether threads are using the website
	for(auto i = this->crawlers.begin(); i != this->crawlers.end(); ++i)
		if((*i)->getWebsite() == id) return Server::CmdResponse(true, "Website cannot be changed while crawler is active.");
	for(auto i = this->parsers.begin(); i != this->parsers.end(); ++i)
		if((*i)->getWebsite() == id) return Server::CmdResponse(true, "Website cannot be changed while parser is active.");
	/*for(auto i = this->extractors.begin(); i != this->extractors.end(); ++i)
		if((*i)->getWebsite() == id) return Server::CmdResponse(true, "Website cannot be changed while extractor is active.");
	for(auto i = this->analyzers.begin(); i != this->analyzers.end(); ++i)
		if((*i)->getWebsite() == id) return Server::CmdResponse(true, "Website cannot be changed while analyzer is active.");*/

	// update website in database
	this->database.updateWebsite(id, name, nameSpace, domain);
	return Server::CmdResponse("Website updated.");
}

// server command deletewebsite(id): delete a website and all associated data from the database by its ID
Server::CmdResponse Server::cmdDeleteWebsite(const rapidjson::Document& json, const std::string& ip) {
	// check whether the deletion of data is allowed
	if(!(this->settings.dataDeletable)) return Server::CmdResponse(true, "Not allowed.");

	// get argument
	if(!json.HasMember("id")) return Server::CmdResponse(true, "Invalid arguments (\'id\' is missing).");
	if(!json["id"].IsInt64()) return Server::CmdResponse(true, "Invalid arguments (\'id\' is not a number).");
	unsigned long id = json["id"].GetInt64();

	// check website
	if(!(this->database.isWebsite(id))) {
		std::ostringstream errStrStr;
		errStrStr << "Website #" << id << " not found.";
		return Server::CmdResponse(true, errStrStr.str());
	}

	// check whether threads are using the website
	for(auto i = this->crawlers.begin(); i != this->crawlers.end(); ++i)
		if((*i)->getWebsite() == id) return Server::CmdResponse(true, "Website cannot be deleted while crawler is active.");
	for(auto i = this->parsers.begin(); i != this->parsers.end(); ++i)
		if((*i)->getWebsite() == id) return Server::CmdResponse(true, "Website cannot be deleted while parser is active.");
	/*for(auto i = this->extractors.begin(); i != this->extractors.end(); ++i)
		if((*i)->getWebsite() == id) return Server::CmdResponse(true, "Website cannot be deleted while extractor is active.");
	for(auto i = this->analyzers.begin(); i != this->analyzers.end(); ++i)
		if((*i)->getWebsite() == id) return Server::CmdResponse(true, "Website cannot be deleted while analyzer is active.");*/

	// deletewebsite needs to be confirmed
	if(!json.HasMember("confirmed"))
		return Server::CmdResponse(false, true, "Do you really want to delete this website?\n"
				"!!! All associated data will be lost !!!");

	// delete website from database
	this->database.deleteWebsite(id);

	// deletewebsite is a logged command
	std::ostringstream logStrStr;
	logStrStr << "Website #" << id << " deleted by " << ip << ".";
	this->database.log("database", logStrStr.str());
	return Server::CmdResponse("Website deleted.");
}

// server command duplicatewebsite(id): Duplicate a website by its ID (no processed data will be duplicated)
Server::CmdResponse Server::cmdDuplicateWebsite(const rapidjson::Document& json) {
	// get argument
	if(!json.HasMember("id")) return Server::CmdResponse(true, "Invalid arguments (\'id\' is missing).");
	if(!json["id"].IsInt64()) return Server::CmdResponse(true, "Invalid arguments (\'id\' is not a number).");
	unsigned long id = json["id"].GetInt64();

	// check website
	if(!(this->database.isWebsite(id))) {
		std::ostringstream errStrStr;
		errStrStr << "Website #" << id << " not found.";
		return Server::CmdResponse(true, errStrStr.str());
	}

	// duplicate website configuration
	unsigned long newId = this->database.duplicateWebsite(id);
	if(!newId) return Server::CmdResponse(true, "Could not add duplicate to database.");

	return Server::CmdResponse("Website duplicated.", newId);
}

// server command addurllist(website, name, namespace): add a URL list to the id-specified website
Server::CmdResponse Server::cmdAddUrlList(const rapidjson::Document& json) {
	// get arguments
	if(!json.HasMember("website")) return Server::CmdResponse(true, "Invalid arguments (\'website\' is missing).");
	if(!json["website"].IsInt64()) return Server::CmdResponse(true, "Invalid arguments (\'website\' is not a number).");
	unsigned long website = json["website"].GetInt64();
	if(!json.HasMember("name")) return Server::CmdResponse(true, "Invalid arguments (\'name\' is missing).");
	if(!json["name"].IsString()) return Server::CmdResponse(true, "Invalid arguments (\'name\' is not a string).");
	std::string name = json["name"].GetString();
	if(!json.HasMember("namespace")) return Server::CmdResponse(true, "Invalid arguments (\'namespace\' is missing).");
	if(!json["namespace"].IsString()) return Server::CmdResponse(true, "Invalid arguments (\'namespace\' is not a string).");
	std::string nameSpace = json["namespace"].GetString();

	// check name
	if(!name.length()) return Server::CmdResponse(true, "Name is empty.");

	// check namespace
	if(nameSpace.length() < 4) return Server::CmdResponse(true, "Namespace of URL list has to be at least 4 characters long.");
	if(!(this->cmdCheckNameSpace(nameSpace))) return Server::CmdResponse(true, "Invalid character(s) in namespace of URL list.");
	if(nameSpace == "config") return Server::CmdResponse(true, "Namespace of URL list cannot be \'config\'.");

	// check website
	if(!(this->database.isWebsite(website))) {
		std::ostringstream errStrStr;
		errStrStr << "Website #" << website << " not found.";
		return Server::CmdResponse(true, errStrStr.str());
	}

	// add URL list to database
	unsigned long id = this->database.addUrlList(website, name, nameSpace);
	if(!id) return Server::CmdResponse(true, "Could not add URL list to database.");

	return Server::CmdResponse("URL list added.", id);
}

// server command updateurllist(id, name, namespace): edit a URL list
Server::CmdResponse Server::cmdUpdateUrlList(const rapidjson::Document& json) {
	// get arguments
	if(!json.HasMember("id")) return Server::CmdResponse(true, "Invalid arguments (\'id\' is missing).");
	if(!json["id"].IsInt64()) return Server::CmdResponse(true, "Invalid arguments (\'id\' is not a number).");
	unsigned long id = json["id"].GetInt64();
	if(!json.HasMember("name")) return Server::CmdResponse(true, "Invalid arguments (\'name\' is missing).");
	if(!json["name"].IsString()) return Server::CmdResponse(true, "Invalid arguments (\'name\' is not a string).");
	std::string name = json["name"].GetString();
	if(!json.HasMember("namespace")) return Server::CmdResponse(true, "Invalid arguments (\'namespace\' is missing).");
	if(!json["namespace"].IsString()) return Server::CmdResponse(true, "Invalid arguments (\'namespace\' is not a string).");
	std::string nameSpace = json["namespace"].GetString();

	if(!name.length()) return Server::CmdResponse(true, "Name is empty.");

	// check namespace
	if(nameSpace.length() < 4) return Server::CmdResponse(true, "Namespace of URL list has to be at least 4 characters long.");
	if(!(this->cmdCheckNameSpace(nameSpace))) return Server::CmdResponse(true, "Invalid character(s) in namespace of URL list.");;
	if(nameSpace == "config") return Server::CmdResponse(true, "Namespace of URL list cannot be \'config\'.");

	// check URL list
	if(!(this->database.isUrlList(id))) {
		std::ostringstream errStrStr;
		errStrStr << "URL list #" << id << " not found.";
		return Server::CmdResponse(true, errStrStr.str());
	}

	// check whether threads are using the URL list
	for(auto i = this->crawlers.begin(); i != this->crawlers.end(); ++i)
		if((*i)->getUrlList() == id) return Server::CmdResponse(true, "URL list cannot be changed while crawler is active.");
	for(auto i = this->parsers.begin(); i != this->parsers.end(); ++i)
		if((*i)->getUrlList() == id) return Server::CmdResponse(true, "URL list cannot be changed while parser is active.");
	/*for(auto i = this->extractors.begin(); i != this->extractors.end(); ++i)
		if((*i)->getUrlList() == id) return Server::CmdResponse(true, "URL list cannot be changed while extractor is active.");
	for(auto i = this->analyzers.begin(); i != this->analyzers.end(); ++i)
		if((*i)->getUrlList() == id) return Server::CmdResponse(true, "URL list cannot be changed while analyzer is active.");*/

	// update URL list in database
	this->database.updateUrlList(id, name, nameSpace);
	return Server::CmdResponse("URL list updated.");
}

// server command deleteurllist(id): delete a URL list and all associated data from the database by its ID
Server::CmdResponse Server::cmdDeleteUrlList(const rapidjson::Document& json, const std::string& ip) {
	// check whether the deletion of data is allowed
	if(!(this->settings.dataDeletable)) return Server::CmdResponse(true, "Not allowed.");

	// get argument
	if(!json.HasMember("id")) return Server::CmdResponse(true, "Invalid arguments (\'id\' is missing).");
	if(!json["id"].IsInt64()) return Server::CmdResponse(true, "Invalid arguments (\'id\' is not a number).");
	unsigned long id = json["id"].GetInt64();

	// check URL list
	if(!(this->database.isUrlList(id))) {
		std::ostringstream errStrStr;
		errStrStr << "URL list #" << id << " not found.";
		Server::CmdResponse(true, errStrStr.str());
	}

	// deleteurllist needs to be confirmed
	if(!json.HasMember("confirmed"))
		return Server::CmdResponse(false, true, "Do you really want to delete this URL list?\n"
				"!!! All associated data will be lost !!!");

	// delete URL list from database
	this->database.deleteUrlList(id);

	// deleteurllist is a logged command
	std::ostringstream logStrStr;
	logStrStr << "URL list #" << id << " deleted by " << ip << ".";
	this->database.log("database", logStrStr.str());
	return Server::CmdResponse("URL list deleted.");
}

// server command addquery(website, name, query, type, resultbool, resultsingle, resultmulti, textonly): add a query
Server::CmdResponse Server::cmdAddQuery(const rapidjson::Document& json) {
	// get arguments
	if(!json.HasMember("website")) return Server::CmdResponse(true, "Invalid arguments (\'website\' is missing).");
	if(!json["website"].IsInt64()) return Server::CmdResponse(true, "Invalid arguments (\'website\' is not a number).");
	unsigned long website = json["website"].GetInt64();
	if(!json.HasMember("name")) return Server::CmdResponse(true, "Invalid arguments (\'name\' is missing).");
	if(!json["name"].IsString()) return Server::CmdResponse(true, "Invalid arguments (\'name\' is not a string).");
	std::string name = json["name"].GetString();
	if(!json.HasMember("query")) return Server::CmdResponse(true, "Invalid arguments (\'query\' is missing).");
	if(!json["query"].IsString()) return Server::CmdResponse(true, "Invalid arguments (\'query\' is not a string).");
	std::string query = json["query"].GetString();
	if(!json.HasMember("type")) return Server::CmdResponse(true, "Invalid arguments (\'type\' is missing).");
	if(!json["type"].IsString()) return Server::CmdResponse(true, "Invalid arguments (\'type\' is not a string).");
	std::string type = json["type"].GetString();
	if(!json.HasMember("resultbool")) return Server::CmdResponse(true, "Invalid arguments (\'resultbool\' is missing).");
	if(!json["resultbool"].IsBool()) return Server::CmdResponse(true, "Invalid arguments (\'resultbool\' is not a boolean).");
	bool resultBool = json["resultbool"].GetBool();
	if(!json.HasMember("resultsingle")) return Server::CmdResponse(true, "Invalid arguments (\'resultsingle\' is missing).");
	if(!json["resultsingle"].IsBool()) return Server::CmdResponse(true, "Invalid arguments (\'resultsingle\' is not a boolean).");
	bool resultSingle = json["resultsingle"].GetBool();
	if(!json.HasMember("resultmulti")) return Server::CmdResponse(true, "Invalid arguments (\'resultmulti\' is missing).");
	if(!json["resultmulti"].IsBool()) return Server::CmdResponse(true, "Invalid arguments (\'resultmulti\' is not a boolean).");
	bool resultMulti = json["resultmulti"].GetBool();
	if(!json.HasMember("textonly")) return Server::CmdResponse(true, "Invalid arguments (\'textonly\' is missing).");
	if(!json["textonly"].IsBool()) return Server::CmdResponse(true, "Invalid arguments (\'textonly\' is not a boolean).");
	bool textOnly = json["textonly"].GetBool();

	// check name
	if(!name.length()) return Server::CmdResponse(true, "Name is empty.");

	// check query text
	if(!query.length()) return Server::CmdResponse(true, "Query text is empty.");

	// check query type
	if(!type.length()) return Server::CmdResponse(true, "Query type is empty.");
	if(type != "regex" && type != "xpath") return Server::CmdResponse(true, "Unknown query type: \'" + type + "\'.");

	// check result type
	if(!resultBool && !resultSingle && !resultMulti) return Server::CmdResponse(true, "No result type selected.");

	// check website
	if(website && !(this->database.isWebsite(website))) {
		std::ostringstream errStrStr;
		errStrStr << "Website #" << website << " not found.";
		return Server::CmdResponse(true, errStrStr.str());
	}

	// add query to database
	unsigned long id = this->database.addQuery(website, name, query, type, resultBool, resultSingle, resultMulti, textOnly);
	if(!id) return Server::CmdResponse("Could not add query to database.");

	return Server::CmdResponse("Query added.", id);
}

// server command updatequery(id, name, query, type, resultbool, resultsingle, resultmulti, textonly): edit a query
Server::CmdResponse Server::cmdUpdateQuery(const rapidjson::Document& json) {
	// get arguments
	if(!json.HasMember("id")) return Server::CmdResponse(true, "Invalid arguments (\'id\' is missing).");
	if(!json["id"].IsInt64()) return Server::CmdResponse(true, "Invalid arguments (\'id\' is not a number).");
	unsigned long id = json["id"].GetInt64();
	if(!json.HasMember("name")) return Server::CmdResponse(true, "Invalid arguments (\'name\' is missing).");
	if(!json["name"].IsString()) return Server::CmdResponse(true, "Invalid arguments (\'name\' is not a string).");
	std::string name = json["name"].GetString();
	if(!json.HasMember("query")) return Server::CmdResponse(true, "Invalid arguments (\'query\' is missing).");
	if(!json["query"].IsString()) return Server::CmdResponse(true, "Invalid arguments (\'query\' is not a string).");
	std::string query = json["query"].GetString();
	if(!json.HasMember("type")) return Server::CmdResponse(true, "Invalid arguments (\'type\' is missing).");
	if(!json["type"].IsString()) return Server::CmdResponse(true, "Invalid arguments (\'type\' is not a string).");
	std::string type = json["type"].GetString();
	if(!json.HasMember("resultbool")) return Server::CmdResponse(true, "Invalid arguments (\'resultbool\' is missing).");
	if(!json["resultbool"].IsBool()) return Server::CmdResponse(true, "Invalid arguments (\'resultbool\' is not a boolean).");
	bool resultBool = json["resultbool"].GetBool();
	if(!json.HasMember("resultsingle")) return Server::CmdResponse(true, "Invalid arguments (\'resultsingle\' is missing).");
	if(!json["resultsingle"].IsBool()) return Server::CmdResponse(true, "Invalid arguments (\'resultsingle\' is not a boolean).");
	bool resultSingle = json["resultsingle"].GetBool();
	if(!json.HasMember("resultmulti")) return Server::CmdResponse(true, "Invalid arguments (\'resultmulti\' is missing).");
	if(!json["resultmulti"].IsBool()) return Server::CmdResponse(true, "Invalid arguments (\'resultmulti\' is not a boolean).");
	bool resultMulti = json["resultmulti"].GetBool();
	if(!json.HasMember("textonly")) return Server::CmdResponse(true, "Invalid arguments (\'textonly\' is missing).");
	if(!json["textonly"].IsBool()) return Server::CmdResponse(true, "Invalid arguments (\'textonly\' is not a boolean).");
	bool textOnly = json["textonly"].GetBool();

	// check name
	if(!name.length()) return Server::CmdResponse(true, "Name is empty.");

	// check query text
	if(!query.length()) return Server::CmdResponse(true, "Query text is empty.");

	// check query type
	if(!type.length()) return Server::CmdResponse(true, "Query type is empty.");
	if(type != "regex" && type != "xpath") return Server::CmdResponse(true, "Unknown query type: \'" + type + "\'.");

	// check result type
	if(!resultBool && !resultSingle && !resultMulti) return Server::CmdResponse(true, "No result type selected.");

	// check query
	if(!(this->database.isQuery(id))) {
		std::ostringstream errStrStr;
		errStrStr << "Query #" << id << " not found.";
		return Server::CmdResponse(true, errStrStr.str());
	}

	// update query in database
	this->database.updateQuery(id, name, query, type, resultBool, resultSingle, resultMulti, textOnly);

	return Server::CmdResponse("Query updated.");
}

// server command deletequery(id): delete a query from the database by its ID
Server::CmdResponse Server::cmdDeleteQuery(const rapidjson::Document& json) {
	// check whether the deletion of data is allowed
	if(!(this->settings.dataDeletable)) return Server::CmdResponse(true, "Not allowed.");

	// get argument
	if(!json.HasMember("id")) return Server::CmdResponse(true, "Invalid arguments (\'id\' is missing).");
	if(!json["id"].IsInt64()) return Server::CmdResponse(true, "Invalid arguments (\'id\' is not a number).");
	unsigned long id = json["id"].GetInt64();

	// check query
	if(!(this->database.isQuery(id))) {
		std::ostringstream errStrStr;
		errStrStr << "Query #" << id << " not found.";
		return Server::CmdResponse(true, errStrStr.str());
	}

	if(!json.HasMember("confirmed")) return Server::CmdResponse(false, true, "Do you really want to delete this query?");

	// delete URL list from database
	this->database.deleteQuery(id);

	return Server::CmdResponse("Query deleted.");
}

// server command duplicatequery(id): Duplicate a query by its ID
Server::CmdResponse Server::cmdDuplicateQuery(const rapidjson::Document& json) {
	// get argument
	if(!json.HasMember("id")) return Server::CmdResponse(true, "Invalid arguments (\'id\' is missing).");
	if(!json["id"].IsInt64()) return Server::CmdResponse(true, "Invalid arguments (\'id\' is not a number).");
	unsigned long id = json["id"].GetInt64();

	// check query
	if(!(this->database.isQuery(id))) {
		std::ostringstream errStrStr;
		errStrStr << "Query #" << id << " not found.";
		return Server::CmdResponse(true, errStrStr.str());
	}

	// duplicate query
	unsigned long newId = this->database.duplicateQuery(id);
	if(!newId) return Server::CmdResponse(true, "Could not add duplicate to database.");

	return Server::CmdResponse("Query duplicated.", newId);
}

// server command testquery(query, type, resultbool, resultsingle, resultmulti, textonly, text): test query on text
void Server::cmdTestQuery(unsigned long index, const char * body, struct mg_connection * connection) {
	Server::CmdResponse response;

	// parse JSON (again for thread)
	rapidjson::Document json;
	if(json.Parse(body).HasParseError()) response = Server::CmdResponse(true, "Could not parse JSON.");
	else {
		// get arguments
		if(!json.HasMember("query")) response = Server::CmdResponse(true, "Invalid arguments (\'query\' is missing).");
		else if(!json["query"].IsString()) response = Server::CmdResponse(true, "Invalid arguments (\'query\' is not a string).");
		else if(!json.HasMember("type")) response = Server::CmdResponse(true, "Invalid arguments (\'type\' is missing).");
		else if(!json["type"].IsString()) response = Server::CmdResponse(true, "Invalid arguments (\'type\' is not a string).");
		else if(!json.HasMember("resultbool")) response = Server::CmdResponse(true, "Invalid arguments (\'resultbool\' is missing).");
		else if(!json["resultbool"].IsBool()) response = Server::CmdResponse(true, "Invalid arguments (\'resultbool\' is not a boolean).");
		else if(!json.HasMember("resultsingle")) response = Server::CmdResponse(true, "Invalid arguments (\'resultsingle\' is missing).");
		else if(!json["resultsingle"].IsBool()) response = Server::CmdResponse(true,
				"Invalid arguments (\'resultsingle\' is not a boolean).");
		else if(!json.HasMember("resultmulti")) response = Server::CmdResponse(true, "Invalid arguments (\'resultmulti\' is missing).");
		else if(!json["resultmulti"].IsBool()) response = Server::CmdResponse(true, "Invalid arguments (\'resultmulti\' is not a boolean).");
		else if(!json.HasMember("textonly")) response = Server::CmdResponse(true, "Invalid arguments (\'textonly\' is missing).");
		else if(!json["textonly"].IsBool()) response = Server::CmdResponse(true, "Invalid arguments (\'textonly\' is not a boolean).");
		else if(!json.HasMember("text")) response = Server::CmdResponse(true, "Invalid arguments (\'text\' is missing).");
		else if(!json["text"].IsString()) response = Server::CmdResponse(true, "Invalid arguments (\'text\' is not a string).");
		else {
			std::string query = json["query"].GetString();
			std::string type = json["type"].GetString();
			bool resultBool = json["resultbool"].GetBool();
			bool resultSingle = json["resultsingle"].GetBool();
			bool resultMulti = json["resultmulti"].GetBool();
			bool textOnly = json["textonly"].GetBool();
			std::string text = json["text"].GetString();

			// check query text, query type and result type
			if(!query.length()) response = Server::CmdResponse(true, "Query text is empty.");
			else if(!type.length()) response = Server::CmdResponse(true, "Query type is empty.");
			else if(type != "regex" && type != "xpath") response = Server::CmdResponse(true, "Unknown query type: \'" + type + "\'.");
			else if(!resultBool && !resultSingle && !resultMulti) response = Server::CmdResponse(true, "No result type selected.");
			else {
				// test query
				std::string result;
				if(type == "regex") {
					// test RegEx expression on text
					RegEx regExTest;
					TimerSimpleHR timer;
					if(!regExTest.compile(query, resultBool || resultSingle, resultMulti))
						Server::CmdResponse(true, regExTest.getErrorMessage());
					result = "COMPILING TIME: " + timer.tickStr() + "\n";
					if(resultBool) {
						// get boolean result (does at least one match exist?)
						bool tempResult = false;
						if(!regExTest.getBool(text, tempResult)) response = Server::CmdResponse(true, regExTest.getErrorMessage());
						else result += "BOOLEAN RESULT (" + timer.tickStr() + "): " + std::string(tempResult ? "true" : "false") + "\n";
					}
					if(resultSingle) {
						// get first result (first full match)
						std::string tempResult;
						if(!regExTest.getFirst(text, tempResult)) response = Server::CmdResponse(true, regExTest.getErrorMessage());
						else if(tempResult.length()) result += "FIRST RESULT (" + timer.tickStr() + "): " + tempResult + "\n";
						else result += "FIRST RESULT (" + timer.tickStr() + "): [empty]\n";
					}
					if(resultMulti) {
						// get all results (all full matches)
						std::vector<std::string> tempResult;
						if(!regExTest.getAll(text, tempResult)) response = Server::CmdResponse(true, regExTest.getErrorMessage());
						else {
							result += "ALL RESULTS (" + timer.tickStr() + "):";

							if(tempResult.size()) {
								unsigned long n = 0;
								std::ostringstream resultStrStr;
								resultStrStr << '\n';
								for(auto i = tempResult.begin(); i != tempResult.end(); ++i) {
									resultStrStr << '[' << (n + 1) << "] " << tempResult.at(n) << '\n';
									n++;
								}
								result += resultStrStr.str();
							}
							else result += " [empty]\n";
						}
					}
				}
				else {
					// test XPath expression on text
					XMLDocument xmlDocumentTest;
					XPath xPathTest;
					TimerSimpleHR timer;
					if(!xPathTest.compile(query, textOnly)) response = Server::CmdResponse(true, xPathTest.getErrorMessage());
					else {
						result = "COMPILING TIME: " + timer.tickStr() + "\n";
						if(!xmlDocumentTest.parse(text)) response = Server::CmdResponse(true, xmlDocumentTest.getErrorMessage());
						else {
							result += "PARSING TIME: " + timer.tickStr() + "\n";
							if(resultBool) {
								// get boolean result (does at least one match exist?)
								bool tempResult = false;
								if(!xPathTest.getBool(xmlDocumentTest, tempResult))
									response = Server::CmdResponse(true, xPathTest.getErrorMessage());
								else result += "BOOLEAN RESULT (" + timer.tickStr() + "): " + std::string(tempResult ? "true" : "false")
									+ "\n";
							}
							if(resultSingle) {
								// get first result (first full match)
								std::string tempResult;
								TimerSimpleHR timer;
								if(!xPathTest.getFirst(xmlDocumentTest, tempResult))
									response = Server::CmdResponse(true, xPathTest.getErrorMessage());
								else if(tempResult.length()) result += "FIRST RESULT (" + timer.tickStr() + "): " + tempResult + "\n";
								else result += "FIRST RESULT (" + timer.tickStr() + "): [empty]\n";
							}
							if(resultMulti) {
								// get all results (all full matches)
								std::vector<std::string> tempResult;
								TimerSimpleHR timer;
								if(!xPathTest.getAll(xmlDocumentTest, tempResult))
									response = Server::CmdResponse(true, xPathTest.getErrorMessage());
								else {
									result += "ALL RESULTS (" + timer.tickStr() + "):";

									if(tempResult.size()) {
										unsigned long n = 0;
										std::ostringstream resultStrStr;
										resultStrStr << '\n';
										for(auto i = tempResult.begin(); i != tempResult.end(); ++i) {
											resultStrStr << '[' << (n + 1) << "] " << tempResult.at(n) << '\n';
											n++;
										}
										result += resultStrStr.str();
									}
									else result += " [empty]\n";
								}
							}
						}
					}
				}
				if(!response.fail){
					result.pop_back();
					response = Server::CmdResponse(result);
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
		reply.String(body);
	}
	reply.Key("text");
	reply.String(response.text.c_str(), response.text.size());
	reply.EndObject();
	std::string replyString = replyBuffer.GetString();

	// delete buffer for body
	delete[] body;

	// send reply
	mg_send_head(connection, 200, replyString.size(), "Content-Type: application/json\r\n"
			"Access-Control-Allow-Origin: *\r\n"
			"Access-Control-Allow-Methods: GET, POST\r\n"
			"Access-Control-Allow-Headers: Content-Type");
	mg_printf(connection, "%s", replyString.c_str());

	// set thread status to not running
	{
		std::lock_guard<std::mutex> workersLocked(this->workersLock);
		this->workersRunning.at(index) = false;
	}
}

// server command addconfig(website, module, name, config): Add configuration to database
Server::CmdResponse Server::cmdAddConfig(const rapidjson::Document& json) {
	// get arguments
	if(!json.HasMember("website")) return Server::CmdResponse(true, "Invalid arguments (\'website\' is missing).");
	if(!json["website"].IsInt64()) return Server::CmdResponse(true, "Invalid arguments (\'website\' is not a number).");
	unsigned long website = json["website"].GetInt64();
	if(!json.HasMember("module")) return Server::CmdResponse(true, "Invalid arguments (\'module\' is missing).");
	if(!json["module"].IsString()) return Server::CmdResponse(true, "Invalid arguments (\'module\' is not a string).");
	std::string module = json["module"].GetString();
	if(!json.HasMember("name")) return Server::CmdResponse(true, "Invalid arguments (\'name\' is missing).");
	if(!json["name"].IsString()) return Server::CmdResponse(true, "Invalid arguments (\'name\' is not a string).");
	std::string name = json["name"].GetString();
	if(!json.HasMember("config")) return Server::CmdResponse(true, "Invalid arguments (\'config\' is missing).");
	if(!json["config"].IsString()) return Server::CmdResponse(true, "Invalid arguments (\'config\' is not a string).");
	std::string config = json["config"].GetString();

	// check name
	if(!name.length()) return Server::CmdResponse(true, "Name is empty.");

	// check configuration JSON
	rapidjson::Document configJson;
	if(configJson.Parse(config.c_str()).HasParseError()) {
		return Server::CmdResponse(true, "Could not parse JSON.");
	}

	// check website
	if(!(this->database.isWebsite(website))) {
		std::ostringstream errStrStr;
		errStrStr << "Website #" << website << " not found.";
		return Server::CmdResponse(true, errStrStr.str());
	}

	// add configuration to database
	unsigned long id = this->database.addConfiguration(website, module, name, config);
	if(!id) return Server::CmdResponse(true, "Could not add configuration to database.");

	return Server::CmdResponse("Configuration added.", id);
}

// server command updateconfig(id, name, config): Update a configuration in the database
Server::CmdResponse Server::cmdUpdateConfig(const rapidjson::Document& json) {
	// get arguments
	if(!json.HasMember("id")) return Server::CmdResponse(true, "Invalid arguments (\'id\' is missing).");
	if(!json["id"].IsInt64()) return Server::CmdResponse(true, "Invalid arguments (\'id\' is not a number).");
	unsigned long id = json["id"].GetInt64();
	if(!json.HasMember("name")) return Server::CmdResponse(true, "Invalid arguments (\'name\' is missing).");
	if(!json["name"].IsString()) return Server::CmdResponse(true, "Invalid arguments (\'name\' is not a string).");
	std::string name = json["name"].GetString();
	if(!json.HasMember("config")) return Server::CmdResponse(true, "Invalid arguments (\'config\' is missing).");
	if(!json["config"].IsString()) return Server::CmdResponse(true, "Invalid arguments (\'config\' is not a string).");
	std::string config = json["config"].GetString();

	// check name
	if(!name.length()) return Server::CmdResponse(true, "Name is empty.");

	// check configuration JSON
	rapidjson::Document configJson;
	if(configJson.Parse(config.c_str()).HasParseError()) {
		return Server::CmdResponse(true, "Could not parse JSON.");
	}

	// check configuration
	if(!(this->database.isConfiguration(id))) {
		std::ostringstream errStrStr;
		errStrStr << "Configuration #" << id << " not found.";
		return Server::CmdResponse(true, errStrStr.str());
	}

	// update configuration in database
	this->database.updateConfiguration(id, name, config);
	return Server::CmdResponse("Configuration updated.");
}

// server command deleteconfig(id): delete a configuration from the database by its ID
Server::CmdResponse Server::cmdDeleteConfig(const rapidjson::Document& json) {
	// check whether the deletion of data is allowed
	if(!(this->settings.dataDeletable)) return Server::CmdResponse(true, "Not allowed.");

	// get argument
	if(!json.HasMember("id")) return Server::CmdResponse(true, "Invalid arguments (\'id\' is missing).");
	if(!json["id"].IsInt64()) return Server::CmdResponse(true, "Invalid arguments (\'id\' is not a number).");
	unsigned long id = json["id"].GetInt64();

	// check configuration
	if(!(this->database.isConfiguration(id))) {
		std::ostringstream errStrStr;
		errStrStr << "Configuration #" << id << " not found.";
		Server::CmdResponse(true, errStrStr.str());
	}

	// deleteconfig needs to be confirmed
	if(!json.HasMember("confirmed"))
		return Server::CmdResponse(false, true, "Do you really want to delete this configuration?");

	// delete configuration from database
	this->database.deleteConfiguration(id);

	return Server::CmdResponse("Configuration deleted.");
}

// server command duplicateconfig(id): Duplicate a configuration by its ID
Server::CmdResponse Server::cmdDuplicateConfig(const rapidjson::Document& json) {
	// get argument
	if(!json.HasMember("id")) return Server::CmdResponse(true, "Invalid arguments (\'id\' is missing).");
	if(!json["id"].IsInt64()) return Server::CmdResponse(true, "Invalid arguments (\'id\' is not a number).");
	unsigned long id = json["id"].GetInt64();

	// check configuration
	if(!(this->database.isConfiguration(id))) {
		std::ostringstream errStrStr;
		errStrStr << "Configuration #" << id << " not found.";
		return Server::CmdResponse(true, errStrStr.str());
	}

	// duplicate configuration
	unsigned long newId = this->database.duplicateConfiguration(id);
	if(!newId) return Server::CmdResponse(true, "Could not add duplicate to database.");

	return Server::CmdResponse("Configuration duplicated.", newId);
}
