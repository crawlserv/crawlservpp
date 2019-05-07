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

#include "Server.hpp"

namespace crawlservpp::Main {

	// constructor, throws Main::Exception
	Server::Server(
			const DatabaseSettings& databaseSettings,
			const ServerSettings& serverSettings
	)
			: settings(serverSettings),
			  dbSettings(databaseSettings),
			  database(databaseSettings, "server"),
			  allowed(serverSettings.allowedClients),
			  running(true),
			  offline(true),
			  dirCache(MAIN_SERVER_DIR_CACHE),
			  dirCookies(MAIN_SERVER_DIR_COOKIES),
			  webServer(this->dirCache) {
		// create cache directory if it does not exist
		if(
				!std::filesystem::is_directory(this->dirCache)
				|| !std::filesystem::exists(this->dirCache)
		)
			std::filesystem::create_directory(this->dirCache);
		else
			// directory exists: clear it
			Helper::FileSystem::clearDirectory(this->dirCache);

		// create cookies directory if it does not exist
		if(
				!std::filesystem::is_directory(this->dirCookies)
				|| !std::filesystem::exists(this->dirCookies)
		)
			std::filesystem::create_directory(this->dirCookies);

		// set database option
		this->database.setSleepOnError(MAIN_SERVER_SLEEP_ON_SQL_ERROR_SEC);

		// connect to database and initialize it
		this->database.connect();
		this->database.initializeSql();
		this->database.prepare();
		this->database.update();

		// change state to online
		this->offline = false;

		// set callbacks (suppressing wrong error messages by Eclipse IDE)
		this->webServer.setAcceptCallback( // @suppress("Invalid arguments")
				std::bind(&Server::onAccept, this, std::placeholders::_1)
		);

		this->webServer.setRequestCallback( // @suppress("Invalid arguments")
				std::bind(
						&Server::onRequest,
						this,
						std::placeholders::_1,
						std::placeholders::_2,
						std::placeholders::_3,
						std::placeholders::_4
				)
		);

		// initialize mongoose embedded web server, bind it to port and set CORS string
		this->webServer.initHTTP(serverSettings.port);

		this->webServer.setCORS(serverSettings.corsOrigins);

		// set initial status
		this->setStatus("crawlserv is ready");

		// load threads from database
		std::vector<ThreadDatabaseEntry> threads = this->database.getThreads();

		for(auto i = threads.begin(); i != threads.end(); ++i) {
			if(i->options.module == "crawler") {
				// load crawler thread
				this->crawlers.push_back(std::make_unique<Module::Crawler::Thread>(
						this->database, this->dirCookies, i->options, i->status
				));

				this->crawlers.back()->Module::Thread::start();

				// write to log
				std::ostringstream logStrStr;

				logStrStr << "crawler #" << i->status.id << " continued.";

				this->database.log(logStrStr.str());
			}
			else if(i->options.module == "parser") {
				// load parser thread
				this->parsers.push_back(std::make_unique<Module::Parser::Thread>(
						this->database, i->options, i->status
				));

				this->parsers.back()->Module::Thread::start();

				// write to log
				std::ostringstream logStrStr;

				logStrStr << "parser #" << i->status.id << " continued.";

				this->database.log(logStrStr.str());
			}
			else if(i->options.module == "extractor") {
				// TODO: load extractor thread
				/*
				this->extractors.push_back(std::make_unique<Module::Extractor::Thread>(
						this->database, i->options, i->status
				));

				this->extractors.back()->Module::Thread::start();
				*/

				// write to log
				std::ostringstream logStrStr;

				logStrStr << "extractor #" << i->status.id << " continued.";

				this->database.log(logStrStr.str());
			}
			else if(i->options.module == "analyzer") {
				// get JSON
				const std::string config = this->database.getConfiguration(i->options.config);

				// parse JSON
				rapidjson::Document configJson;

				try {
					configJson = Helper::Json::parseRapid(config);
				}
				catch(const JsonException& e) {
					throw Exception("Could not parse configuration: " + e.whatStr());
				}

				if(!configJson.IsArray())
					throw Exception("Parsed configuration JSON is not an array.");

				// try to add algorithm according to parsed algorithm ID
				this->analyzers.push_back(
						Module::Analyzer::Algo::initAlgo(AlgoThreadProperties(
								Server::getAlgoFromConfig(configJson),
								this->database,
								i->options,
								i->status
						))
				);

				if(!(this->analyzers.back())) {
					this->analyzers.pop_back();

					this->database.log("[WARNING] Unknown algorithm ignored when loading threads.");

					continue;
				}

				// start algorithm (and get its ID)
				this->analyzers.back()->Module::Thread::start();


				// write to log
				std::ostringstream logStrStr;

				logStrStr << "analyzer #" << i->status.id << " continued.";

				this->database.log(logStrStr.str());
			}
			else
				throw Exception("Unknown thread module \'" + i->options.module + "\'");
		}

		// save start time for up-time calculation
		this->uptimeStart = std::chrono::steady_clock::now();

		// start logging
		std::ostringstream logStrStr;

		logStrStr	<< "started. [MySQL v"
					<< this->database.getMysqlVersion()
					<< "; datadir=\'"
					<< this->database.getDataDir()
					<< "\'; maxAllowedPacketSize="
					<< this->database.getMaxAllowedPacketSize()
					<< "]";

		this->database.log(logStrStr.str());
	}

	// destructor
	Server::~Server() {
		// interrupt module threads
		for(auto i = this->crawlers.begin(); i != this->crawlers.end(); ++i)
			(*i)->Module::Thread::interrupt();

		for(auto i = this->parsers.begin(); i != this->parsers.end(); ++i)
			(*i)->Module::Thread::interrupt();

		//for(auto i = this->extractors.begin(); i != this->extractors.end(); ++i)
		//	(*i)->Module::Thread::interrupt();

		for(auto i = this->analyzers.begin(); i != this->analyzers.end(); ++i)
			(*i)->Module::Thread::interrupt();

		// wait for module threads
		for(auto i = this->crawlers.begin(); i != this->crawlers.end(); ++i) {
			if(*i) {
				// get thread ID (for logging)
				const unsigned long id = (*i)->getId();

				// wait for thread
				(*i)->Module::Thread::end();

				// log interruption
				std::ostringstream logStrStr;

				logStrStr << "crawler #" << id << " interrupted.";

				try {
					this->database.log(logStrStr.str());
				}
				catch(const Database::Exception& e) {
					std::cout	<< '\n' << logStrStr.str()
								<< "\nCould not write to log: " << e.what() << std::flush;
				}
				catch(...) {
					std::cout	<< '\n' << logStrStr.str()
								<< "\nERROR: Unknown exception in Server::~Server()" << std::flush;
				}
			}
		}

		this->crawlers.clear();

		for(auto i = this->parsers.begin(); i != this->parsers.end(); ++i) {
			if(*i) {
				// get thread ID (for logging)
				const unsigned long id = (*i)->getId();

				// wait for thread
				(*i)->Module::Thread::end();

				// log interruption
				std::ostringstream logStrStr;

				logStrStr << "parser #" << id << " interrupted.";

				try {
					this->database.log(logStrStr.str());
				}
				catch(const Database::Exception& e) {
					std::cout	<< '\n' << logStrStr.str()
								<< "\nCould not write to log: " << e.what() << std::flush;
				}
				catch(...) {
					std::cout	<< '\n' << logStrStr.str()
								<< "\nERROR: Unknown exception in Server::~Server()" << std::flush;
				}
			}
		}

		this->parsers.clear();

		/*for(auto i = this->extractors.begin(); i != this->extractors.end(); ++i) {
			if(*i) {
				// get thread ID (for logging)
				const unsigned long id = (*i)->getId();

				// wait for thread
				(*i)->Module::Thread::end();

				// log interruption
				std::ostringstream logStrStr;

				logStrStr << "extractor #" << id << " interrupted.";

				try {
					this->database.log(logStrStr.str());
				}
				catch(const Database::Exception& e) {
					std::cout	<< '\n' << logStrStr.str()
								<< "\nCould not write to log: " << e.what() << std::flush;
				}
				catch(...) {
					std::cout	<< '\n' << logStrStr.str()
								<< "\nERROR: Unknown exception in Server::~Server()" << std::flush;
				}
			}
		}

		this->extractors.clear();*/

		for(auto i = this->analyzers.begin(); i != this->analyzers.end(); ++i) {
			if(*i) {
				// get thread ID (for logging)
				const unsigned long id = (*i)->getId();

				// wait for thread
				(*i)->Module::Thread::end();

				// log interruption
				std::ostringstream logStrStr;

				logStrStr << "analyzer #" << id << " interrupted.";

				try {
					this->database.log(logStrStr.str());
				}
				catch(const Database::Exception& e) {
					std::cout	<< '\n' << logStrStr.str()
								<< "\nCould not write to log: " << e.what() << std::flush;
				}
				catch(...) {
					std::cout	<< '\n' << logStrStr.str()
								<< "\nERROR: Unknown exception in Server::~Server()" << std::flush;
				}
			}
		}

		this->analyzers.clear();

		// wait for worker threads
		for(auto i = this->workers.begin(); i != this->workers.end(); ++i)
			if(i->joinable())
				i->join();

		this->workers.clear();

		// log shutdown message with server up-time
		try {
			this->database.log("shuts down after up-time of "
					+ Helper::DateTime::secondsToString(this->getUpTime()) + ".");
		}
		catch(const Database::Exception& e) {
			std::cout << "server shuts down after up-time of"
					<< Helper::DateTime::secondsToString(this->getUpTime()) << "."
					<< "\nCould not write to log: " << e.what() << std::flush;
		}
		catch(...) {
			std::cout << "server shuts down after up-time of"
					<< Helper::DateTime::secondsToString(this->getUpTime()) << "."
					<< "\nERROR: Unknown exception in Server::~Server()" << std::flush;
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

		// check whether a thread was shut down and the shutdown is finished
		for(unsigned long n = 1; n <= this->crawlers.size(); ++n) {
			if(this->crawlers.at(n - 1)->isShutdown() && this->crawlers.at(n - 1)->isFinished()) {
				--n;

				this->crawlers.at(n)->Module::Thread::end();

				this->crawlers.erase(this->crawlers.begin() + n);
			}
		}

		for(unsigned long n = 1; n <= this->parsers.size(); ++n) {
			if(this->parsers.at(n - 1)->isShutdown() && this->parsers.at(n - 1)->isFinished()) {
				--n;

				this->parsers.at(n)->Module::Thread::end();

				this->parsers.erase(this->parsers.begin() + n);
			}
		}

		/*for(unsigned long n = 1; n <= this->extractors.size(); ++n) {
			if(this->extractors.at(n - 1)->isShutdown() && this->extractors.at(n - 1)->isFinished()) {
				--n;

				this->extractors.at(n)->Module::Thread::end();

				this->extractors.erase(this->extractors.begin() + n);
			}
		}*/

		for(unsigned long n = 1; n <= this->analyzers.size(); ++n) {
			if(this->analyzers.at(n - 1)->isShutdown() && this->analyzers.at(n - 1)->isFinished()) {
				--n;

				this->analyzers.at(n)->Module::Thread::end();

				this->analyzers.erase(this->analyzers.begin() + n);
			}
		}

		// check whether worker threads were terminated
		if(!(this->workers.empty())) {
			std::lock_guard<std::mutex> workersLocked(this->workersLock);

			for(unsigned long n = 1; n <= this->workers.size(); ++n) {
				if(!(this->workersRunning.at(n - 1))) {
					// join and remove thread
					--n;

					std::thread& worker = this->workers.at(n);

					if(worker.joinable())
						worker.join();

					this->workers.erase(this->workers.begin() + n);

					this->workersRunning.erase(this->workersRunning.begin() + n);
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
		return std::chrono::duration_cast<std::chrono::seconds>(
				std::chrono::steady_clock::now() - this->uptimeStart
		).count();
	}

	// get number of active module threads
	unsigned long Server::getActiveThreads() const {
		unsigned long result = 0;

		// count active crawlers
		result += std::count_if(
				this->crawlers.begin(),
				this->crawlers.end(),
				[](const auto& thread) {
						return thread->Module::Thread::isRunning();
				}
		);

		// count active parsers
		result += std::count_if(
				this->parsers.begin(),
				this->parsers.end(),
				[](const auto& thread) {
						return thread->Module::Thread::isRunning();
				}
		);

		/* TODO: count extractors
		// count active extractors
		result += std::count_if(
				this->extractors.begin(),
				this->extractors.end(),
				[](const auto& thread) {
						return thread->Module::Thread::isRunning();
				}
		);
		*/

		// count active analyzers
		result += std::count_if(
				this->analyzers.begin(),
				this->analyzers.end(),
				[](const auto& thread) {
						return thread->Module::Thread::isRunning();
				}
		);

		return result;
	}

	// get number of active worker threads
	unsigned long Server::getActiveWorkers() const {
		std::lock_guard<std::mutex> workersLocked(this->workersLock);

		return std::count(this->workersRunning.begin(), this->workersRunning.end(), true);
	}

	// perform a server command, throws Main::Exception
	std::string Server::cmd(
			ConnectionPtr connection,
			const std::string& msgBody,
			bool& threadStartedTo,
			bool& fileDownloadTo
	) {
		ServerCommandResponse response;

		// set default values
		threadStartedTo = false;
		fileDownloadTo = false;

		if(this->offline) {
			// database offline: return error
			response = ServerCommandResponse::failed("Database is offline.");
		}
		else {
			// check connection and get IP
			if(!connection)
				throw Exception("Server::cmd(): No connection specified");

			const std::string ip(WebServer::getIP(connection));

			// parse JSON
			rapidjson::Document json;

			try {
				json = Helper::Json::parseRapid(msgBody);

				if(!json.IsObject())
					response = ServerCommandResponse::failed("Parsed JSON is not an object.");
			}
			catch(const JsonException& e) {
				response = ServerCommandResponse::failed("Could not parse JSON: " + e.whatStr() + ".");
			}

			if(!response.fail){
				// get server command
				if(json.HasMember("cmd")) {
					if(json["cmd"].IsString()) {
						try {
							// handle server commands
							const std::string command(json["cmd"].GetString(), json["cmd"].GetStringLength());

							if(command == "kill")
								response = this->cmdKill(json, ip);
							else if(command == "allow")
								response = this->cmdAllow(json, ip);
							else if(command == "disallow")
								response = this->cmdDisallow(ip);

							else if(command == "log")
								response = this->cmdLog(json);
							else if(command == "clearlogs")
								response = this->cmdClearLog(json, ip);

							else if(command == "startcrawler")
								response = this->cmdStartCrawler(json, ip);
							else if(command == "pausecrawler")
								response = this->cmdPauseCrawler(json, ip);
							else if(command == "unpausecrawler")
								response = this->cmdUnpauseCrawler(json, ip);
							else if(command == "stopcrawler")
								response = this->cmdStopCrawler(json, ip);

							else if(command == "startparser")
								response = this->cmdStartParser(json, ip);
							else if(command == "pauseparser")
								response = this->cmdPauseParser(json, ip);
							else if(command == "unpauseparser")
								response = this->cmdUnpauseParser(json, ip);
							else if(command == "stopparser")
								response = this->cmdStopParser(json, ip);
							else if(command == "resetparsingstatus")
								response = this->cmdResetParsingStatus(json);

							//TODO: server commands for extractor
							else if(command == "resetextractingstatus")
								response = this->cmdResetExtractingStatus(json);

							else if(command == "startanalyzer")
								response = this->cmdStartAnalyzer(json, ip);
							else if(command == "pauseanalyzer")
								response = this->cmdPauseAnalyzer(json, ip);
							else if(command == "unpauseanalyzer")
								response = this->cmdUnpauseAnalyzer(json, ip);
							else if(command == "stopanalyzer")
								response = this->cmdStopAnalyzer(json, ip);
							else if(command == "resetanalyzingstatus")
								response = this->cmdResetAnalyzingStatus(json);

							else if(command == "addwebsite")
								response = this->cmdAddWebsite(json);
							else if(command == "updatewebsite")
								response = this->cmdUpdateWebsite(json);
							else if(command == "deletewebsite")
								response = this->cmdDeleteWebsite(json, ip);
							else if(command == "duplicatewebsite")
								response = this->cmdDuplicateWebsite(json);

							else if(command == "addurllist")
								response = this->cmdAddUrlList(json);
							else if(command == "updateurllist")
								response = this->cmdUpdateUrlList(json);
							else if(command == "deleteurllist")
								response = this->cmdDeleteUrlList(json, ip);

							else if(command == "import") {
								// create a worker thread for importing (to not block the server)
								{
									std::lock_guard<std::mutex> workersLocked(this->workersLock);

									this->workersRunning.push_back(true);

									this->workers.emplace_back(
											&Server::cmdImport,
											this,
											connection,
											this->workers.size(),
											msgBody
									);
								}

								threadStartedTo = true;
							}
							else if(command == "merge") {
								// create a worker thread for merging (to not block the server)
								{
									std::lock_guard<std::mutex> workersLocked(this->workersLock);

									this->workersRunning.push_back(true);

									this->workers.emplace_back(
											&Server::cmdMerge,
											this, connection,
											this->workers.size(),
											msgBody
									);
								}

								threadStartedTo = true;
							}
							else if(command == "export") {
								// create a worker thread for exporting (to not block the server)
								{
									std::lock_guard<std::mutex> workersLocked(this->workersLock);

									this->workersRunning.push_back(true);

									this->workers.emplace_back(
											&Server::cmdExport,
											this,
											connection,
											this->workers.size(),
											msgBody
									);
								}

								threadStartedTo = true;
							}

							else if(command == "addquery")
								response = this->cmdAddQuery(json);
							else if(command == "updatequery")
								response = this->cmdUpdateQuery(json);
							else if(command == "deletequery")
								response = this->cmdDeleteQuery(json);
							else if(command == "duplicatequery")
								response = this->cmdDuplicateQuery(json);

							else if(command == "testquery") {
								// create a worker thread for testing query (to not block the server)
								{
									std::lock_guard<std::mutex> workersLocked(this->workersLock);

									this->workersRunning.push_back(true);

									this->workers.emplace_back(
											&Server::cmdTestQuery,
											this, connection,
											this->workers.size(),
											msgBody
									);
								}

								threadStartedTo = true;
							}

							else if(command == "addconfig")
								response = this->cmdAddConfig(json);
							else if(command == "updateconfig")
								response = this->cmdUpdateConfig(json);
							else if(command == "deleteconfig")
								response = this->cmdDeleteConfig(json);
							else if(command == "duplicateconfig")
								response = this->cmdDuplicateConfig(json);

							else if(command == "warp")
								response = this->cmdWarp(json);

							else if(command == "download") {
								response = this->cmdDownload(json);

								if(!response.fail) {
									fileDownloadTo = true;

									return response.text;
								}
							}

							else if(!command.empty())
								// unknown command: debug the command and its arguments
								response = ServerCommandResponse::failed("Unknown command \'" + command + "\'.");

							else
								response = ServerCommandResponse::failed("Empty command.");
						}
						catch(const std::exception &e) {
							// exceptions caused by server commands should not kill the server
							//  (and are attributed to the frontend)
							this->database.log("frontend", e.what());

							response = response = ServerCommandResponse::failed(e.what());
						}
					}
					else
						response = ServerCommandResponse::failed("Invalid command: Name is not a string.");
				}
				else
					response = ServerCommandResponse::failed("No command specified.");
			}
		}

		// return the reply
		return Server::generateReply(response, msgBody);
	}

	// set status of the server
	void Server::setStatus(const std::string& statusMsg) {
		this->status = statusMsg;
	}

	// handle accept event, throws Main::Exception
	void Server::onAccept(ConnectionPtr connection) {
		// check connection and get IP
		if(!connection)
			throw Exception("Server::onAccept(): No connection specified");

		const std::string ip(WebServer::getIP(connection));

		// check authorization
		if(this->allowed != "*") {
			if(this->allowed.find(ip) == std::string::npos) {
				WebServer::close(connection);

				if(this->offline)
					std::cout << "\nserver rejected client " + ip + "." << std::flush;
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
							std::cout << "\nserver rejected client " + ip + "." << std::flush;

							this->offline = true;
						}
					}
				}
			}
			else {
				if(this->offline)
					std::cout << "\nserver accepted client " + ip + "." << std::flush;
				else
					try {
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
							std::cout << "\nserver rejected client " + ip + "." << std::flush;

							this->offline = true;
						}
					}
			}
		}
	}

	// handle request event, throws Main::Exception
	void Server::onRequest(
			ConnectionPtr connection,
			const std::string& method,
			const std::string& body,
			void * data
	) {
		// check connection and get IP
		if(!connection)
			throw Exception("Server::onRequest(): No connection specified");

		const std::string ip(WebServer::getIP(connection));

		// check authorization
		if(this->allowed != "*") {
			if(this->allowed.find(ip) == std::string::npos) {
				this->database.log("Client " + ip + " rejected.");

				WebServer::close(connection);
			}
		}

		// check for GET request
		if(method == "GET")
			this->webServer.send(connection, 200, "text/plain", this->getStatus());
		// check for POST request
		else if(method == "POST") {
			// parse JSON
			bool threadStarted = false;
			bool fileDownload = false;

			const std::string reply(this->cmd(connection, body, threadStarted, fileDownload));

			// send reply
			if(fileDownload)
				this->webServer.sendFile(connection, reply, data);
			else if(!threadStarted)
				this->webServer.send(connection, 200, "application/json", reply);
		}
		else if(method == "OPTIONS")
			this->webServer.send(connection, 200, "", "");
	}

	// static helper function: get algorithm ID from configuration JSON
	unsigned int Server::getAlgoFromConfig(const rapidjson::Document& json) {
		unsigned int result = 0;

		// go through all array items i.e. configuration entries
		for(auto i = json.Begin(); i != json.End(); ++i) {
			bool algoItem = false;

			if(i->IsObject()) {
				// go through all item properties
				for(auto j = i->MemberBegin(); j != i->MemberEnd(); ++j) {
					if(j->name.IsString()) {
						const std::string itemName(j->name.GetString(), j->name.GetStringLength());

						if(itemName == "name") {
							if(j->value.IsString()) {
								if(std::string(j->value.GetString(), j->value.GetStringLength()) == "_algo") {
									algoItem = true;

									if(result)
										break;
								}
								else
									break;
							}
						}
						else if(itemName == "value") {
							if(j->value.IsUint()) {
								result = j->value.GetUint();

								if(algoItem)
									break;
							}
						}
					}
				}

				if(algoItem)
					break;
				else
					result = 0;
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
			return ServerCommandResponse("Bye bye.");
		}

		return ServerCommandResponse::toBeConfirmed("Are you sure to kill the server?");
	}

	// server command allow(ip): allow acces for the specified IP(s)
	Server::ServerCommandResponse Server::cmdAllow(const rapidjson::Document& json,
			const std::string& ip) {
		// get argument
		if(!json.HasMember("ip"))
			return ServerCommandResponse::failed("Invalid arguments (\'ip\' is missing).");

		if(!json["ip"].IsString())
			return ServerCommandResponse::failed("Invalid arguments (\'ip\' is not a string).");

		const std::string toAllow(json["ip"].GetString(), json["ip"].GetStringLength());

		if(toAllow.empty())
			return ServerCommandResponse::failed("Invalid arguments (\'ip\' is empty).");

		// allow needs to be confirmed
		if(!json.HasMember("confirmed"))
			return ServerCommandResponse::toBeConfirmed(
					"Do you really want to allow " + toAllow + " access to the server?"
			);

		// add IP(s)
		this->allowed += "," + toAllow;

		// allow is a logged command
		this->database.log(toAllow + " allowed by " + ip + ".");

		return ServerCommandResponse("Allowed IPs: " + this->allowed + ".");
	}

	// server command disallow: revoke access from all except the initial IP(s) specified by the configuration file
	Server::ServerCommandResponse Server::cmdDisallow(
			const std::string& ip
	) {
		// reset alled IP(s)
		this->allowed = this->settings.allowedClients;

		// disallow is a logged command
		this->database.log("Allowed IPs reset by " + ip + ".");

		return ServerCommandResponse("Allowed IP(s): " + this->allowed + ".");
	}

	// server command log(entry): write a log entry by the frontend into the database
	Server::ServerCommandResponse Server::cmdLog(const rapidjson::Document& json) {
		// get argument
		if(!json.HasMember("entry"))
			return ServerCommandResponse::failed("Invalid arguments (\'entry\' is missing).");

		if(!json["entry"].IsString())
			return ServerCommandResponse::failed("Invalid arguments (\'entry\' is not a string).");

		const std::string entry(json["entry"].GetString(), json["entry"].GetStringLength());

		// write log entry
		this->database.log("frontend", entry);

		return ServerCommandResponse("Wrote log entry: " + entry);
	}

	// server command clearlog([module]):	remove all log entries or the log entries of a specific module
	// 										(or all log entries when module is empty/not given)
	Server::ServerCommandResponse Server::cmdClearLog(
			const rapidjson::Document& json,
			const std::string& ip
		) {
		// check whether the clearing of logs is allowed
		if(!(this->settings.logsDeletable))
			return ServerCommandResponse::failed("Not allowed.");

		// get argument
		std::string module;

		if(json.HasMember("module") && json["module"].IsString())
			module = std::string(json["module"].GetString(), json["module"].GetStringLength());

		// clearlog needs to be confirmed
		if(!json.HasMember("confirmed")) {
			std::ostringstream replyStrStr;

			replyStrStr << "Are you sure to delete ";

			const unsigned long num = this->database.getNumberOfLogEntries(module);

			switch(num) {
			case 0:
				return ServerCommandResponse("No log entries to delete.");

			case 1:
				replyStrStr << "one log entry";
				break;

			default:
				replyStrStr.imbue(std::locale(""));

				replyStrStr << num << " log entries";
			}

			replyStrStr << "?";

			return ServerCommandResponse::toBeConfirmed(replyStrStr.str());
		}

		this->database.clearLogs(module);

		// clearlog is a logged command
		if(!module.empty()) {
			this->database.log("Logs of " + module + " cleared by " + ip + ".");

			return ServerCommandResponse("Logs of " + module + " cleared.");
		}

		this->database.log("All logs cleared by " + ip + ".");

		return ServerCommandResponse("All logs cleared.");
	}

	// server command startcrawler(website, urllist, config): start a crawler using the specified website, URL list and configuration
	Server::ServerCommandResponse Server::cmdStartCrawler(
			const rapidjson::Document& json,
			const std::string& ip
	) {
		// get arguments
		if(!json.HasMember("website"))
			return ServerCommandResponse::failed("Invalid arguments (\'website\' is missing).");

		if(!json["website"].IsUint64())
			return ServerCommandResponse::failed("Invalid arguments (\'website\' is not a valid number).");

		if(!json.HasMember("urllist"))
			return ServerCommandResponse::failed("Invalid arguments (\'urllist\' is missing).");

		if(!json["urllist"].IsUint64())
			return ServerCommandResponse::failed("Invalid arguments (\'urllist\' is not a valid number).");

		if(!json.HasMember("config"))
			return ServerCommandResponse::failed("Invalid arguments (\'config\' is missing).");

		if(!json["config"].IsUint64())
			return ServerCommandResponse::failed("Invalid arguments (\'config\' is not a valid number).");

		const ThreadOptions options(
				"crawler",
				json["website"].GetUint64(),
				json["urllist"].GetUint64(),
				json["config"].GetUint64()
		);

		// check arguments
		if(!(this->database.isWebsite(options.website))) {
			std::ostringstream errStrStr;

			errStrStr << "Website #" << options.website << " not found.";

			return ServerCommandResponse::failed(errStrStr.str());
		}

		if(!(this->database.isUrlList(options.website, options.urlList))) {
			std::ostringstream errStrStr;

			errStrStr << "URL list #" << options.urlList << " for website #" << options.website << " not found.";

			return ServerCommandResponse::failed(errStrStr.str());
		}

		if(!(this->database.isConfiguration(options.website, options.config))) {
			std::ostringstream errStrStr;

			errStrStr << "Configuration #" << options.config << " for website #" << options.website << " not found.";

			return ServerCommandResponse::failed(errStrStr.str());
		}

		// create crawler
		this->crawlers.push_back(
				std::make_unique<Module::Crawler::Thread>(
						this->database,
						this->dirCookies,
						options
				)
		);

		// start crawler
		this->crawlers.back()->Module::Thread::start();

		unsigned long id = this->crawlers.back()->Module::Thread::getId();

		// startcrawler is a logged command
		std::ostringstream logStrStr;

		logStrStr << "crawler #" << id << " started by " << ip << ".";

		this->database.log(logStrStr.str());

		return ServerCommandResponse("Crawler has been started.");
	}

	// server command pausecrawler(id): pause a crawler by its ID
	Server::ServerCommandResponse Server::cmdPauseCrawler(
			const rapidjson::Document& json,
			const std::string& ip
	) {
		// get argument
		if(!json.HasMember("id"))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is missing).");

		if(!json["id"].IsUint64())
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is not a valid number).");

		unsigned long id = json["id"].GetUint64();

		// find crawler
		auto i = std::find_if(this->crawlers.begin(), this->crawlers.end(),
				[&id](const auto& p) {
					return p->Module::Thread::getId() == id;
				}
		);

		if(i == this->crawlers.end()) {
			std::ostringstream errStrStr;

			errStrStr << "Could not find crawler #" << id << ".";

			return ServerCommandResponse::failed(errStrStr.str());
		}

		// pause crawler
		(*i)->Module::Thread::pause();

		// pausecrawler is a logged command
		std::ostringstream logStrStr;

		logStrStr << "crawler #" << id << " paused by " << ip << ".";

		this->database.log(logStrStr.str());

		return ServerCommandResponse("Crawler is pausing.");
	}

	// server command unpausecrawler(id): unpause a crawler by its ID
	Server::ServerCommandResponse Server::cmdUnpauseCrawler(
			const rapidjson::Document& json,
			const std::string& ip
	) {
		// get argument
		if(!json.HasMember("id"))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is missing).");

		if(!json["id"].IsUint64())
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is not a valid number).");

		unsigned long id = json["id"].GetUint64();

		// find crawler
		auto i = std::find_if(this->crawlers.begin(), this->crawlers.end(),
				[&id](const auto& p) {
					return p->Module::Thread::getId() == id;
				}
		);

		if(i == this->crawlers.end()) {
			std::ostringstream errStrStr;

			errStrStr << "Could not find crawler #" << id << ".";

			return ServerCommandResponse::failed(errStrStr.str());
		}

		// unpause crawler
		(*i)->Module::Thread::unpause();

		// unpausecrawler is a logged command
		std::ostringstream logStrStr;

		logStrStr << "crawler #" << id << " unpaused by " << ip << ".";

		this->database.log(logStrStr.str());

		return ServerCommandResponse("Crawler is unpausing.");
	}

	// server command stopcrawler(id): stop a crawler by its ID
	Server::ServerCommandResponse Server::cmdStopCrawler(
			const rapidjson::Document& json,
			const std::string& ip
	) {
		// get argument
		if(!json.HasMember("id"))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is missing).");

		if(!json["id"].IsUint64())
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is not a valid number).");

		unsigned long id = json["id"].GetUint64();

		// find crawler
		auto i = std::find_if(this->crawlers.begin(), this->crawlers.end(),
				[&id](const auto& p) {
					return p->Module::Thread::getId() == id;
				}
		);

		if(i == this->crawlers.end()) {
			std::ostringstream errStrStr;

			errStrStr << "Could not find crawler #" << id << ".";

			return ServerCommandResponse::failed(errStrStr.str());
		}

		// interrupt crawler
		(*i)->Module::Thread::stop();

		// stopcrawler is a logged command
		std::ostringstream logStrStr;

		logStrStr << "crawler #" << id << " stopped by " << ip << ".";

		this->database.log(logStrStr.str());

		return ServerCommandResponse("Crawler is stopping.");
	}

	// server command startparser(website, urllist, config): start a parser using the specified website, URL list and configuration
	Server::ServerCommandResponse Server::cmdStartParser(
			const rapidjson::Document& json,
			const std::string& ip
	) {
		// get arguments
		if(!json.HasMember("website"))
			return ServerCommandResponse::failed("Invalid arguments (\'website\' is missing).");

		if(!json["website"].IsUint64())
			return ServerCommandResponse::failed("Invalid arguments (\'website\' is not a valid number).");

		if(!json.HasMember("urllist"))
			return ServerCommandResponse::failed("Invalid arguments (\'urllist\' is missing).");

		if(!json["urllist"].IsUint64())
			return ServerCommandResponse::failed("Invalid arguments (\'urllist\' is not a valid number).");

		if(!json.HasMember("config"))
			return ServerCommandResponse::failed("Invalid arguments (\'config\' is missing).");

		if(!json["config"].IsUint64())
			return ServerCommandResponse::failed("Invalid arguments (\'config\' is not a valid number).");

		const ThreadOptions options(
				"parser",
				json["website"].GetUint64(),
				json["urllist"].GetUint64(),
				json["config"].GetUint64()
		);

		// check arguments
		if(!(this->database.isWebsite(options.website))) {
			std::ostringstream errStrStr;

			errStrStr << "Website #" << options.website << " not found.";

			return ServerCommandResponse::failed(errStrStr.str());
		}

		if(!(this->database.isUrlList(options.website, options.urlList))) {
			std::ostringstream errStrStr;

			errStrStr << "URL list #" << options.urlList << " for website #" << options.website << " not found.";

			return ServerCommandResponse::failed(errStrStr.str());
		}

		if(!(this->database.isConfiguration(options.website, options.config))) {
			std::ostringstream errStrStr;

			errStrStr << "Configuration #" << options.config << " for website #" << options.website << " not found.";

			return ServerCommandResponse::failed(errStrStr.str());
		}

		// create parser
		this->parsers.push_back(std::make_unique<Module::Parser::Thread>(this->database, options));

		// start parser
		this->parsers.back()->Module::Thread::start();

		const unsigned long id = this->parsers.back()->Module::Thread::getId();

		// startparser is a logged command
		std::ostringstream logStrStr;

		logStrStr << "parser #" << id << " started by " << ip << ".";

		this->database.log(logStrStr.str());

		return ServerCommandResponse("Parser has been started.");
	}

	// server command pauseparser(id): pause a parser by its ID
	Server::ServerCommandResponse Server::cmdPauseParser(
			const rapidjson::Document& json,
			const std::string& ip
	) {
		// get argument
		if(!json.HasMember("id"))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is missing).");

		if(!json["id"].IsUint64())
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is not a valid number).");

		const unsigned long id = json["id"].GetUint64();

		// find parser
		auto i = std::find_if(this->parsers.begin(), this->parsers.end(),
				[&id](const auto& p) {
					return p->Module::Thread::getId() == id;
				}
		);

		if(i == this->parsers.end()) {
			std::ostringstream errStrStr;

			errStrStr << "Could not find parser #" << id << ".";

			return ServerCommandResponse::failed(errStrStr.str());
		}

		// pause parser
		(*i)->Module::Thread::pause();

		// pauseparser is a logged command
		std::ostringstream logStrStr;

		logStrStr << "parser #" << id << " paused by " << ip << ".";

		this->database.log(logStrStr.str());

		return ServerCommandResponse("Parser is pausing.");
	}

	// server command unpauseparser(id): unpause a parser by its ID
	Server::ServerCommandResponse Server::cmdUnpauseParser(
			const rapidjson::Document& json,
			const std::string& ip
	) {
		// get argument
		if(!json.HasMember("id"))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is missing).");

		if(!json["id"].IsUint64())
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is not a valid number).");

		const unsigned long id = json["id"].GetUint64();

		// find parser
		auto i = std::find_if(this->parsers.begin(), this->parsers.end(),
				[&id](const auto& p) {
					return p->Module::Thread::getId() == id;
				}
		);

		if(i == this->parsers.end()) {
			std::ostringstream errStrStr;

			errStrStr << "Could not find parser #" << id << ".";

			return ServerCommandResponse::failed(errStrStr.str());
		}

		// unpause parser
		(*i)->Module::Thread::unpause();

		// unpauseparser is a logged command
		std::ostringstream logStrStr;

		logStrStr << "parser #" << id << " unpaused by " << ip << ".";

		this->database.log(logStrStr.str());

		return ServerCommandResponse("Parser is unpausing.");
	}

	// server command stopparser(id): stop a parser by its ID
	Server::ServerCommandResponse Server::cmdStopParser(
			const rapidjson::Document& json,
			const std::string& ip
	) {
		// get argument
		if(!json.HasMember("id"))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is missing).");

		if(!json["id"].IsUint64())
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is not a valid number).");

		const unsigned long id = json["id"].GetUint64();

		// find parser
		auto i = std::find_if(this->parsers.begin(), this->parsers.end(),
				[&id](const auto& p) {
					return p->Module::Thread::getId() == id;
				}
		);

		if(i == this->parsers.end()) {
			std::ostringstream errStrStr;

			errStrStr << "Could not find parser #" << id << ".";

			return ServerCommandResponse::failed(errStrStr.str());
		}

		// interrupt parser
		(*i)->Module::Thread::stop();

		// stopparser is a logged command
		std::ostringstream logStrStr;

		logStrStr << "parser #" << id << " stopped by " << ip << ".";

		this->database.log(logStrStr.str());

		return ServerCommandResponse("Parser is stopping.");
	}

	// server command resetparsingstatus(urllist): reset the parsing status of a ID-specificed URL list
	Server::ServerCommandResponse Server::cmdResetParsingStatus(const rapidjson::Document& json) {
		// get argument
		if(!json.HasMember("urllist"))
			return ServerCommandResponse::failed("Invalid arguments (\'urllist\' is missing).");

		if(!json["urllist"].IsUint64())
			return ServerCommandResponse::failed("Invalid arguments (\'urllist\' is not a valid number).");

		const unsigned long listId = json["urllist"].GetUint64();

		// resetparsingstatus needs to be confirmed
		if(!json.HasMember("confirmed"))
			return ServerCommandResponse::toBeConfirmed(
					"Are you sure that you want to reset the parsing status of this URL list?"
			);

		// reset parsing status
		this->database.resetParsingStatus(listId);

		return ServerCommandResponse("Parsing status reset.");
	}

	// server command resetextractingstatus(urllist): reset the parsing status of a ID-specificed URL list
	Server::ServerCommandResponse Server::cmdResetExtractingStatus(const rapidjson::Document& json) {
		// get argument
		if(!json.HasMember("urllist"))
			return ServerCommandResponse::failed("Invalid arguments (\'urllist\' is missing).");

		if(!json["urllist"].IsUint64())
			return ServerCommandResponse::failed("Invalid arguments (\'urllist\' is not a valid number).");

		unsigned long listId = json["urllist"].GetUint64();

		// resetextractingstatus needs to be confirmed
		if(!json.HasMember("confirmed"))
				return ServerCommandResponse::toBeConfirmed(
						"Are you sure that you want to reset the extracting status of this URL list?"
				);

		// reset extracting status
		this->database.resetExtractingStatus(listId);

		return ServerCommandResponse("Extracting status reset.");
	}

	// server command startanalyzer(website, urllist, config): start an analyzer using the specified website, URL list, module
	//	and configuration
	Server::ServerCommandResponse Server::cmdStartAnalyzer(
			const rapidjson::Document& json,
			const std::string& ip
	) {
		// get arguments
		if(!json.HasMember("website"))
			return ServerCommandResponse::failed("Invalid arguments (\'website\' is missing).");

		if(!json["website"].IsUint64())
			return ServerCommandResponse::failed("Invalid arguments (\'website\' is not a valid number).");

		if(!json.HasMember("urllist"))
			return ServerCommandResponse::failed("Invalid arguments (\'urllist\' is missing).");

		if(!json["urllist"].IsUint64())
			return ServerCommandResponse::failed("Invalid arguments (\'urllist\' is not a valid number).");

		if(!json.HasMember("config"))
			return ServerCommandResponse::failed("Invalid arguments (\'config\' is missing).");

		if(!json["config"].IsUint64())
			return ServerCommandResponse::failed("Invalid arguments (\'config\' is not a valid number).");

		const ThreadOptions options(
				"analyzer",
				json["website"].GetUint64(),
				json["urllist"].GetUint64(),
				json["config"].GetUint64()
		);

		// check arguments
		if(!(this->database.isWebsite(options.website))) {
			std::ostringstream errStrStr;

			errStrStr << "Website #" << options.website << " not found.";

			return ServerCommandResponse::failed(errStrStr.str());
		}

		if(!(this->database.isUrlList(options.website, options.urlList))) {
			std::ostringstream errStrStr;

			errStrStr << "URL list #" << options.urlList << " for website #" << options.website << " not found.";

			return ServerCommandResponse::failed(errStrStr.str());
		}

		if(!(this->database.isConfiguration(options.website, options.config))) {
			std::ostringstream errStrStr;

			errStrStr << "Configuration #" << options.config << " for website #" << options.website << " not found.";

			return ServerCommandResponse::failed(errStrStr.str());
		}

		// get configuration
		const std::string config = this->database.getConfiguration(options.config);

		// check configuration JSON
		rapidjson::Document configJson;

		try {
			configJson = Helper::Json::parseRapid(config);
		}
		catch(const JsonException& e) {
			return ServerCommandResponse::failed("Could not parse analyzing configuration: " + e.whatStr() + ".");
		}

		if(!configJson.IsArray())
			return ServerCommandResponse::failed("Parsed analyzing configuration is not an array.");

		// get algorithm from configuration
		const unsigned int algo = Server::getAlgoFromConfig(configJson);

		if(!algo)
			return ServerCommandResponse::failed("Analyzing configuration does not include an algorithm.");

		// try to create algorithm thread
		this->analyzers.push_back(
				Module::Analyzer::Algo::initAlgo(AlgoThreadProperties(algo, this->database, options))
		);

		if(!(this->analyzers.back())) {
			this->analyzers.pop_back();

			std::ostringstream errStrStr;

			errStrStr << "Algorithm #" << algo << " not found.";

			return ServerCommandResponse::failed(errStrStr.str());
		}

		// start algorithm
		this->analyzers.back()->Module::Thread::start();

		// startanalyzer is a logged command
		std::ostringstream logStrStr;

		const unsigned long id = this->analyzers.back()->Module::Thread::getId();

		logStrStr << "analyzer #" << id << " started by " << ip << ".";

		this->database.log(logStrStr.str());

		return ServerCommandResponse("Analyzer has been started.");
	}

	// server command pauseparser(id): pause a parser by its ID
	Server::ServerCommandResponse Server::cmdPauseAnalyzer(
			const rapidjson::Document& json,
			const std::string& ip
	) {
		// get argument
		if(!json.HasMember("id"))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is missing).");

		if(!json["id"].IsUint64())
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is not a valid number).");

		const unsigned long id = json["id"].GetUint64();

		// find analyzer
		auto i = std::find_if(this->analyzers.begin(), this->analyzers.end(),
				[&id](const auto& p) {
					return p->Module::Thread::getId() == id;
				}
		);

		if(i == this->analyzers.end()) {
			std::ostringstream errStrStr;

			errStrStr << "Could not find analyzer #" << id << ".";

			return ServerCommandResponse::failed(errStrStr.str());
		}

		// pause analyzer
		if((*i)->Module::Thread::pause()) {
			// pauseanalyzer is a logged command
			std::ostringstream logStrStr;

			logStrStr << "analyzer #" << id << " paused by " << ip << ".";

			this->database.log(logStrStr.str());

			return ServerCommandResponse("Analyzer is pausing.");
		}

		// analyzer is not pausable
		return ServerCommandResponse::failed("This algorithm cannot be paused at this moment.");
	}

	// server command unpauseanalyzer(id): unpause a parser by its ID
	Server::ServerCommandResponse Server::cmdUnpauseAnalyzer(
			const rapidjson::Document& json,
			const std::string& ip
	) {
		// get argument
		if(!json.HasMember("id"))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is missing).");

		if(!json["id"].IsUint64())
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is not a valid number).");

		const unsigned long id = json["id"].GetUint64();

		// find analyzer
		auto i = std::find_if(this->analyzers.begin(), this->analyzers.end(),
				[&id](const auto& p) {
					return p->Module::Thread::getId() == id;
				}
		);

		if(i == this->analyzers.end()) {
			std::ostringstream errStrStr;

			errStrStr << "Could not find analyzer #" << id << ".";

			return ServerCommandResponse::failed(errStrStr.str());
		}

		// unpause analyzer
		(*i)->Module::Thread::unpause();

		// unpauseanalyzer is a logged command
		std::ostringstream logStrStr;

		logStrStr << "analyzer #" << id << " unpaused by " << ip << ".";

		this->database.log(logStrStr.str());

		return ServerCommandResponse("Analyzer is unpausing.");
	}

	// server command stopanalyzer(id): stop a parser by its ID
	Server::ServerCommandResponse Server::cmdStopAnalyzer(
			const rapidjson::Document& json,
			const std::string& ip
	) {
		// get argument
		if(!json.HasMember("id"))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is missing).");

		if(!json["id"].IsUint64())
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is not a valid number).");

		const unsigned long id = json["id"].GetUint64();

		// find analyzer
		auto i = std::find_if(this->analyzers.begin(), this->analyzers.end(),
				[&id](const auto& p) {
					return p->Module::Thread::getId() == id;
				}
		);

		if(i == this->analyzers.end()) {
			std::ostringstream errStrStr;

			errStrStr << "Could not find analyzer #" << id << ".";

			return ServerCommandResponse::failed(errStrStr.str());
		}

		// interrupt analyzer
		(*i)->Module::Thread::stop();

		// stopanalyzer is a logged command
		std::ostringstream logStrStr;

		logStrStr << "analyzer #" << id << " stopped by " << ip << ".";

		this->database.log(logStrStr.str());

		return ServerCommandResponse("Analyzer is stopping.");
	}

	// server command resetanalyzingstatus(urllist): reset the parsing status of a ID-specificed URL list
	Server::ServerCommandResponse Server::cmdResetAnalyzingStatus(const rapidjson::Document& json) {
		// get argument
		if(!json.HasMember("urllist"))
			return ServerCommandResponse::failed("Invalid arguments (\'urllist\' is missing).");

		if(!json["urllist"].IsUint64())
			return ServerCommandResponse::failed("Invalid arguments (\'urllist\' is not a valid number).");

		const unsigned long listId = json["urllist"].GetUint64();

		// resetanalyzingstatus needs to be confirmed
		if(!json.HasMember("confirmed"))
				return ServerCommandResponse::toBeConfirmed(
						"Are you sure that you want to reset the analyzing status of this URL list?"
				);

		// reset analyzing status
		this->database.resetAnalyzingStatus(listId);

		return ServerCommandResponse("Analyzing status reset.");
	}

	// server command addwebsite([crossdomain], [domain], namespace, name, [dir]): add a website
	Server::ServerCommandResponse Server::cmdAddWebsite(const rapidjson::Document& json) {
		WebsiteProperties properties;
		bool crossDomain = false;
		unsigned long id = 0;

		// get arguments
		if(json.HasMember("crossdomain")) {
			if(!json["crossdomain"].IsBool())
				return ServerCommandResponse::failed("Invalid arguments (\'crossdomain\' is not a boolean).");

			crossDomain = json["crossdomain"].GetBool();
		}

		if(!crossDomain) {
			if(!json.HasMember("domain"))
				return ServerCommandResponse::failed("Invalid arguments (\'domain\' is missing).");

			if(!json["domain"].IsString())
				return ServerCommandResponse::failed("Invalid arguments (\'domain\' is not a string).");

			properties.domain = std::string(json["domain"].GetString(), json["domain"].GetStringLength());
		}

		if(!json.HasMember("namespace"))
			return ServerCommandResponse::failed("Invalid arguments (\'namespace\' is missing).");

		if(!json["namespace"].IsString())
			return ServerCommandResponse::failed("Invalid arguments (\'namespace\' is not a string).");

		properties.nameSpace = std::string(json["namespace"].GetString(), json["namespace"].GetStringLength());

		if(!json.HasMember("name"))
			return ServerCommandResponse::failed("Invalid arguments (\'name\' is missing).");

		if(!json["name"].IsString())
			return ServerCommandResponse::failed("Invalid arguments (\'name\' is not a string).");

		properties.name = std::string(json["name"].GetString(), json["name"].GetStringLength());

		if(json.HasMember("dir")) {
			if(!json["dir"].IsString())
				return ServerCommandResponse::failed("Invalid arguments (\'dir\' is not a string).");

			properties.dir = std::string(json["dir"].GetString(), json["dir"].GetStringLength());
		}

		// check domain name
		if(!(Helper::Strings::checkDomainName(properties.domain)))
			return ServerCommandResponse::failed("Invalid character(s) in domain.");

		// check namespace
		if(properties.nameSpace.length() < 3)
			return ServerCommandResponse::failed("Website namespace has to be at least three characters long.");

		if(!(Helper::Strings::checkSQLName(properties.nameSpace)))
			return ServerCommandResponse::failed("Invalid character(s) in website namespace.");

		// check name
		if(properties.name.empty())
			return ServerCommandResponse::failed("Name is empty.");

		// correct and check domain name (remove protocol from start and slash from the end)
		if(!crossDomain) {
			while(properties.domain.length() > 6 && properties.domain.substr(0, 7) == "http://")
				properties.domain = properties.domain.substr(7);

			while(properties.domain.length() > 7 && properties.domain.substr(0, 8) == "https://")
				properties.domain = properties.domain.substr(8);

			while(!properties.domain.empty() && properties.domain.back() == '/')
				properties.domain.pop_back();

			if(properties.domain.empty())
				return ServerCommandResponse::failed("Domain is empty.");
		}

		// check for external directory
		if(properties.dir.empty())
			// add website to database
			id = this->database.addWebsite(properties);
		else {
			// check external directory
			if(!Helper::FileSystem::isValidDirectory(properties.dir))
				return ServerCommandResponse::failed("External directory does not exist");

			// adding a website using an external directory needs to be confirmed
			if(!json.HasMember("confirmed"))
				return ServerCommandResponse::toBeConfirmed(
						"Do you really want to use an external directory?\n"
						"!!! The directory cannot be changed once the website has been added !!!"
				);

			// try adding website to the database using an external directory
			try {
				id = this->database.addWebsite(properties);
			}
			catch(const IncorrectPathException &e) {
				return ServerCommandResponse::failed(
						"Incorrect path for external directory"
				);
			}
			catch(const PrivilegesException &e) {
				return ServerCommandResponse::failed(
						"The MySQL user used by the server needs to have FILE privilege to use an external directory"
				);
			}
			catch(const StorageEngineException &e) {
				return ServerCommandResponse::failed(
						"Could not access external directory. Make sure that\n"
						"* the MySQL server has permission to write into the directory\n"
						"* the AppArmor profile of the MySQL server allows access to the directory (if applicable)\n"
						"* file-per-table tablespace (innodb_file_per_table) is enabled"
				);
			}
		}

		if(!id)
			return ServerCommandResponse::failed("Could not add website to database.");

		return ServerCommandResponse("Website added.", id);
	}

	// server command updatewebsite(id, crossdomain, domain, namespace, name): edit a website
	Server::ServerCommandResponse Server::cmdUpdateWebsite(const rapidjson::Document& json) {
		WebsiteProperties properties;
		bool crossDomain = false;

		// get arguments
		if(!json.HasMember("id"))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is missing).");

		if(!json["id"].IsUint64())
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is not a valid number).");

		unsigned long id = json["id"].GetUint64();

		if(json.HasMember("crossdomain")) {
			if(!json["crossdomain"].IsBool())
				return ServerCommandResponse::failed("Invalid arguments (\'crossdomain\' is not a boolean).");

			crossDomain = json["crossdomain"].GetBool();
		}

		if(!crossDomain) {
			if(!json.HasMember("domain"))
				return ServerCommandResponse::failed("Invalid arguments (\'domain\' is missing).");

			if(!json["domain"].IsString())
				return ServerCommandResponse::failed("Invalid arguments (\'domain\' is not a string).");

			properties.domain = std::string(json["domain"].GetString(), json["domain"].GetStringLength());
		}

		if(!json.HasMember("namespace"))
			return ServerCommandResponse::failed("Invalid arguments (\'namespace\' is missing).");

		if(!json["namespace"].IsString())
			return ServerCommandResponse::failed("Invalid arguments (\'namespace\' is not a string).");

		properties.nameSpace = std::string(json["namespace"].GetString(), json["namespace"].GetStringLength());

		if(!json.HasMember("name"))
			return ServerCommandResponse::failed("Invalid arguments (\'name\' is missing).");

		if(!json["name"].IsString())
			return ServerCommandResponse::failed("Invalid arguments (\'name\' is not a string).");

		properties.name = std::string(json["name"].GetString(), json["name"].GetStringLength());

		// check name
		if(properties.name.empty())
			return ServerCommandResponse::failed("Name is empty.");

		// check namespace name
		if(properties.nameSpace.length() < 3)
			return ServerCommandResponse::failed("Website namespace has to be at least three characters long.");

		if(!(Helper::Strings::checkSQLName(properties.nameSpace)))
			return ServerCommandResponse::failed("Invalid character(s) in website namespace.");

		// correct and check domain name (remove protocol from start and slash from the end)
		while(properties.domain.length() > 6 && properties.domain.substr(0, 7) == "http://")
			properties.domain = properties.domain.substr(7);

		while(properties.domain.length() > 7 && properties.domain.substr(0, 8) == "https://")
			properties.domain = properties.domain.substr(8);

		while(!properties.domain.empty() && properties.domain.back() == '/')
			properties.domain.pop_back();

		if(properties.domain.empty())
			ServerCommandResponse::failed("Domain is empty.");

		// check website
		if(!(this->database.isWebsite(id))) {
			std::ostringstream errStrStr;

			errStrStr << "Website #" << id << " not found.";

			return ServerCommandResponse::failed(errStrStr.str());
		}

		// check whether threads are using the website
		if(std::find_if(this->crawlers.begin(), this->crawlers.end(), [&id](const auto& p) {
			return p->getWebsite() == id;
		}) != this->crawlers.end())
			return ServerCommandResponse::failed("Website cannot be changed while crawler is active.");

		if(std::find_if(this->parsers.begin(), this->parsers.end(), [&id](const auto& p) {
			return p->getWebsite() == id;
		}) != this->parsers.end())
			return ServerCommandResponse::failed("Website cannot be changed while parser is active.");

		// TODO: check extractors
		/*if(std::find_if(this->extractors.begin(), this->extractors.end(), [&id](const auto& p) {
			return p->getWebsite() == id;
		}) != this->extractors.end())
				return ServerCommandResponse::failed("Website cannot be changed while extractor is active.");*/

		if(std::find_if(this->analyzers.begin(), this->analyzers.end(), [&id](const auto& p) {
			return p->getWebsite() == id;
		}) != this->analyzers.end())
			return ServerCommandResponse::failed("Website cannot be changed while analyzer is active.");

		// check whether URLs will be changed or lost by changing type of website from cross-domain to specific domain
		if(!json.HasMember("confirmed")) {
			const unsigned long toModify = this->database.getChangedUrlsByWebsiteUpdate(id, properties);
			const unsigned long toDelete = this->database.getLostUrlsByWebsiteUpdate(id, properties);

			if(toModify || toDelete) {
				std::ostringstream confirmationStrStr;

				confirmationStrStr.imbue(std::locale(""));

				if(toModify)
					confirmationStrStr << toModify << " URL(s) will be modified.\n";

				if(toDelete)
					confirmationStrStr << toDelete << " URL(s) will be IRRECOVERABLY LOST.\n";

				confirmationStrStr << "Do you really want to change the domain?";

				return ServerCommandResponse::toBeConfirmed(confirmationStrStr.str());
			}
		}

		// update website in database
		this->database.updateWebsite(id, properties);

		return ServerCommandResponse("Website updated.");
	}

	// server command deletewebsite(id): delete a website and all associated data from the database by its ID
	Server::ServerCommandResponse Server::cmdDeleteWebsite(
			const rapidjson::Document& json,
			const std::string& ip
	) {
		// check whether the deletion of data is allowed
		if(!(this->settings.dataDeletable))
			return ServerCommandResponse::failed("Not allowed.");

		// get argument
		if(!json.HasMember("id"))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is missing).");

		if(!json["id"].IsUint64())
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is not a valid number).");

		const unsigned long id = json["id"].GetUint64();

		// check website
		if(!(this->database.isWebsite(id))) {
			std::ostringstream errStrStr;

			errStrStr << "Website #" << id << " not found.";

			return ServerCommandResponse::failed(errStrStr.str());
		}

		// check whether threads are using the website
		if(std::find_if(this->crawlers.begin(), this->crawlers.end(), [&id](const auto& p) {
			return p->getWebsite() == id;
		}) != this->crawlers.end())
			return ServerCommandResponse::failed("Website cannot be deleted while crawler is active.");

		if(std::find_if(this->parsers.begin(), this->parsers.end(), [&id](const auto& p) {
			return p->getWebsite() == id;
		}) != this->parsers.end())
			return ServerCommandResponse::failed("Website cannot be deleted while parser is active.");

		// TODO: check extractors
		/*if(std::find_if(this->extractors.begin(), this->extractors.end(), [&id](const auto& p) {
			return p->getWebsite() == id;
		}) != this->extractors.end())
				return ServerCommandResponse::failed("Website cannot be deleted while extractor is active.");*/

		if(std::find_if(this->analyzers.begin(), this->analyzers.end(), [&id](const auto& p) {
			return p->getWebsite() == id;
		}) != this->analyzers.end())
			return ServerCommandResponse::failed("Website cannot be deleted while analyzer is active.");

		// deletewebsite needs to be confirmed
		if(!json.HasMember("confirmed"))
			return ServerCommandResponse::toBeConfirmed(
					"Do you really want to delete this website?\n"
					"!!! All associated data will be lost !!!"
			);

		// delete website from database
		this->database.deleteWebsite(id);

		// deletewebsite is a logged command
		std::ostringstream logStrStr;

		logStrStr << "website #" << id << " deleted by " << ip << ".";

		this->database.log(logStrStr.str());

		return ServerCommandResponse("Website deleted.");
	}

	// server command duplicatewebsite(id): Duplicate a website by its ID (no processed data will be duplicated)
	Server::ServerCommandResponse Server::cmdDuplicateWebsite(const rapidjson::Document& json) {
		// get argument
		if(!json.HasMember("id"))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is missing).");

		if(!json["id"].IsUint64())
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is not a valid number).");

		const unsigned long id = json["id"].GetUint64();

		// check website
		if(!(this->database.isWebsite(id))) {
			std::ostringstream errStrStr;

			errStrStr << "Website #" << id << " not found.";

			return ServerCommandResponse::failed(errStrStr.str());
		}

		// duplicate website configuration
		const unsigned long newId = this->database.duplicateWebsite(id);

		if(!newId)
			return ServerCommandResponse::failed("Could not add duplicate to database.");

		return ServerCommandResponse("Website duplicated.", newId);
	}

	// server command addurllist(website, namespace, name): add a URL list to the ID-specified website
	Server::ServerCommandResponse Server::cmdAddUrlList(const rapidjson::Document& json) {

		// get arguments
		if(!json.HasMember("website"))
			return ServerCommandResponse::failed("Invalid arguments (\'website\' is missing).");

		if(!json["website"].IsUint64())
			return ServerCommandResponse::failed("Invalid arguments (\'website\' is not a valid number).");

		const unsigned long website = json["website"].GetUint64();

		if(!json.HasMember("namespace"))
			return ServerCommandResponse::failed("Invalid arguments (\'namespace\' is missing).");

		if(!json["namespace"].IsString())
			return ServerCommandResponse::failed("Invalid arguments (\'namespace\' is not a string).");

		if(!json.HasMember("name"))
			return ServerCommandResponse::failed("Invalid arguments (\'name\' is missing).");

		if(!json["name"].IsString())
			return ServerCommandResponse::failed("Invalid arguments (\'name\' is not a string).");

		const UrlListProperties properties(
				std::string(json["namespace"].GetString(), json["namespace"].GetStringLength()),
				std::string(json["name"].GetString(), json["name"].GetStringLength())
		);

		// check namespace
		if(properties.nameSpace.length() < 3)
			return ServerCommandResponse::failed("Namespace of URL list has to be at least three characters long.");

		if(!(Helper::Strings::checkSQLName(properties.nameSpace)))
			return ServerCommandResponse::failed("Invalid character(s) in namespace of URL list.");

		if(properties.nameSpace == "config")
			return ServerCommandResponse::failed("Namespace of URL list cannot be \'config\'.");

		// check name
		if(properties.name.empty())
			return ServerCommandResponse::failed("Name is empty.");

		// check website
		if(!(this->database.isWebsite(website))) {
			std::ostringstream errStrStr;

			errStrStr << "Website #" << website << " not found.";

			return ServerCommandResponse::failed(errStrStr.str());
		}

		// add URL list to database
		const unsigned long id = this->database.addUrlList(website, properties);

		if(!id)
			return ServerCommandResponse::failed("Could not add URL list to database.");

		return ServerCommandResponse("URL list added.", id);
	}

	// server command updateurllist(id, namespace, name): edit a URL list
	Server::ServerCommandResponse Server::cmdUpdateUrlList(const rapidjson::Document& json) {
		// get arguments
		if(!json.HasMember("id"))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is missing).");

		if(!json["id"].IsUint64())
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is not a valid number).");

		const unsigned long id = json["id"].GetUint64();

		if(!json.HasMember("namespace"))
			return ServerCommandResponse::failed("Invalid arguments (\'namespace\' is missing).");

		if(!json["namespace"].IsString())
			return ServerCommandResponse::failed("Invalid arguments (\'namespace\' is not a string).");

		if(!json.HasMember("name"))
			return ServerCommandResponse::failed("Invalid arguments (\'name\' is missing).");

		if(!json["name"].IsString())
			return ServerCommandResponse::failed("Invalid arguments (\'name\' is not a string).");

		const UrlListProperties properties(
				std::string(json["namespace"].GetString(), json["namespace"].GetStringLength()),
				std::string(json["name"].GetString(), json["name"].GetStringLength())
		);

		// check namespace
		if(properties.nameSpace.length() < 3)
			return ServerCommandResponse::failed("Namespace of URL list has to be at least three characters long.");

		if(!(Helper::Strings::checkSQLName(properties.nameSpace)))
			return ServerCommandResponse::failed("Invalid character(s) in namespace of URL list.");

		if(properties.nameSpace == "config")
			return ServerCommandResponse::failed("Namespace of URL list cannot be \'config\'.");

		// check name
		if(properties.name.empty())
			return ServerCommandResponse::failed("Name is empty.");

		// check URL list
		if(!(this->database.isUrlList(id))) {
			std::ostringstream errStrStr;

			errStrStr << "URL list #" << id << " not found.";

			return ServerCommandResponse::failed(errStrStr.str());
		}

		// check whether threads are using the URL list
		if(std::find_if(this->crawlers.begin(), this->crawlers.end(), [&id](const auto& p) {
			return p->getUrlList() == id;
		}) != this->crawlers.end())
			return ServerCommandResponse::failed("URL list cannot be changed while crawler is active.");

		if(std::find_if(this->parsers.begin(), this->parsers.end(), [&id](const auto& p) {
			return p->getUrlList() == id;
		}) != this->parsers.end())
			return ServerCommandResponse::failed("URL list cannot be changed while parser is active.");

		// TODO: check extractors
		/*if(std::find_if(this->extractors.begin(), this->extractors.end(), [&id](const auto& p) {
			return p->getUrlList() == id;
		}) != this->extractors.end())
				return ServerCommandResponse::failed("URL list cannot be changed while extractor is active.");*/

		if(std::find_if(this->analyzers.begin(), this->analyzers.end(), [&id](const auto& p) {
			return p->getUrlList() == id;
		}) != this->analyzers.end())
			return ServerCommandResponse::failed("URL list cannot be changed while analyzer is active.");

		// update URL list in database
		this->database.updateUrlList(id, properties);

		return ServerCommandResponse("URL list updated.");
	}

	// server command deleteurllist(id): delete a URL list and all associated data from the database by its ID
	Server::ServerCommandResponse Server::cmdDeleteUrlList(
			const rapidjson::Document& json,
			const std::string& ip
	) {
		// check whether the deletion of data is allowed
		if(!(this->settings.dataDeletable))
			return ServerCommandResponse::failed("Not allowed.");

		// get argument
		if(!json.HasMember("id"))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is missing).");

		if(!json["id"].IsUint64())
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is not a valid number).");

		const unsigned long id = json["id"].GetUint64();

		// check URL list
		if(!(this->database.isUrlList(id))) {
			std::ostringstream errStrStr;

			errStrStr << "URL list #" << id << " not found.";

			ServerCommandResponse::failed(errStrStr.str());
		}

		// check whether threads are using the URL list
		if(std::find_if(this->crawlers.begin(), this->crawlers.end(), [&id](const auto& p) {
			return p->getUrlList() == id;
		}) != this->crawlers.end())
			return ServerCommandResponse::failed("URL list cannot be deleted while crawler is active.");

		if(std::find_if(this->parsers.begin(), this->parsers.end(), [&id](const auto& p) {
			return p->getUrlList() == id;
		}) != this->parsers.end())
			return ServerCommandResponse::failed("URL list cannot be deleted while parser is active.");

		// TODO: check extractors
		/*if(std::find_if(this->extractors.begin(), this->extractors.end(), [&id](const auto& p) {
			return p->getUrlList() == id;
		}) != this->extractors.end())
				return ServerCommandResponse::failed("URL list cannot be deleted while extractor is active.");*/

		if(std::find_if(this->analyzers.begin(), this->analyzers.end(), [&id](const auto& p) {
			return p->getUrlList() == id;
		}) != this->analyzers.end())
			return ServerCommandResponse::failed("URL list cannot be deleted while analyzer is active.");

		// deleteurllist needs to be confirmed
		if(!json.HasMember("confirmed"))
			return ServerCommandResponse::toBeConfirmed(
					"Do you really want to delete this URL list?\n"
					"!!! All associated data will be lost !!!"
			);

		// delete URL list from database
		this->database.deleteUrlList(id);

		// deleteurllist is a logged command
		std::ostringstream logStrStr;

		logStrStr << "URL list #" << id << " deleted by " << ip << ".";

		this->database.log(logStrStr.str());

		return ServerCommandResponse("URL list deleted.");
	}

	// server command import(datatype, filetype, compression, filename, [...]): import data from a file into the database
	void Server::cmdImport(ConnectionPtr connection, unsigned long threadIndex, const std::string& message) {
		namespace Data = crawlservpp::Data;

		ServerCommandResponse response;
		rapidjson::Document json;

		// begin of worker thread
		MAIN_SERVER_WORKER_BEGIN

		if(Server::workerBegin(message, json, response)) {
			// get arguments
			if(!json.HasMember("datatype"))
				response = ServerCommandResponse::failed("Invalid arguments (\'datatype\' is missing).");

			else if(!json["datatype"].IsString())
				response = ServerCommandResponse::failed("Invalid arguments (\'datatype\' is not a string).");

			else if(!json.HasMember("filetype"))
				response = ServerCommandResponse::failed("Invalid arguments (\'filetype\' is missing).");

			else if(!json["filetype"].IsString())
				response = ServerCommandResponse::failed("Invalid arguments (\'filetype\' is not a string).");

			else if(!json.HasMember("compression"))
				response = ServerCommandResponse::failed("Invalid arguments (\'compression\' is missing).");

			else if(!json["compression"].IsString())
				response = ServerCommandResponse::failed("Invalid arguments (\'compression\' is not a string).");

			else if(!json.HasMember("filename"))
				response = ServerCommandResponse::failed("Invalid arguments (\'filename\' is missing).");

			else if(!json["filename"].IsString())
				response = ServerCommandResponse::failed("Invalid arguments (\'filename\' is not a string).");

			else if(!json["filename"].GetStringLength())
				response = ServerCommandResponse::failed("File upload failed.");

			else {
				const std::string dataType(json["datatype"].GetString(), json["datatype"].GetStringLength());
				const std::string fileType(json["filetype"].GetString(), json["filetype"].GetStringLength());
				const std::string compression(json["compression"].GetString(), json["compression"].GetStringLength());
				const std::string fileName(json["filename"].GetString(), json["filename"].GetStringLength());

				// start timer
				Timer::Simple timer;

				// generate full file name to import from
				const std::string fullFileName(
						this->dirCache
						+ Helper::FileSystem::getPathSeparator()
						+ fileName
				);

				std::string content;

				// check file name and whether file exists
				if(Helper::FileSystem::contains(this->dirCache, fullFileName)) {
					if(Helper::FileSystem::isValidFile(fullFileName)) {
						content = std::string(Data::File::read(fullFileName, true));

						if(compression != "none") {
							if(compression == "gzip")
								content = Data::Compression::Gzip::decompress(content);

							else if(compression == "zlib")
								content = Data::Compression::Zlib::decompress(content);

							else
								response = ServerCommandResponse::failed("Unknown compression type: \'" + compression + "\'.");
						}
					}
					else
						response = ServerCommandResponse::failed("File does not exist: \'" + fileName + "\'.");
				}
				else
					response = ServerCommandResponse::failed("Invalid file name: \'" + fileName + "\'.");

				if(!response.fail) {
					if(dataType == "urllist") {
						// get arguments for importing a URL list
						if(!json.HasMember("website"))
							response = ServerCommandResponse::failed(
									"Invalid arguments (\'website\' is missing)."
							);

						else if(!json["website"].IsUint64()) {
							response = ServerCommandResponse::failed(
									"Invalid arguments (\'website\' is not a valid number)."
							);
						}

						else if(!json.HasMember("urllist-target"))
							response = ServerCommandResponse::failed(
									"Invalid arguments (\'urllist-target\' is missing)."
							);

						else if(!json["urllist-target"].IsUint64()) {
							response = ServerCommandResponse::failed(
									"Invalid arguments (\'urllist-target\' is not a valid number)."
							);
						}

						else {
							const unsigned long website = json["website"].GetUint64();
							unsigned long target = json["urllist-target"].GetUint64();

							// import URL list
							std::queue<std::string> urls;

							if(fileType == "text") {
								// import URL list from text file
								if(!json.HasMember("is-firstline-header"))
									response = ServerCommandResponse::failed(
											"Invalid arguments (\'is-firstline-header\' is missing)."
									);

								else if(!json["is-firstline-header"].IsBool())
									response = ServerCommandResponse::failed(
											"Invalid arguments (\'is-firstline-header\' is not a boolean)."
									);

								else {
									urls = Data::ImportExport::Text::importList(
											content,
											json["is-firstline-header"].GetBool(),
											true
									);
								}
							}
							else
								response = ServerCommandResponse::failed("Unknown file type: \'" + fileType + "\'.");

							if(!response.fail) {
								// create new database connection for worker thread
								Module::Database db(this->dbSettings, "worker");

								db.setSleepOnError(MAIN_SERVER_SLEEP_ON_SQL_ERROR_SEC);

								db.connect();
								db.prepare();

								// check website
								if(!db.isWebsite(website))
									response = ServerCommandResponse::failed("Invalid website ID.");
								else {
									// check URL list
									if(target) {
										if(!db.isUrlList(website, target))
											response = ServerCommandResponse::failed(
													"Invalid target URL list ID."
											);
									}
									else {
										// check arguments for URL list creation
										if(!json.HasMember("urllist-namespace"))
											response = ServerCommandResponse::failed(
													"Invalid arguments (\'urllist-namespace\' is missing)."
											);

										else if(!json["urllist-namespace"].IsString())
											response = ServerCommandResponse::failed(
													"Invalid arguments (\'urllist-namespace\' is not a string)."
											);

										else if(!json.HasMember("urllist-name"))
											response = ServerCommandResponse::failed(
													"Invalid arguments (\'urllist-name\' is missing)."
											);

										else if(!json["urllist-name"].IsString())
											response = ServerCommandResponse::failed(
													"Invalid arguments (\'urllist-name\' is not a string)."
											);

										else {
											// set properties for new URL list
											UrlListProperties urlListProperties(
													std::string(
															json["urllist-namespace"].GetString(),
															json["urllist-namespace"].GetStringLength()
													),
													std::string(
															json["urllist-name"].GetString(),
															json["urllist-name"].GetStringLength()
													)
											);

											// add new URL list
											target = db.addUrlList(website, urlListProperties);
										}
									}

									if(!response.fail) {
										unsigned long added = 0;

										if(urls.size()) {
											// write to log
											std::ostringstream logStrStr;

											logStrStr << "importing ";

											if(urls.size() == 1)
												logStrStr << "one URL";
											else {
												logStrStr.imbue(std::locale(""));

												logStrStr << urls.size() << " URLs";
											}

											logStrStr << "...";

											db.log(logStrStr.str());

											// add URLs that do not exist already to URL list
											added = db.mergeUrls(target, urls);
										}

										// generate response and final log entry
										std::ostringstream responseStrStr;
										std::ostringstream logStrStr;
										const std::string timerStr(timer.tickStr());

										logStrStr << "completed (added ";

										switch(added) {
										case 0:
											responseStrStr << "Added no new URLs";
											logStrStr << "no new URL";

											break;

										case 1:
											responseStrStr << "Added one new URL";
											logStrStr << "one new URL";

											break;

										default:
											responseStrStr.imbue(std::locale(""));
											logStrStr.imbue(std::locale(""));

											responseStrStr << "Added " << added << " new URLs";
											logStrStr << added << " new URLs";
										}

										responseStrStr << " after " << timerStr << ".";
										logStrStr << " after " << timerStr << ").";

										response = ServerCommandResponse(responseStrStr.str());

										db.log(logStrStr.str());
									}
								}
							}
						}
					}
					else
						response = ServerCommandResponse::failed("Unknown data type: \'" + dataType + "\'.");
				}
			}
		}

		// end of worker thread
		MAIN_SERVER_WORKER_END(response)

		this->workerEnd(threadIndex, connection, message, response);
	}

	// server command merge(datatype, [...]): merge two tables in the database
	void Server::cmdMerge(ConnectionPtr connection, unsigned long threadIndex, const std::string& message) {
		namespace Data = crawlservpp::Data;

		ServerCommandResponse response;
		rapidjson::Document json;

		// begin of worker thread
		MAIN_SERVER_WORKER_BEGIN

		if(Server::workerBegin(message, json, response)) {
			// get arguments
			if(!json.HasMember("datatype"))
				response = ServerCommandResponse::failed("Invalid arguments (\'datatype\' is missing).");

			else if(!json["datatype"].IsString())
				response = ServerCommandResponse::failed("Invalid arguments (\'datatype\' is not a string).");

			else {
				const std::string datatype(json["datatype"].GetString(), json["datatype"].GetStringLength());

				if(datatype == "urllist") {
					// get arguments for merging two URL lists
					if(!json.HasMember("website"))
						response = ServerCommandResponse::failed(
								"Invalid arguments (\'website\' is missing)."
						);

					else if(!json["website"].IsUint64()) {
						response = ServerCommandResponse::failed(
								"Invalid arguments (\'website\' is not a valid number)."
						);
					}

					else if(!json.HasMember("urllist-source"))
						response = ServerCommandResponse::failed(
								"Invalid arguments (\'urllist-source\' is missing)."
						);

					else if(!json["urllist-source"].IsUint64()) {
						response = ServerCommandResponse::failed(
								"Invalid arguments (\'urllist-source\' is not a valid number)."
						);
					}

					else if(!json.HasMember("urllist-target"))
						response = ServerCommandResponse::failed(
								"Invalid arguments (\'urllist-target\' is missing)."
						);

					else if(!json["urllist-target"].IsUint64()) {
						response = ServerCommandResponse::failed(
								"Invalid arguments (\'urllist-target\' is not a valid number)."
						);
					}

					else {
						// merge URL lists
						const unsigned long website = json["website"].GetUint64();
						const unsigned long source = json["urllist-source"].GetUint64();
						const unsigned long target = json["urllist-target"].GetUint64();

						if(source == target)
							response = ServerCommandResponse::failed("A URL list cannot be merged with itself.");
						else {
							// create new database connection for worker thread
							Module::Database db(this->dbSettings, "worker");

							db.setSleepOnError(MAIN_SERVER_SLEEP_ON_SQL_ERROR_SEC);

							db.connect();
							db.prepare();

							// check website and URL lists
							if(!db.isWebsite(website))
								response = ServerCommandResponse::failed("Invalid website ID.");

							else if(!db.isUrlList(website, source))
								response = ServerCommandResponse::failed("Invalid ID of source URL list.");

							else if(!db.isUrlList(website, target))
								response = ServerCommandResponse::failed("Invalid ID of target URL list.");

							else {
								// start timer
								Timer::Simple timer;

								// get URLs from source
								auto urls(db.getUrls(source));

								// write to log
								std::ostringstream logStrStr;

								logStrStr << "merging with ";

								switch(urls.size()) {
								case 0:
									logStrStr << "empty URL list";

									break;

								case 1:
									logStrStr << "one URL";

									break;

								default:
									logStrStr.imbue(std::locale(""));

									logStrStr << urls.size() << " URLs";
								}

								logStrStr << "...";

								db.log(logStrStr.str());

								logStrStr.str("");

								logStrStr.clear();

								// merge URLs with target, generate response and final log entry
								const unsigned long added = db.mergeUrls(target, urls);
								const std::string timerStr(timer.tickStr());

								logStrStr << "completed (added ";

								switch(added) {
								case 0:
									response = ServerCommandResponse("No new URLs added after " + timerStr + ".");

									logStrStr << "no new URLs";

									break;

								case 1:
									response = ServerCommandResponse("One new URL added after " + timerStr + ".");

									logStrStr << "one new URL";

									break;

								default:
									std::ostringstream responseStrStr;

									responseStrStr.imbue(std::locale(""));

									responseStrStr << added << " new URLs added after " << timerStr << ".";

									response = responseStrStr.str();

									logStrStr << added << " new URLs";
								}

								logStrStr << " after " << timerStr << ").";

								// write to log
								db.log(logStrStr.str());
							}
						}
					}
				}
			}
		}

		// end of worker thread
		MAIN_SERVER_WORKER_END(response)

		this->workerEnd(threadIndex, connection, message, response);
	}

	// server command export(datatype, filetype, compression, [...]): export data from the database into a file
	void Server::cmdExport(ConnectionPtr connection, unsigned long threadIndex, const std::string& message) {
		namespace Data = crawlservpp::Data;

		ServerCommandResponse response;
		rapidjson::Document json;

		// begin of worker thread
		MAIN_SERVER_WORKER_BEGIN

		if(Server::workerBegin(message, json, response)) {
			// get arguments
			if(!json.HasMember("datatype"))
				response = ServerCommandResponse::failed("Invalid arguments (\'datatype\' is missing).");

			else if(!json["datatype"].IsString())
				response = ServerCommandResponse::failed("Invalid arguments (\'datatype\' is not a string).");

			else if(!json.HasMember("filetype"))
				response = ServerCommandResponse::failed("Invalid arguments (\'filetype\' is missing).");

			else if(!json["filetype"].IsString())
				response = ServerCommandResponse::failed("Invalid arguments (\'filetype\' is not a string).");

			else if(!json.HasMember("compression"))
				response = ServerCommandResponse::failed("Invalid arguments (\'compression\' is missing).");

			else if(!json["compression"].IsString())
				response = ServerCommandResponse::failed("Invalid arguments (\'compression\' is not a string).");

			else {
				const std::string datatype(json["datatype"].GetString(), json["datatype"].GetStringLength());
				const std::string filetype(json["filetype"].GetString(), json["filetype"].GetStringLength());
				const std::string compression(json["compression"].GetString(), json["compression"].GetStringLength());

				std::queue<std::string> urls;
				std::string content;

				// create new database connection for worker thread
				Module::Database db(this->dbSettings, "worker");

				db.setSleepOnError(MAIN_SERVER_SLEEP_ON_SQL_ERROR_SEC);

				db.connect();
				db.prepare();

				// start timer
				Timer::Simple timer;

				if(datatype == "urllist") {
					// get arguments for exporting a URL list
					if(!json.HasMember("website"))
						response = ServerCommandResponse::failed(
								"Invalid arguments (\'website\' is missing)."
						);

					else if(!json["website"].IsUint64()) {
						response = ServerCommandResponse::failed(
								"Invalid arguments (\'website\' is not a valid number)."
						);
					}

					else if(!json.HasMember("urllist-source"))
						response = ServerCommandResponse::failed(
								"Invalid arguments (\'urllist-source\' is missing)."
						);

					else if(!json["urllist-source"].IsUint64()) {
						response = ServerCommandResponse::failed(
								"Invalid arguments (\'urllist-source\' is not a valid number)."
						);
					}

					else {
						const unsigned long website = json["website"].GetUint64();
						const unsigned long source = json["urllist-source"].GetUint64();

						// check website and URL list
						if(!db.isWebsite(website))
							response = ServerCommandResponse::failed("Invalid website ID.");

						else if(!db.isUrlList(website, source))
							response = ServerCommandResponse::failed("Invalid URL list ID.");

						else {
							// get URLs
							urls = db.getUrls(source);

							// write to log
							std::ostringstream logStrStr;

							logStrStr << "exporting ";

							switch(urls.size()) {
							case 0:
								logStrStr << "empty URL list";

								break;

							case 1:
								logStrStr << "one URL";

								break;

							default:
								logStrStr.imbue(std::locale(""));

								logStrStr << urls.size() << " URLs";
							}

							logStrStr << "...";

							db.log(logStrStr.str());
						}
					}
				}
				else
					response = ServerCommandResponse::failed("Unknown data type: \'" + datatype + "\'.");

				if(!response.fail) {
					if(filetype == "text") {
						// export URL list into text file
						if(!json.HasMember("write-firstline-header"))
							response = ServerCommandResponse::failed(
									"Invalid arguments (\'write-firstline-header\' is missing)."
							);

						else if(!json["write-firstline-header"].IsBool())
							response = ServerCommandResponse::failed(
									"Invalid arguments (\'write-firstline-header\' is not a boolean)."
							);
						else {
							bool writeHeader = json["write-firstline-header"].GetBool();

							std::string header;

							if(writeHeader) {
								if(!json.HasMember("firstline-header"))
									response = ServerCommandResponse::failed(
											"Invalid arguments (\'firstline-header\' is missing)."
									);

								else if(!json["firstline-header"].IsString())
									response = ServerCommandResponse::failed(
											"Invalid arguments (\'firstline-header\' is not a string)."
									);
								else
									header = std::string(
											json["firstline-header"].GetString(),
											json["firstline-header"].GetStringLength()
									);
							}

							if(!response.fail && urls.size())
								content = Data::ImportExport::Text::exportList(urls, writeHeader, header);
						}
					}
					else
						response = ServerCommandResponse::failed("Unknown file type: \'" + filetype + "\'.");

					if(!response.fail && compression != "none") {
						if(compression == "gzip")
							content = Data::Compression::Gzip::compress(content);

						else if(compression == "zlib")
							content = Data::Compression::Zlib::compress(content);

						else
							response = ServerCommandResponse::failed("Unknown compression type: \'" + compression + "\'.");
					}

					if(!response.fail) {
						// generate file name
						const std::string fileName(Helper::Strings::generateRandom(this->webServer.fileLength));
						const std::string fullFileName(
								this->dirCache
								+ Helper::FileSystem::getPathSeparator()
								+ fileName
						);

						// write file
						Data::File::write(fullFileName, content, true);

						// return file name
						response = ServerCommandResponse(fileName);

						// write to log
						std::ostringstream logStrStr;

						logStrStr << "complete (generated ";

						switch(content.size()) {
						case 0:
							logStrStr << "empty file";

							break;

						case 1:
							logStrStr << "one byte";

							break;

						default:
							logStrStr.imbue(std::locale(""));

							logStrStr << content.size() << " bytes";
						}

						logStrStr << " after " << timer.tickStr() << ").";

						db.log(logStrStr.str());
					}
				}
			}
		}

		// end of worker thread
		MAIN_SERVER_WORKER_END(response)

		this->workerEnd(threadIndex, connection, message, response);
	}

	// server command addquery(website, name, query, type, resultbool, resultsingle, resultmulti, textonly): add a query
	Server::ServerCommandResponse Server::cmdAddQuery(const rapidjson::Document& json) {
		// get arguments
		if(!json.HasMember("website"))
			return ServerCommandResponse::failed("Invalid arguments (\'website\' is missing).");

		if(!json["website"].IsUint64())
			return ServerCommandResponse::failed("Invalid arguments (\'website\' is not a valid number).");

		const unsigned long website = json["website"].GetUint64();

		if(!json.HasMember("name"))
			return ServerCommandResponse::failed("Invalid arguments (\'name\' is missing).");

		if(!json["name"].IsString())
			return ServerCommandResponse::failed("Invalid arguments (\'name\' is not a string).");

		if(!json.HasMember("query"))
			return ServerCommandResponse::failed("Invalid arguments (\'query\' is missing).");

		if(!json["query"].IsString())
			return ServerCommandResponse::failed("Invalid arguments (\'query\' is not a string).");

		if(!json.HasMember("type"))
			return ServerCommandResponse::failed("Invalid arguments (\'type\' is missing).");

		if(!json["type"].IsString())
			return ServerCommandResponse::failed("Invalid arguments (\'type\' is not a string).");

		if(!json.HasMember("resultbool"))
			return ServerCommandResponse::failed("Invalid arguments (\'resultbool\' is missing).");

		if(!json["resultbool"].IsBool())
			return ServerCommandResponse::failed("Invalid arguments (\'resultbool\' is not a boolean).");

		if(!json.HasMember("resultsingle"))
			return ServerCommandResponse::failed("Invalid arguments (\'resultsingle\' is missing).");

		if(!json["resultsingle"].IsBool())
			return ServerCommandResponse::failed("Invalid arguments (\'resultsingle\' is not a boolean).");

		if(!json.HasMember("resultmulti"))
			return ServerCommandResponse::failed("Invalid arguments (\'resultmulti\' is missing).");

		if(!json["resultmulti"].IsBool())
			return ServerCommandResponse::failed("Invalid arguments (\'resultmulti\' is not a boolean).");

		if(!json.HasMember("textonly"))
			return ServerCommandResponse::failed("Invalid arguments (\'textonly\' is missing).");

		if(!json["textonly"].IsBool())
			return ServerCommandResponse::failed("Invalid arguments (\'textonly\' is not a boolean).");

		const QueryProperties properties(
				std::string(json["name"].GetString(), json["name"].GetStringLength()),
				std::string(json["query"].GetString(), json["query"].GetStringLength()),
				std::string(json["type"].GetString(), json["type"].GetStringLength()),
				json["resultbool"].GetBool(),
				json["resultsingle"].GetBool(),
				json["resultmulti"].GetBool(),
				json["textonly"].GetBool()
		);

		// check name
		if(properties.name.empty())
			return ServerCommandResponse::failed("Name is empty.");

		// check query text
		if(properties.text.empty())
			return ServerCommandResponse::failed("Query text is empty.");

		// check query type
		if(properties.type.empty())
			return ServerCommandResponse::failed("Query type is empty.");

		if(
				properties.type != "regex"
				&& properties.type != "xpath"
				&& properties.type != "jsonpointer"
				&& properties.type != "jsonpath"
		)
			return ServerCommandResponse::failed("Unknown query type: \'" + properties.type + "\'.");

		// check result type
		if(!properties.resultBool && !properties.resultSingle && !properties.resultMulti)
			return ServerCommandResponse::failed("No result type selected.");

		// check website
		if(website && !(this->database.isWebsite(website))) {
			std::ostringstream errStrStr;

			errStrStr << "Website #" << website << " not found.";

			return ServerCommandResponse::failed(errStrStr.str());
		}

		// add query to database
		const unsigned long id = this->database.addQuery(website, properties);

		if(!id)
			return ServerCommandResponse("Could not add query to database.");

		return ServerCommandResponse("Query added.", id);
	}

	// server command updatequery(id, name, query, type, resultbool, resultsingle, resultmulti, textonly): edit a query
	Server::ServerCommandResponse Server::cmdUpdateQuery(const rapidjson::Document& json) {
		// get arguments
		if(!json.HasMember("id"))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is missing).");

		if(!json["id"].IsUint64())
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is not a valid number).");

		const unsigned long id = json["id"].GetUint64();

		if(!json.HasMember("name"))
			return ServerCommandResponse::failed("Invalid arguments (\'name\' is missing).");

		if(!json["name"].IsString())
			return ServerCommandResponse::failed("Invalid arguments (\'name\' is not a string).");

		if(!json.HasMember("query"))
			return ServerCommandResponse::failed("Invalid arguments (\'query\' is missing).");

		if(!json["query"].IsString())
			return ServerCommandResponse::failed("Invalid arguments (\'query\' is not a string).");

		if(!json.HasMember("type"))
			return ServerCommandResponse::failed("Invalid arguments (\'type\' is missing).");

		if(!json["type"].IsString())
			return ServerCommandResponse::failed("Invalid arguments (\'type\' is not a string).");

		if(!json.HasMember("resultbool"))
			return ServerCommandResponse::failed("Invalid arguments (\'resultbool\' is missing).");

		if(!json["resultbool"].IsBool())
			return ServerCommandResponse::failed("Invalid arguments (\'resultbool\' is not a boolean).");

		if(!json.HasMember("resultsingle"))
			return ServerCommandResponse::failed("Invalid arguments (\'resultsingle\' is missing).");

		if(!json["resultsingle"].IsBool())
			return ServerCommandResponse::failed("Invalid arguments (\'resultsingle\' is not a boolean).");

		if(!json.HasMember("resultmulti"))
			return ServerCommandResponse::failed("Invalid arguments (\'resultmulti\' is missing).");

		if(!json["resultmulti"].IsBool())
			return ServerCommandResponse::failed("Invalid arguments (\'resultmulti\' is not a boolean).");

		if(!json.HasMember("textonly"))
			return ServerCommandResponse::failed("Invalid arguments (\'textonly\' is missing).");

		if(!json["textonly"].IsBool())
			return ServerCommandResponse::failed("Invalid arguments (\'textonly\' is not a boolean).");

		const QueryProperties properties(
				std::string(json["name"].GetString(), json["name"].GetStringLength()),
				std::string(json["query"].GetString(), json["query"].GetStringLength()),
				std::string(json["type"].GetString(), json["type"].GetStringLength()),
				json["resultbool"].GetBool(),
				json["resultsingle"].GetBool(),
				json["resultmulti"].GetBool(),
				json["textonly"].GetBool()
		);

		// check name
		if(properties.name.empty())
			return ServerCommandResponse::failed("Name is empty.");

		// check query text
		if(properties.text.empty())
			return ServerCommandResponse::failed("Query text is empty.");

		// check query type
		if(properties.type.empty())
			return ServerCommandResponse::failed("Query type is empty.");

		if(
				properties.type != "regex"
				&& properties.type != "xpath"
				&& properties.type != "jsonpointer"
				&& properties.type != "jsonpath"
		)
			return ServerCommandResponse::failed("Unknown query type: \'" + properties.type + "\'.");

		// check result type
		if(!properties.resultBool && !properties.resultSingle && !properties.resultMulti)
			return ServerCommandResponse::failed("No result type selected.");

		// check query
		if(!(this->database.isQuery(id))) {
			std::ostringstream errStrStr;

			errStrStr << "Query #" << id << " not found.";

			return ServerCommandResponse::failed(errStrStr.str());
		}

		// update query in database
		this->database.updateQuery(id, properties);

		return ServerCommandResponse("Query updated.");
	}

	// server command deletequery(id): delete a query from the database by its ID
	Server::ServerCommandResponse Server::cmdDeleteQuery(const rapidjson::Document& json) {
		// check whether the deletion of data is allowed
		if(!(this->settings.dataDeletable))
			return ServerCommandResponse::failed("Not allowed.");

		// get argument
		if(!json.HasMember("id"))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is missing).");

		if(!json["id"].IsUint64())
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is not a valid number).");

		const unsigned long id = json["id"].GetUint64();

		// check query
		if(!(this->database.isQuery(id))) {
			std::ostringstream errStrStr;

			errStrStr << "Query #" << id << " not found.";

			return ServerCommandResponse::failed(errStrStr.str());
		}

		if(!json.HasMember("confirmed"))
			return ServerCommandResponse::toBeConfirmed("Do you really want to delete this query?");

		// delete URL list from database
		this->database.deleteQuery(id);

		return ServerCommandResponse("Query deleted.");
	}

	// server command duplicatequery(id): Duplicate a query by its ID
	Server::ServerCommandResponse Server::cmdDuplicateQuery(const rapidjson::Document& json) {
		// get argument
		if(!json.HasMember("id"))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is missing).");

		if(!json["id"].IsUint64())
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is not a valid number).");

		const unsigned long id = json["id"].GetUint64();

		// check query
		if(!(this->database.isQuery(id))) {
			std::ostringstream errStrStr;

			errStrStr << "Query #" << id << " not found.";

			return ServerCommandResponse::failed(errStrStr.str());
		}

		// duplicate query
		const unsigned long newId = this->database.duplicateQuery(id);

		if(!newId)
			return ServerCommandResponse::failed("Could not add duplicate to database.");

		return ServerCommandResponse("Query duplicated.", newId);
	}

	// server command testquery(query, type, resultbool, resultsingle, resultmulti, textonly, text): test query on text
	void Server::cmdTestQuery(ConnectionPtr connection, unsigned long threadIndex, const std::string& message) {
		ServerCommandResponse response;
		rapidjson::Document json;

		// begin of worker thread
		MAIN_SERVER_WORKER_BEGIN

		if(Server::workerBegin(message, json, response)) {
			// get arguments
			if(!json.HasMember("query"))
				response = ServerCommandResponse::failed("Invalid arguments (\'query\' is missing).");

			else if(!json["query"].IsString())
				response = ServerCommandResponse::failed("Invalid arguments (\'query\' is not a string).");

			else if(!json.HasMember("type"))
				response = ServerCommandResponse::failed("Invalid arguments (\'type\' is missing).");

			else if(!json["type"].IsString())
				response = ServerCommandResponse::failed("Invalid arguments (\'type\' is not a string).");

			else if(!json.HasMember("resultbool"))
				response = ServerCommandResponse::failed("Invalid arguments (\'resultbool\' is missing).");

			else if(!json["resultbool"].IsBool())
				response = ServerCommandResponse::failed("Invalid arguments (\'resultbool\' is not a boolean).");

			else if(!json.HasMember("resultsingle"))
				response = ServerCommandResponse::failed("Invalid arguments (\'resultsingle\' is missing).");

			else if(!json["resultsingle"].IsBool())
				response = ServerCommandResponse::failed("Invalid arguments (\'resultsingle\' is not a boolean).");

			else if(!json.HasMember("resultmulti"))
				response = ServerCommandResponse::failed("Invalid arguments (\'resultmulti\' is missing).");

			else if(!json["resultmulti"].IsBool())
				response = ServerCommandResponse::failed("Invalid arguments (\'resultmulti\' is not a boolean).");

			else if(!json.HasMember("textonly"))
				response = ServerCommandResponse::failed("Invalid arguments (\'textonly\' is missing).");

			else if(!json["textonly"].IsBool())
				response = ServerCommandResponse::failed("Invalid arguments (\'textonly\' is not a boolean).");

			else if(!json.HasMember("text"))
				response = ServerCommandResponse::failed("Invalid arguments (\'text\' is missing).");

			else if(!json["text"].IsString())
				response = ServerCommandResponse::failed("Invalid arguments (\'text\' is not a string).");

			else {
				const QueryProperties properties(
						std::string(json["query"].GetString(), json["query"].GetStringLength()),
						std::string(json["type"].GetString(), json["type"].GetStringLength()),
						json["resultbool"].GetBool(),
						json["resultsingle"].GetBool(),
						json["resultmulti"].GetBool(),
						json["textonly"].GetBool()
				);

				const std::string text(json["text"].GetString(), json["text"].GetStringLength());

				// check query text, query type and result type
				if(properties.text.empty())
					response = ServerCommandResponse::failed("Query text is empty.");

				else if(properties.type.empty())
					response = ServerCommandResponse::failed("Query type is empty.");

				else if(
						properties.type != "regex"
						&& properties.type != "xpath"
						&& properties.type != "jsonpointer"
						&& properties.type != "jsonpath"
				)
					response = ServerCommandResponse::failed("Unknown query type: \'" + properties.type + "\'.");

				else if(!properties.resultBool && !properties.resultSingle && !properties.resultMulti)
					response = ServerCommandResponse::failed("No result type selected.");

				else {
					// test query
					std::string result;

					if(properties.type == "regex") {
						// test RegEx expression on text
						try {
							Timer::SimpleHR timer;

							const Query::RegEx regExTest(
									properties.text,
									properties.resultBool || properties.resultSingle,
									properties.resultMulti
							);

							result = "COMPILING TIME: " + timer.tickStr() + '\n';

							if(properties.resultBool) {
								// get boolean result (does at least one match exist?)
								result	+= "BOOLEAN RESULT ("
										+ timer.tickStr()
										+ "): "
										+ (regExTest.getBool(text) ? "true" : "false")
										+ '\n';
							}

							if(properties.resultSingle) {
								// get first result (first full match)
								std::string tempResult;

								regExTest.getFirst(text, tempResult);

								if(tempResult.empty())
									result += "FIRST RESULT (" + timer.tickStr() + "): [empty]\n";
								else
									result += "FIRST RESULT (" + timer.tickStr() + "): " + tempResult + '\n';
							}

							if(properties.resultMulti) {
								// get all results (all full matches)
								std::vector<std::string> tempResult;

								regExTest.getAll(text, tempResult);

								result += "ALL RESULTS (" + timer.tickStr() + "):";

								if(tempResult.empty())
									result += " [empty]\n";
								else {
									std::ostringstream resultStrStr;
									unsigned long counter = 0;

									resultStrStr << '\n';

									for(auto i = tempResult.begin(); i != tempResult.end(); ++i) {
										++counter;

										resultStrStr << '[' << counter << "] " << *i << '\n';
									}

									result += resultStrStr.str();
								}
							}
						}
						catch(const RegExException& e) {
							response = ServerCommandResponse::failed("RegEx error: " + e.whatStr());
						}
					}
					else if(properties.type == "xpath") {
						Timer::SimpleHR timer;
						std::queue<std::string> warnings;

						// test XPath expression on text
						try {
							const Query::XPath xPathTest(
									properties.text,
									properties.textOnly
							);

							result = "COMPILING TIME: " + timer.tickStr() + '\n';

							Parsing::XML xmlDocumentTest;

							xmlDocumentTest.parse(text, warnings, true);

							while(!warnings.empty()) {
								result += "WARNING: " + warnings.front() + '\n';

								warnings.pop();
							}

							result += "PARSING TIME: " + timer.tickStr() + '\n';

							if(properties.resultBool) {
								// get boolean result (does at least one match exist?)
								result	+= "BOOLEAN RESULT ("
										+ timer.tickStr()
										+ "): "
										+ (xPathTest.getBool(xmlDocumentTest) ? "true" : "false")
										+ '\n';
							}

							if(properties.resultSingle) {
								// get first result (first full match)
								std::string tempResult;

								xPathTest.getFirst(xmlDocumentTest, tempResult);

								if(tempResult.empty())
									result += "FIRST RESULT (" + timer.tickStr() + "): [empty]\n";
								else
									result += "FIRST RESULT (" + timer.tickStr() + "): " + tempResult + '\n';
							}

							if(properties.resultMulti) {
								// get all results (all full matches)
								std::vector<std::string> tempResult;

								xPathTest.getAll(xmlDocumentTest, tempResult);

								result += "ALL RESULTS (" + timer.tickStr() + "):";

								if(tempResult.empty())
									result += " [empty]\n";
								else {
									std::ostringstream resultStrStr;
									unsigned long counter = 0;

									resultStrStr << '\n';

									for(auto i = tempResult.begin(); i != tempResult.end(); ++i) {
										++counter;

										resultStrStr << '[' << counter << "] " << *i << '\n';
									}

									result += resultStrStr.str();
								}
							}
						}
						catch(const XPathException& e) {
							response = ServerCommandResponse::failed("XPath error - " + e.whatStr());
						}
						catch(const XMLException& e) {
							response = ServerCommandResponse::failed("XML error: " + e.whatStr());
						}
					}
					else if(properties.type == "jsonpointer") {
						// test JSONPointer query on text
						try {
							Timer::SimpleHR timer;

							const Query::JsonPointer JSONPointerTest(
									properties.text
							);

							result = "COMPILING TIME: " + timer.tickStr() + '\n';

							rapidjson::Document jsonDocumentTest;

							try {
								jsonDocumentTest = Helper::Json::parseRapid(text);
							}
							catch(const JsonException& e) {
								response = ServerCommandResponse::failed("Could not parse JSON: " + e.whatStr() + ".");
							}

							if(!response.fail) {
								result += "PARSING TIME: " + timer.tickStr() + '\n';

								if(properties.resultBool) {
									// get boolean result (does at least one match exist?)
									result	+= "BOOLEAN RESULT ("
											+ timer.tickStr() + "): "
											+ (JSONPointerTest.getBool(jsonDocumentTest) ? "true" : "false")
											+ '\n';
								}

								if(properties.resultSingle) {
									// get first result (first full match)
									std::string tempResult;

									JSONPointerTest.getFirst(jsonDocumentTest, tempResult);

									if(tempResult.empty())
										result += "FIRST RESULT (" + timer.tickStr() + "): [empty]\n";
									else
										result += "FIRST RESULT (" + timer.tickStr() + "): " + tempResult + '\n';
								}

								if(properties.resultMulti) {
									// get all results (all full matches)
									std::vector<std::string> tempResult;

									JSONPointerTest.getAll(jsonDocumentTest, tempResult);

									result += "ALL RESULTS (" + timer.tickStr() + "):";

									if(tempResult.empty())
										result += " [empty]\n";
									else {
										std::ostringstream resultStrStr;
										unsigned long counter = 0;

										resultStrStr << '\n';

										for(auto i = tempResult.begin(); i != tempResult.end(); ++i) {
											++counter;

											resultStrStr << '[' << counter << "] " << *i << '\n';
										}

										result += resultStrStr.str();
									}
								}
							}
						}
						catch(const JSONPointerException& e) {
							response = ServerCommandResponse::failed("JSONPointer error: " + e.whatStr());
						}
					}
					else {
						// test JSONPath query on text
						try {
							Timer::SimpleHR timer;

							const Query::JsonPath JSONPathTest(
									properties.text
							);

							jsoncons::json jsonTest;

							try {
								jsonTest = Helper::Json::parseCons(text);
							}
							catch(const JsonException& e) {
								response = ServerCommandResponse::failed("Could not parse JSON: " + e.whatStr() + ".");
							}

							if(!response.fail) {
								result += "PARSING TIME: " + timer.tickStr() + '\n';

								if(properties.resultBool) {
									// get boolean result (does at least one match exist?)
									result	+= "BOOLEAN RESULT ("
											+ timer.tickStr()
											+ "): "
											+ (JSONPathTest.getBool(jsonTest) ? "true" : "false")
											+ '\n';
								}

								if(properties.resultSingle) {
									// get first result (first full match)
									std::string tempResult;

									JSONPathTest.getFirst(jsonTest, tempResult);

									if(tempResult.empty())
										result += "FIRST RESULT (" + timer.tickStr() + "): [empty]\n";
									else
										result += "FIRST RESULT (" + timer.tickStr() + "): " + tempResult + '\n';
								}

								if(properties.resultMulti) {
									// get all results (all full matches)
									std::vector<std::string> tempResult;

									JSONPathTest.getAll(jsonTest, tempResult);

									result += "ALL RESULTS (" + timer.tickStr() + "):";

									if(tempResult.empty())
										result += " [empty]\n";
									else {
										std::ostringstream resultStrStr;
										unsigned long counter = 0;

										resultStrStr << '\n';

										for(auto i = tempResult.begin(); i != tempResult.end(); ++i) {
											++counter;

											resultStrStr << '[' << counter << "] " << *i << '\n';
										}

										result += resultStrStr.str();
									}
								}
							}
						}
						catch(const JSONPathException& e) {
							response = ServerCommandResponse::failed("JSONPath error: " + e.whatStr());
						}
					}

					if(!response.fail){
						result.pop_back();

						response = ServerCommandResponse(result);
					}
				}
			}
		}

		// end of worker thread
		MAIN_SERVER_WORKER_END(response)

		this->workerEnd(threadIndex, connection, message, response);
	}

	// server command addconfig(website, module, name, config): Add configuration to database
	Server::ServerCommandResponse Server::cmdAddConfig(const rapidjson::Document& json) {
		// get arguments
		if(!json.HasMember("website"))
			return ServerCommandResponse::failed("Invalid arguments (\'website\' is missing).");

		if(!json["website"].IsUint64())
			return ServerCommandResponse::failed("Invalid arguments (\'website\' is not a valid number).");

		const unsigned long website = json["website"].GetUint64();

		if(!json.HasMember("module"))
			return ServerCommandResponse::failed("Invalid arguments (\'module\' is missing).");

		if(!json["module"].IsString())
			return ServerCommandResponse::failed("Invalid arguments (\'module\' is not a string).");

		if(!json.HasMember("name"))
			return ServerCommandResponse::failed("Invalid arguments (\'name\' is missing).");

		if(!json["name"].IsString())
			return ServerCommandResponse::failed("Invalid arguments (\'name\' is not a string).");

		if(!json.HasMember("config"))
			return ServerCommandResponse::failed("Invalid arguments (\'config\' is missing).");

		if(!json["config"].IsString())
			return ServerCommandResponse::failed("Invalid arguments (\'config\' is not a string).");

		const ConfigProperties properties(
				std::string(json["module"].GetString(), json["module"].GetStringLength()),
				std::string(json["name"].GetString(), json["name"].GetStringLength()),
				std::string(json["config"].GetString(), json["config"].GetStringLength())
		);

		// check name
		if(properties.name.empty())
			return ServerCommandResponse::failed("Name is empty.");

		// check configuration JSON
		rapidjson::Document configJson;

		try {
			configJson = Helper::Json::parseRapid(properties.config);
		}
		catch(const JsonException& e) {
			return ServerCommandResponse::failed("Could not parse JSON: " + e.whatStr() + ".");
		}

		if(!configJson.IsArray())
			return ServerCommandResponse::failed("Parsed JSON is not an array.");

		// check analyzer configuration for algorithm
		if(properties.module == "analyzer") {
			if(!Server::getAlgoFromConfig(configJson))
				return ServerCommandResponse::failed("No algorithm selected.");
		}

		// check website
		if(!(this->database.isWebsite(website))) {
			std::ostringstream errStrStr;

			errStrStr << "Website #" << website << " not found.";

			return ServerCommandResponse::failed(errStrStr.str());
		}

		// add configuration to database
		const unsigned long id = this->database.addConfiguration(website, properties);

		if(!id)
			return ServerCommandResponse::failed("Could not add configuration to database.");

		return ServerCommandResponse("Configuration added.", id);
	}

	// server command updateconfig(id, name, config): Update a configuration in the database by its ID
	Server::ServerCommandResponse Server::cmdUpdateConfig(const rapidjson::Document& json) {
		// get arguments
		if(!json.HasMember("id"))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is missing).");

		if(!json["id"].IsUint64())
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is not a valid number).");

		const unsigned long id = json["id"].GetUint64();

		if(!json.HasMember("name"))
			return ServerCommandResponse::failed("Invalid arguments (\'name\' is missing).");

		if(!json["name"].IsString())
			return ServerCommandResponse::failed("Invalid arguments (\'name\' is not a string).");

		if(!json.HasMember("config"))
			return ServerCommandResponse::failed("Invalid arguments (\'config\' is missing).");

		if(!json["config"].IsString())
			return ServerCommandResponse::failed("Invalid arguments (\'config\' is not a string).");

		const ConfigProperties properties(
				std::string(json["name"].GetString(), json["name"].GetStringLength()),
				std::string(json["config"].GetString(), json["config"].GetStringLength())
		);

		// check name
		if(properties.name.empty())
			return ServerCommandResponse::failed("Name is empty.");

		// check configuration JSON
		rapidjson::Document configJson;

		try {
			configJson = Helper::Json::parseRapid(properties.config);
		}
		catch(const JsonException& e) {
			return ServerCommandResponse::failed("Could not parse JSON.");
		}

		if(!configJson.IsArray())
			return ServerCommandResponse::failed("Parsed JSON is not an array.");

		// check configuration
		if(!(this->database.isConfiguration(id))) {
			std::ostringstream errStrStr;

			errStrStr << "Configuration #" << id << " not found.";

			return ServerCommandResponse::failed(errStrStr.str());
		}

		// update configuration in database
		this->database.updateConfiguration(id, properties);

		return ServerCommandResponse("Configuration updated.");
	}

	// server command deleteconfig(id): delete a configuration from the database by its ID
	Server::ServerCommandResponse Server::cmdDeleteConfig(const rapidjson::Document& json) {
		// check whether the deletion of data is allowed
		if(!(this->settings.dataDeletable))
			return ServerCommandResponse::failed("Not allowed.");

		// get argument
		if(!json.HasMember("id"))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is missing).");

		if(!json["id"].IsUint64())
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is not a valid number).");

		const unsigned long id = json["id"].GetUint64();

		// check configuration
		if(!(this->database.isConfiguration(id))) {
			std::ostringstream errStrStr;

			errStrStr << "Configuration #" << id << " not found.";

			ServerCommandResponse::failed(errStrStr.str());
		}

		// deleteconfig needs to be confirmed
		if(!json.HasMember("confirmed"))
			return ServerCommandResponse::toBeConfirmed("Do you really want to delete this configuration?");

		// delete configuration from database
		this->database.deleteConfiguration(id);

		return ServerCommandResponse("Configuration deleted.");
	}

	// server command duplicateconfig(id): Duplicate a configuration by its ID
	Server::ServerCommandResponse Server::cmdDuplicateConfig(const rapidjson::Document& json) {
		// get argument
		if(!json.HasMember("id"))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is missing).");

		if(!json["id"].IsUint64())
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is not a valid number).");

		const unsigned long id = json["id"].GetUint64();

		// check configuration
		if(!(this->database.isConfiguration(id))) {
			std::ostringstream errStrStr;

			errStrStr << "Configuration #" << id << " not found.";

			return ServerCommandResponse::failed(errStrStr.str());
		}

		// duplicate configuration
		const unsigned long newId = this->database.duplicateConfiguration(id);

		if(!newId)
			return ServerCommandResponse::failed("Could not add duplicate to database.");

		return ServerCommandResponse("Configuration duplicated.", newId);
	}

	// server command warp(thread, target): Let thread jump to specified ID
	Server::ServerCommandResponse Server::cmdWarp(const rapidjson::Document& json) {
		// get arguments
		if(!json.HasMember("thread"))
			return ServerCommandResponse::failed("Invalid arguments (\'thread\' is missing).");

		if(!json["thread"].IsUint64())
			return ServerCommandResponse::failed("Invalid arguments (\'thread\' is not a valid number).");

		if(!json.HasMember("target"))
			return ServerCommandResponse::failed("Invalid arguments (\'target\' is missing).");

		if(!json["target"].IsUint64())
			return ServerCommandResponse::failed("Invalid arguments (\'target\' is not a valid number).");

		const unsigned long thread = json["thread"].GetUint64();
		const unsigned long target = json["target"].GetUint64();

		// find thread
		auto c = std::find_if(this->crawlers.begin(), this->crawlers.end(),
				[&thread](const auto& p) {
					return p->Module::Thread::getId() == thread;
				}
		);

		if(c != this->crawlers.end()) {
			(*c)->Module::Thread::warpTo(target);

			std::ostringstream responseStrStr;

			responseStrStr << "Crawler #" << thread << " will warp to #" << target << ".";

			return ServerCommandResponse(responseStrStr.str());
		}

		auto p = std::find_if(this->parsers.begin(), this->parsers.end(),
				[&thread](const auto& p) {
					return p->Module::Thread::getId() == thread;
				}
		);

		if(p != this->parsers.end()) {
			(*p)->Module::Thread::warpTo(target);

			std::ostringstream responseStrStr;

			responseStrStr << "Parser #" << thread << " will warp to #" << target << ".";

			return ServerCommandResponse(responseStrStr.str());
		}

		// TODO: Time travel for extractors
/*
		auto e = std::find_if(this->extractors.begin(), this->extractors.end(),
				[&thread](const auto& p) {
					return p->Module::Thread::getId() == thread;
				}
		);

		if(e != this->extractors.end()) {
			(*e)->Module::Thread::warpTo(target);

			std::ostringstream responseStrStr;

			responseStrStr << "Extractor #" << thread << " will warp to #" << target << ".";

			return ServerCommandResponse(responseStrStr.str());
		}
*/
		auto a = std::find_if(this->analyzers.begin(), this->analyzers.end(),
				[&thread](const auto& p) {
					return p->Module::Thread::getId() == thread;
				}
		);

		if(a != this->analyzers.end())
			return ServerCommandResponse::failed("Time travel is not supported for analyzers.");
		else {
			std::ostringstream errStrStr;

			errStrStr << "Could not find thread #" << thread << ".";

			return ServerCommandResponse::failed(errStrStr.str());
		}
	}

	// server command download(filename): Download the specified file from the file cache of the web server
	//  NOTE: Returns the name of the file to download
	Server::ServerCommandResponse Server::cmdDownload(const rapidjson::Document& json) {
		// get argument
		if(!json.HasMember("filename"))
			return ServerCommandResponse::failed("Invalid arguments (\'filename\' is missing).");

		if(!json["filename"].IsString())
			return ServerCommandResponse::failed("Invalid arguments (\'filename\' is not a string).");

		return ServerCommandResponse(
				std::string(
						json["filename"].GetString(),
						json["filename"].GetStringLength()
				)
		);
	}

	// private static helper function: begin of worker thread
	bool Server::workerBegin(
			const std::string& message,
			rapidjson::Document& json,
			ServerCommandResponse& response
	) {
		// parse JSON (again) for thread
		try {
			json = Helper::Json::parseRapid(message);

			if(!json.IsObject())
				response = ServerCommandResponse::failed("Parsed JSON is not an object.");
		}
		catch(const JsonException& e) {
			response = ServerCommandResponse::failed("Could not parse JSON: " + e.whatStr() + ".");
		}

		return !response.fail;
	}

	// private helper function: end of worker thread
	void Server::workerEnd(
			unsigned long threadIndex,
			ConnectionPtr connection,
			const std::string& message,
			const ServerCommandResponse& response
	) {
		// generate the reply
		const std::string replyString(Server::generateReply(response, message));

		// send the reply
		this->webServer.send(connection, 200, "application/json", replyString);

		// set thread status to not running
		{
			std::lock_guard<std::mutex> workersLocked(this->workersLock);

			this->workersRunning.at(threadIndex) = false;
		}
	}

	// private static helper function: generate server reply
	std::string Server::generateReply(const ServerCommandResponse& response, const std::string& msgBody) {
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

		return std::string(replyBuffer.GetString(), replyBuffer.GetLength());
	}

} /* crawlservpp::Main */
