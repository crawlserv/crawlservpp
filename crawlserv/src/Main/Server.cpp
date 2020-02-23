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
			const ServerSettings& serverSettings,
			const DatabaseSettings& databaseSettings,
			const NetworkSettings& networkSettings
	)
			: settings(serverSettings),
			  dbSettings(databaseSettings, MAIN_SERVER_DIR_DEBUG),
			  netSettings(networkSettings),
			  database(dbSettings, "server"),
			  allowed(serverSettings.allowedClients),
			  running(true),
			  offline(true),
			  dirCache(MAIN_SERVER_DIR_CACHE),
			  dirCookies(MAIN_SERVER_DIR_COOKIES),
			  webServer(this->dirCache) {

		// clear or create cache directory
		if(Helper::FileSystem::isValidDirectory(this->dirCache))
			Helper::FileSystem::clearDirectory(this->dirCache);
		else
			Helper::FileSystem::createDirectory(this->dirCache);

		// create cookies directory if it does not exist
		Helper::FileSystem::createDirectoryIfNotExists(this->dirCookies);

		// create and save debug directory if needed
		if(dbSettings.debugLogging)
			Helper::FileSystem::createDirectoryIfNotExists(dbSettings.debugDir);

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
		this->setStatus("crawlserv++ is ready");

		// load threads from database
		for(const auto& thread : this->database.getThreads()) {
			if(thread.options.module == "crawler") {
				// load crawler thread
				this->crawlers.push_back(
						std::make_unique<Module::Crawler::Thread>(
								this->database,
								this->dirCookies,
								thread.options,
								this->netSettings,
								thread.status
						)
				);

				this->crawlers.back()->Module::Thread::start();

				// write to log
				this->database.log(
						"crawler #"
						+ std::to_string(thread.status.id)
						+ " continued."
				);
			}
			else if(thread.options.module == "parser") {
				// load parser thread
				this->parsers.push_back(
						std::make_unique<Module::Parser::Thread>(
								this->database,
								thread.options,
								thread.status
						)
				);

				this->parsers.back()->Module::Thread::start();

				// write to log
				this->database.log(
						"parser #"
						+ std::to_string(thread.status.id)
						+ " continued."
				);
			}
			else if(thread.options.module == "extractor") {
				this->extractors.push_back(
						std::make_unique<Module::Extractor::Thread>(
								this->database,
								this->dirCookies,
								thread.options,
								this->netSettings,
								thread.status
						)
				);

				this->extractors.back()->Module::Thread::start();

				// write to log
				this->database.log(
						"extractor #"
						+ std::to_string(thread.status.id)
						+ " continued."
				);
			}
			else if(thread.options.module == "analyzer") {
				// get JSON
				const std::string config(
						this->database.getConfiguration(
								thread.options.config
						)
				);

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
						Module::Analyzer::Algo::initAlgo(
								AlgoThreadProperties(
										Server::getAlgoFromConfig(configJson),
										this->database,
										thread.options,
										thread.status
								)
						)
				);

				if(!(this->analyzers.back())) {
					this->analyzers.pop_back();

					this->database.log("[WARNING] Unknown algorithm ignored when loading threads.");

					continue;
				}

				// start algorithm (and get its ID)
				this->analyzers.back()->Module::Thread::start();

				// write to log
				this->database.log(
						"analyzer #"
						+ std::to_string(thread.status.id)
						+ " continued."
				);
			}
			else
				throw Exception("Unknown thread module \'" + thread.options.module + "\'");
		}

		// save start time for up-time calculation
		this->uptimeStart = std::chrono::steady_clock::now();

		// start logging
		this->database.log(
				"successfully started and connected to database [MySQL v"
				+ this->database.getMysqlVersion()
				+ "; datadir=\'"
				+ this->database.getDataDir()
				+ "\'; maxAllowedPacketSize="
				+ std::to_string(this->database.getMaxAllowedPacketSize())
				+ "]."
		);
	}

	// destructor
	Server::~Server() {
		// interrupt module threads
		for(auto& crawler : this->crawlers)
			crawler->Module::Thread::interrupt();

		for(auto& parser : this->parsers)
			parser->Module::Thread::interrupt();

		for(auto& extractor : this->extractors)
			extractor->Module::Thread::interrupt();

		for(auto& analyzer : this->analyzers)
			analyzer->Module::Thread::interrupt();

		// wait for module threads
		for(auto& crawler : this->crawlers) {
			if(crawler) {
				// save the ID of the thread before ending it
				const unsigned long id = crawler->getId();

				// wait for thread
				crawler->Module::Thread::end();

				// log interruption
				const std::string logString(
						"crawler #"
						+ std::to_string(id)
						+ " interrupted."
				);

				try {
					this->database.log(logString);
				}
				catch(const DatabaseException& e) {
					std::cout	<< '\n'
								<< logString
								<< "\nCould not write to log: "
								<< e.what()
								<< std::flush;
				}
			}
		}

		this->crawlers.clear();

		for(auto& parser : this->parsers) {
			if(parser) {
				// save the ID of the thread before ending it
				const unsigned long id = parser->getId();

				// wait for thread
				parser->Module::Thread::end();

				// log interruption
				const std::string logString(
						"parser #"
						+ std::to_string(id)
						+ " interrupted."
				);

				try {
					this->database.log(logString);
				}
				catch(const DatabaseException& e) {
					std::cout	<< '\n'
								<< logString
								<< "\nCould not write to log: "
								<< e.what()
								<< std::flush;
				}
			}
		}

		this->parsers.clear();

		for(auto& extractor : this->extractors) {
			if(extractor) {
				// save the ID of the thread before ending it
				const unsigned long id = extractor->getId();

				// wait for thread
				extractor->Module::Thread::end();

				// log interruption
				const std::string logString(
						"extractor #"
						+ std::to_string(id)
						+ " interrupted."
				);

				try {
					this->database.log(logString);
				}
				catch(const DatabaseException& e) {
					std::cout	<< '\n'
								<< logString
								<< "\nCould not write to log: "
								<< e.what()
								<< std::flush;
				}
			}
		}

		this->extractors.clear();

		for(auto& analyzer : this->analyzers) {
			if(analyzer) {
				// save the ID of the thread before ending it
				const unsigned long id = analyzer->getId();

				// wait for thread
				analyzer->Module::Thread::end();

				// log interruption
				const std::string logString(
						"analyzer #"
						+ std::to_string(id)
						+ " interrupted."
				);

				try {
					this->database.log(logString);
				}
				catch(const DatabaseException& e) {
					std::cout	<< '\n'
								<< logString
								<< "\nCould not write to log: "
								<< e.what()
								<< std::flush;
				}
			}
		}

		this->analyzers.clear();

		// wait for worker threads
		for(auto& worker : this->workers)
			if(worker.joinable())
				worker.join();

		this->workers.clear();

		// log shutdown message with server up-time
		try {
			this->database.log("shuts down after up-time of "
					+ Helper::DateTime::secondsToString(this->getUpTime()) + ".");
		}
		catch(const DatabaseException& e) {
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
		catch(const DatabaseException& e) {
			// try to re-connect once on database exception
			try {
				this->database.checkConnection();
				this->database.log("re-connected to database after error: " + e.whatStr());
			}
			catch(const DatabaseException& e) {
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

		for(unsigned long n = 1; n <= this->extractors.size(); ++n) {
			if(this->extractors.at(n - 1)->isShutdown() && this->extractors.at(n - 1)->isFinished()) {
				--n;

				this->extractors.at(n)->Module::Thread::end();

				this->extractors.erase(this->extractors.begin() + n);
			}
		}

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
			catch(const DatabaseException &e) {}
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

		// count active extractors
		result += std::count_if(
				this->extractors.begin(),
				this->extractors.end(),
				[](const auto& thread) {
						return thread->Module::Thread::isRunning();
				}
		);

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

		return std::count(
				this->workersRunning.begin(),
				this->workersRunning.end(),
				true
		);
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

			this->cmdIp = WebServer::getIP(connection);

			// parse JSON
			try {
				this->cmdJson = Helper::Json::parseRapid(msgBody);

				if(!(this->cmdJson.IsObject()))
					response = ServerCommandResponse::failed("Parsed JSON is not an object.");
			}
			catch(const JsonException& e) {
				response = ServerCommandResponse::failed("Could not parse JSON: " + e.whatStr() + ".");
			}

			if(!response.fail){
				// get server command
				if(this->cmdJson.HasMember("cmd")) {
					if(this->cmdJson["cmd"].IsString()) {
						try {
							// handle server commands
							const std::string command(
									this->cmdJson["cmd"].GetString(),
									this->cmdJson["cmd"].GetStringLength()
							);

							if(!(this->cmd(command, response))) {
								if(command == "import") {
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
												this,
												connection,
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
								else if(command == "testquery") {
									// create a worker thread for testing query (to not block the server)
									{
										std::lock_guard<std::mutex> workersLocked(this->workersLock);

										this->workersRunning.push_back(true);

										this->workers.emplace_back(
												&Server::cmdTestQuery,
												this,
												connection,
												this->workers.size(),
												msgBody
										);
									}

									threadStartedTo = true;
								}
								else if(command == "download") {
									response = this->cmdDownload();

									if(!response.fail) {
										fileDownloadTo = true;

										return response.text;
									}
								}
								else if(command == "ping")
									response = ServerCommandResponse("pong");
								else if(!command.empty())
									// unknown command: debug the command and its arguments
									response = ServerCommandResponse::failed("Unknown command \'" + command + "\'.");
								else
									response = ServerCommandResponse::failed("Empty command.");
							}
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

			// clear IP and JSON
			std::string().swap(this->cmdIp);

			rapidjson::Value(rapidjson::kObjectType).Swap(this->cmdJson);
		}

		// return the reply
		return Server::generateReply(response, msgBody);
	}

	// perform a basic server command, return whether such a command has been found, throws Main::Exception
	//  NOTE: Server::cmdJson and Server::cmdIp need to be set!
	bool Server::cmd(const std::string& name, ServerCommandResponse& response) {
		MAIN_SERVER_BASIC_CMD("kill", this->cmdKill);
		MAIN_SERVER_BASIC_CMD("allow", this->cmdAllow);
		MAIN_SERVER_BASIC_CMD("disallow", this->cmdDisallow);

		MAIN_SERVER_BASIC_CMD("log", this->cmdLog);
		MAIN_SERVER_BASIC_CMD("clearlogs", this->cmdClearLogs);

		MAIN_SERVER_BASIC_CMD("startcrawler", this->cmdStartCrawler);
		MAIN_SERVER_BASIC_CMD("pausecrawler", this->cmdPauseCrawler);
		MAIN_SERVER_BASIC_CMD("unpausecrawler", this->cmdUnpauseCrawler);
		MAIN_SERVER_BASIC_CMD("stopcrawler", this->cmdStopCrawler);

		MAIN_SERVER_BASIC_CMD("startparser", this->cmdStartParser);
		MAIN_SERVER_BASIC_CMD("pauseparser", this->cmdPauseParser);
		MAIN_SERVER_BASIC_CMD("unpauseparser", this->cmdUnpauseParser);
		MAIN_SERVER_BASIC_CMD("stopparser", this->cmdStopParser);
		MAIN_SERVER_BASIC_CMD("resetparsingstatus", this->cmdResetParsingStatus);

		MAIN_SERVER_BASIC_CMD("startextractor", this->cmdStartExtractor);
		MAIN_SERVER_BASIC_CMD("pauseextractor", this->cmdPauseExtractor);
		MAIN_SERVER_BASIC_CMD("unpauseextractor", this->cmdUnpauseExtractor);
		MAIN_SERVER_BASIC_CMD("stopextractor", this->cmdStopExtractor);
		MAIN_SERVER_BASIC_CMD("resetextractingstatus", this->cmdResetExtractingStatus);

		MAIN_SERVER_BASIC_CMD("startanalyzer", this->cmdStartAnalyzer);
		MAIN_SERVER_BASIC_CMD("pauseanalyzer", this->cmdPauseAnalyzer);
		MAIN_SERVER_BASIC_CMD("unpauseanalyzer", this->cmdUnpauseAnalyzer);
		MAIN_SERVER_BASIC_CMD("stopanalyzer", this->cmdStopAnalyzer);
		MAIN_SERVER_BASIC_CMD("resetanalyzingstatus", this->cmdResetAnalyzingStatus);

		MAIN_SERVER_BASIC_CMD("pauseall", this->cmdPauseAll);
		MAIN_SERVER_BASIC_CMD("unpauseall", this->cmdUnpauseAll);

		MAIN_SERVER_BASIC_CMD("addwebsite", this->cmdAddWebsite);
		MAIN_SERVER_BASIC_CMD("updatewebsite", this->cmdUpdateWebsite);
		MAIN_SERVER_BASIC_CMD("deletewebsite", this->cmdDeleteWebsite);
		MAIN_SERVER_BASIC_CMD("duplicatewebsite", this->cmdDuplicateWebsite);

		MAIN_SERVER_BASIC_CMD("addurllist", this->cmdAddUrlList);
		MAIN_SERVER_BASIC_CMD("updateurllist", this->cmdUpdateUrlList);
		MAIN_SERVER_BASIC_CMD("deleteurllist", this->cmdDeleteUrlList);
		MAIN_SERVER_BASIC_CMD("deleteurls", this->cmdDeleteUrls);

		MAIN_SERVER_BASIC_CMD("addquery", this->cmdAddQuery);
		MAIN_SERVER_BASIC_CMD("updatequery", this->cmdUpdateQuery);
		MAIN_SERVER_BASIC_CMD("deletequery", this->cmdDeleteQuery);
		MAIN_SERVER_BASIC_CMD("duplicatequery", this->cmdDuplicateQuery);

		MAIN_SERVER_BASIC_CMD("addconfig", this->cmdAddConfig);
		MAIN_SERVER_BASIC_CMD("updateconfig", this->cmdUpdateConfig);
		MAIN_SERVER_BASIC_CMD("deleteconfig", this->cmdDeleteConfig);
		MAIN_SERVER_BASIC_CMD("duplicateconfig", this->cmdDuplicateConfig);

		MAIN_SERVER_BASIC_CMD("warp", this->cmdWarp);

		return false;
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
					catch(const DatabaseException& e) {
						// try to re-connect once on database exception
						try {
							this->database.checkConnection();

							this->database.log("re-connected to database after error: " + e.whatStr());
							this->database.log("rejected client " + ip + ".");
						}
						catch(const DatabaseException& e) {
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
					catch(const DatabaseException& e) {
						// try to re-connect once on database exception
						try {
							this->database.checkConnection();

							this->database.log("re-connected to database after error: " + e.whatStr());
							this->database.log("accepted client " + ip + ".");
						}
						catch(const DatabaseException& e) {
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

			const std::string reply(
					this->cmd(
							connection,
							body,
							threadStarted,
							fileDownload
					)
			);

			// send reply
			if(fileDownload)
				this->webServer.sendFile(connection, reply, data);
			else if(!threadStarted)
				this->webServer.send(connection, 200, "application/json", reply);
		}
		else if(method == "OPTIONS")
			this->webServer.send(connection, 200, "", "");
	}

	// server command kill: kill the server
	Server::ServerCommandResponse Server::cmdKill() {
		// kill needs to be confirmed
		if(this->cmdJson.HasMember("confirmed")) {
			// kill server
			this->running = false;

			// kill is a logged command
			this->database.log("killed by " + this->cmdIp + ".");

			// send bye message
			return ServerCommandResponse("Bye bye.");
		}

		return ServerCommandResponse::toBeConfirmed("Are you sure to kill the server?");
	}

	// server command allow(ip): allow acces for the specified IP(s)
	Server::ServerCommandResponse Server::cmdAllow() {
		// get argument
		if(!(this->cmdJson.HasMember("ip")))
			return ServerCommandResponse::failed("Invalid arguments (\'ip\' is missing).");

		if(!(this->cmdJson["ip"].IsString()))
			return ServerCommandResponse::failed("Invalid arguments (\'ip\' is not a string).");

		const std::string toAllow(
				this->cmdJson["ip"].GetString(),
				this->cmdJson["ip"].GetStringLength()
		);

		if(toAllow.empty())
			return ServerCommandResponse::failed("Invalid arguments (\'ip\' is empty).");

		// allow needs to be confirmed
		if(this->cmdJson.HasMember("confirmed")) {
			// add IP(s)
			this->allowed += "," + toAllow;

			// allow is a logged command
			this->database.log(toAllow + " allowed by " + this->cmdIp + ".");

			return ServerCommandResponse("Allowed IPs: " + this->allowed + ".");
		}

		return ServerCommandResponse::toBeConfirmed(
				"Do you really want to allow " + toAllow + " access to the server?"
		);
	}

	// server command disallow: revoke access from all except the initial IP(s) specified by the configuration file
	Server::ServerCommandResponse Server::cmdDisallow() {
		// reset alled IP(s)
		this->allowed = this->settings.allowedClients;

		// disallow is a logged command
		this->database.log("Allowed IPs reset by " + this->cmdIp + ".");

		return ServerCommandResponse("Allowed IP(s): " + this->allowed + ".");
	}

	// server command log(entry): write a log entry by the frontend into the database
	Server::ServerCommandResponse Server::cmdLog() {
		// get argument
		if(!(this->cmdJson.HasMember("entry")))
			return ServerCommandResponse::failed("Invalid arguments (\'entry\' is missing).");

		if(!(this->cmdJson["entry"].IsString()))
			return ServerCommandResponse::failed("Invalid arguments (\'entry\' is not a string).");

		const std::string entry(
				this->cmdJson["entry"].GetString(),
				this->cmdJson["entry"].GetStringLength()
		);

		// write log entry
		this->database.log("frontend", entry);

		return ServerCommandResponse("Wrote log entry: " + entry);
	}

	// server command clearlog([module]):	remove all log entries or the log entries of a specific module
	// 										(or all log entries when module is empty/not given)
	Server::ServerCommandResponse Server::cmdClearLogs() {
		// check whether the clearing of logs is allowed
		if(!(this->settings.logsDeletable))
			return ServerCommandResponse::failed("Not allowed.");

		// get argument
		std::string module;

		if(this->cmdJson.HasMember("module") && this->cmdJson["module"].IsString())
			module = std::string(
					this->cmdJson["module"].GetString(),
					this->cmdJson["module"].GetStringLength()
			);

		// clearlog needs to be confirmed
		if(this->cmdJson.HasMember("confirmed")) {
			this->database.clearLogs(module);

			// clearlog is a logged command
			if(!module.empty()) {
				this->database.log("Logs of " + module + " cleared by " + this->cmdIp + ".");

				return ServerCommandResponse("Logs of " + module + " cleared.");
			}

			this->database.log("All logs cleared by " + this->cmdIp + ".");

			return ServerCommandResponse("All logs cleared.");
		}

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

	// server command startcrawler(website, urllist, config):
	//  start a crawler using the specified website, URL list and configuration
	Server::ServerCommandResponse Server::cmdStartCrawler() {
		// get arguments
		if(!(this->cmdJson.HasMember("website")))
			return ServerCommandResponse::failed("Invalid arguments (\'website\' is missing).");

		if(!(this->cmdJson["website"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'website\' is not a valid number).");

		if(!(this->cmdJson.HasMember("urllist")))
			return ServerCommandResponse::failed("Invalid arguments (\'urllist\' is missing).");

		if(!(this->cmdJson["urllist"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'urllist\' is not a valid number).");

		if(!(this->cmdJson.HasMember("config")))
			return ServerCommandResponse::failed("Invalid arguments (\'config\' is missing).");

		if(!(this->cmdJson["config"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'config\' is not a valid number).");

		const ThreadOptions options(
				"crawler",
				this->cmdJson["website"].GetUint64(),
				this->cmdJson["urllist"].GetUint64(),
				this->cmdJson["config"].GetUint64()
		);

		// check arguments
		if(!(this->database.isWebsite(options.website)))
			return ServerCommandResponse::failed(
					"Website #"
					+ std::to_string(options.website)
					+ " not found."
			);

		if(!(this->database.isUrlList(options.website, options.urlList)))
			return ServerCommandResponse::failed(
					"URL list #"
					+ std::to_string(options.urlList)
					+ " for website #"
					+ std::to_string(options.website)
					+ " not found."
			);

		if(!(this->database.isConfiguration(options.website, options.config)))
			return ServerCommandResponse::failed(
					"Configuration #"
					+ std::to_string(options.config)
					+ " for website #"
					+ std::to_string(options.website)
					+ " not found."
			);

		// create crawler
		this->crawlers.push_back(
				std::make_unique<Module::Crawler::Thread>(
						this->database,
						this->dirCookies,
						options,
						this->netSettings
				)
		);

		// start crawler
		this->crawlers.back()->Module::Thread::start();

		// startcrawler is a logged command
		this->database.log(
				"crawler #"
				+ std::to_string(this->crawlers.back()->Module::Thread::getId())
				+ " started by "
				+ this->cmdIp
				+ "."
		);

		return ServerCommandResponse("Crawler has been started.");
	}

	// server command pausecrawler(id): pause a crawler by its ID
	Server::ServerCommandResponse Server::cmdPauseCrawler() {
		// get argument
		if(!(this->cmdJson.HasMember("id")))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is missing).");

		if(!(this->cmdJson["id"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is not a valid number).");

		const unsigned long id = this->cmdJson["id"].GetUint64();

		// find crawler
		auto i = std::find_if(this->crawlers.begin(), this->crawlers.end(),
				[&id](const auto& p) {
					return p->Module::Thread::getId() == id;
				}
		);

		if(i == this->crawlers.end())
			return ServerCommandResponse::failed(
					"Could not find crawler #"
					+ std::to_string(id)
					+ "."
			);

		// pause crawler
		(*i)->Module::Thread::pause();

		// pausecrawler is a logged command
		this->database.log(
				"crawler #"
				+ std::to_string(id)
				+ " paused by "
				+ this->cmdIp
				+ "."
		);

		return ServerCommandResponse("Crawler is pausing.");
	}

	// server command unpausecrawler(id): unpause a crawler by its ID
	Server::ServerCommandResponse Server::cmdUnpauseCrawler() {
		// get argument
		if(!(this->cmdJson.HasMember("id")))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is missing).");

		if(!(this->cmdJson["id"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is not a valid number).");

		const unsigned long id = this->cmdJson["id"].GetUint64();

		// find crawler
		auto i = std::find_if(this->crawlers.begin(), this->crawlers.end(),
				[&id](const auto& p) {
					return p->Module::Thread::getId() == id;
				}
		);

		if(i == this->crawlers.end())
			return ServerCommandResponse::failed(
					"Could not find crawler #"
					+ std::to_string(id)
					+ "."
			);

		// unpause crawler
		(*i)->Module::Thread::unpause();

		// unpausecrawler is a logged command
		this->database.log(
				"crawler #"
				+ std::to_string(id)
				+ " unpaused by "
				+ this->cmdIp
				+ "."
		);

		return ServerCommandResponse("Crawler is unpausing.");
	}

	// server command stopcrawler(id): stop a crawler by its ID
	Server::ServerCommandResponse Server::cmdStopCrawler() {
		// get argument
		if(!(this->cmdJson.HasMember("id")))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is missing).");

		if(!(this->cmdJson["id"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is not a valid number).");

		const unsigned long id = this->cmdJson["id"].GetUint64();

		// find crawler
		auto i = std::find_if(this->crawlers.begin(), this->crawlers.end(),
				[&id](const auto& p) {
					return p->Module::Thread::getId() == id;
				}
		);

		if(i == this->crawlers.end())
			return ServerCommandResponse::failed(
					"Could not find crawler #"
					+ std::to_string(id)
					+ "."
			);

		// interrupt crawler
		(*i)->Module::Thread::stop();

		// stopcrawler is a logged command
		this->database.log(
				"crawler #"
				+ std::to_string(id)
				+ " stopped by "
				+ this->cmdIp
				+ "."
		);

		return ServerCommandResponse("Crawler is stopping.");
	}

	// server command startparser(website, urllist, config):
	//  start a parser using the specified website, URL list and configuration
	Server::ServerCommandResponse Server::cmdStartParser() {
		// get arguments
		if(!(this->cmdJson.HasMember("website")))
			return ServerCommandResponse::failed("Invalid arguments (\'website\' is missing).");

		if(!(this->cmdJson["website"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'website\' is not a valid number).");

		if(!(this->cmdJson.HasMember("urllist")))
			return ServerCommandResponse::failed("Invalid arguments (\'urllist\' is missing).");

		if(!(this->cmdJson["urllist"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'urllist\' is not a valid number).");

		if(!(this->cmdJson.HasMember("config")))
			return ServerCommandResponse::failed("Invalid arguments (\'config\' is missing).");

		if(!(this->cmdJson["config"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'config\' is not a valid number).");

		const ThreadOptions options(
				"parser",
				this->cmdJson["website"].GetUint64(),
				this->cmdJson["urllist"].GetUint64(),
				this->cmdJson["config"].GetUint64()
		);

		// check arguments
		if(!(this->database.isWebsite(options.website)))
			return ServerCommandResponse::failed(
					"Website #"
					+ std::to_string(options.website)
					+ " not found."
			);

		if(!(this->database.isUrlList(options.website, options.urlList)))
			return ServerCommandResponse::failed(
					"URL list #"
					+ std::to_string(options.urlList)
					+ " for website #"
					+ std::to_string(options.website)
					+ " not found."
			);

		if(!(this->database.isConfiguration(options.website, options.config)))
			return ServerCommandResponse::failed(
					"Configuration #"
					+ std::to_string(options.config)
					+ " for website #"
					+ std::to_string(options.website)
					+ " not found."
			);

		// create parser
		this->parsers.push_back(
				std::make_unique<Module::Parser::Thread>(
						this->database,
						options
				)
		);

		// start parser
		this->parsers.back()->Module::Thread::start();

		// startparser is a logged command
		this->database.log(
				"parser #"
				+ std::to_string(this->parsers.back()->Module::Thread::getId())
				+ " started by "
				+ this->cmdIp
				+ "."
		);

		return ServerCommandResponse("Parser has been started.");
	}

	// server command pauseparser(id): pause a parser by its ID
	Server::ServerCommandResponse Server::cmdPauseParser() {
		// get argument
		if(!(this->cmdJson.HasMember("id")))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is missing).");

		if(!(this->cmdJson["id"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is not a valid number).");

		const unsigned long id = this->cmdJson["id"].GetUint64();

		// find parser
		auto i = std::find_if(this->parsers.begin(), this->parsers.end(),
				[&id](const auto& p) {
					return p->Module::Thread::getId() == id;
				}
		);

		if(i == this->parsers.end())
			return ServerCommandResponse::failed(
					"Could not find parser #"
					+ std::to_string(id)
					+ "."
			);

		// pause parser
		(*i)->Module::Thread::pause();

		// pauseparser is a logged command
		this->database.log(
				"parser #"
				+ std::to_string(id)
				+ " paused by "
				+ this->cmdIp
				+ "."
		);

		return ServerCommandResponse("Parser is pausing.");
	}

	// server command unpauseparser(id): unpause a parser by its ID
	Server::ServerCommandResponse Server::cmdUnpauseParser() {
		// get argument
		if(!(this->cmdJson.HasMember("id")))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is missing).");

		if(!(this->cmdJson["id"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is not a valid number).");

		const unsigned long id = this->cmdJson["id"].GetUint64();

		// find parser
		auto i = std::find_if(this->parsers.begin(), this->parsers.end(),
				[&id](const auto& p) {
					return p->Module::Thread::getId() == id;
				}
		);

		if(i == this->parsers.end())
			return ServerCommandResponse::failed(
					"Could not find parser #"
					+ std::to_string(id)
					+ "."
			);

		// unpause parser
		(*i)->Module::Thread::unpause();

		// unpauseparser is a logged command
		this->database.log(
				"parser #"
				+ std::to_string(id)
				+ " unpaused by "
				+ this->cmdIp
				+ "."
		);

		return ServerCommandResponse("Parser is unpausing.");
	}

	// server command stopparser(id): stop a parser by its ID
	Server::ServerCommandResponse Server::cmdStopParser() {
		// get argument
		if(!(this->cmdJson.HasMember("id")))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is missing).");

		if(!(this->cmdJson["id"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is not a valid number).");

		const unsigned long id = this->cmdJson["id"].GetUint64();

		// find parser
		auto i = std::find_if(this->parsers.begin(), this->parsers.end(),
				[&id](const auto& p) {
					return p->Module::Thread::getId() == id;
				}
		);

		if(i == this->parsers.end())
			return ServerCommandResponse::failed(
					"Could not find parser #"
					+ std::to_string(id)
					+ "."
			);

		// interrupt parser
		(*i)->Module::Thread::stop();

		// stopparser is a logged command
		this->database.log(
				"parser #"
				+ std::to_string(id)
				+ " stopped by "
				+ this->cmdIp
				+ "."
		);

		return ServerCommandResponse("Parser is stopping.");
	}

	// server command resetparsingstatus(urllist): reset the parsing status of a ID-specificed URL list
	Server::ServerCommandResponse Server::cmdResetParsingStatus() {
		// get argument
		if(!(this->cmdJson.HasMember("urllist")))
			return ServerCommandResponse::failed("Invalid arguments (\'urllist\' is missing).");

		if(!(this->cmdJson["urllist"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'urllist\' is not a valid number).");

		// resetparsingstatus needs to be confirmed
		if(this->cmdJson.HasMember("confirmed")) {
			// reset parsing status
			this->database.resetParsingStatus(this->cmdJson["urllist"].GetUint64());

			return ServerCommandResponse("Parsing status reset.");
		}

		return ServerCommandResponse::toBeConfirmed(
				"Are you sure that you want to reset the parsing status of this URL list?"
		);
	}

	// server command startextractor(website, urllist, config):
	//  start an extractor using the specified website, URL list and configuration
	Server::ServerCommandResponse Server::cmdStartExtractor() {
		// get arguments
		if(!(this->cmdJson.HasMember("website")))
			return ServerCommandResponse::failed("Invalid arguments (\'website\' is missing).");

		if(!(this->cmdJson["website"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'website\' is not a valid number).");

		if(!(this->cmdJson.HasMember("urllist")))
			return ServerCommandResponse::failed("Invalid arguments (\'urllist\' is missing).");

		if(!(this->cmdJson["urllist"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'urllist\' is not a valid number).");

		if(!(this->cmdJson.HasMember("config")))
			return ServerCommandResponse::failed("Invalid arguments (\'config\' is missing).");

		if(!(this->cmdJson["config"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'config\' is not a valid number).");

		const ThreadOptions options(
				"extractor",
				this->cmdJson["website"].GetUint64(),
				this->cmdJson["urllist"].GetUint64(),
				this->cmdJson["config"].GetUint64()
		);

		// check arguments
		if(!(this->database.isWebsite(options.website)))
			return ServerCommandResponse::failed(
					"Website #"
					+ std::to_string(options.website)
					+ " not found."
			);

		if(!(this->database.isUrlList(options.website, options.urlList)))
			return ServerCommandResponse::failed(
					"URL list #"
					+ std::to_string(options.urlList)
					+ " for website #"
					+ std::to_string(options.website)
					+ " not found."
			);

		if(!(this->database.isConfiguration(options.website, options.config)))
			return ServerCommandResponse::failed(
					"Configuration #"
					+ std::to_string(options.config)
					+ " for website #"
					+ std::to_string(options.website)
					+ " not found."
			);

		// create extractor
		this->extractors.push_back(
				std::make_unique<Module::Extractor::Thread>(
						this->database,
						this->dirCookies,
						options,
						this->netSettings
				)
		);

		// start extractor
		this->extractors.back()->Module::Thread::start();

		// startextractor is a logged command
		this->database.log(
				"extractor #"
				+ std::to_string(this->extractors.back()->Module::Thread::getId())
				+ " started by "
				+ this->cmdIp
				+ "."
		);

		return ServerCommandResponse("Extractor has been started.");
	}

	// server command pauseextractor(id): pause an extractor by its ID
	Server::ServerCommandResponse Server::cmdPauseExtractor() {
		// get argument
		if(!(this->cmdJson.HasMember("id")))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is missing).");

		if(!(this->cmdJson["id"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is not a valid number).");

		const unsigned long id = this->cmdJson["id"].GetUint64();

		// find extractor
		auto i = std::find_if(this->extractors.begin(), this->extractors.end(),
				[&id](const auto& p) {
					return p->Module::Thread::getId() == id;
				}
		);

		if(i == this->extractors.end())
			return ServerCommandResponse::failed(
					"Could not find extractor #"
					+ std::to_string(id)
					+ "."
			);

		// pause parser
		(*i)->Module::Thread::pause();

		// pauseextractor is a logged command
		this->database.log(
				"extractor #"
				+ std::to_string(id)
				+ " paused by "
				+ this->cmdIp
				+ "."
		);

		return ServerCommandResponse("Extractor is pausing.");
	}

	// server command unpauseextractor(id): unpause an extractor by its ID
	Server::ServerCommandResponse Server::cmdUnpauseExtractor() {
		// get argument
		if(!(this->cmdJson.HasMember("id")))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is missing).");

		if(!(this->cmdJson["id"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is not a valid number).");

		const unsigned long id = this->cmdJson["id"].GetUint64();

		// find extractor
		auto i = std::find_if(this->extractors.begin(), this->extractors.end(),
				[&id](const auto& p) {
					return p->Module::Thread::getId() == id;
				}
		);

		if(i == this->extractors.end())
			return ServerCommandResponse::failed(
					"Could not find extractor #"
					+ std::to_string(id)
					+ "."
			);

		// unpause extractor
		(*i)->Module::Thread::unpause();

		// unpauseextractor is a logged command
		this->database.log(
				"extractor #"
				+ std::to_string(id)
				+ " unpaused by "
				+ this->cmdIp
				+ "."
		);

		return ServerCommandResponse("Extractor is unpausing.");
	}

	// server command stopextractor(id): stop an extractor by its ID
	Server::ServerCommandResponse Server::cmdStopExtractor() {
		// get argument
		if(!(this->cmdJson.HasMember("id")))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is missing).");

		if(!(this->cmdJson["id"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is not a valid number).");

		const unsigned long id = this->cmdJson["id"].GetUint64();

		// find extractor
		auto i = std::find_if(this->extractors.begin(), this->extractors.end(),
				[&id](const auto& p) {
					return p->Module::Thread::getId() == id;
				}
		);

		if(i == this->extractors.end())
			return ServerCommandResponse::failed(
					"Could not find extractor #"
					+ std::to_string(id)
					+ "."
			);

		// interrupt extractor
		(*i)->Module::Thread::stop();

		// stopextractor is a logged command
		this->database.log(
				"extractor #"
				+ std::to_string(id)
				+ " stopped by "
				+ this->cmdIp
				+ "."
		);

		return ServerCommandResponse("Extractor is stopping.");
	}

	// server command resetextractingstatus(urllist): reset the parsing status of a ID-specificed URL list
	Server::ServerCommandResponse Server::cmdResetExtractingStatus() {
		// get argument
		if(!(this->cmdJson.HasMember("urllist")))
			return ServerCommandResponse::failed("Invalid arguments (\'urllist\' is missing).");

		if(!(this->cmdJson["urllist"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'urllist\' is not a valid number).");

		// resetextractingstatus needs to be confirmed
		if(this->cmdJson.HasMember("confirmed")) {
			// reset extracting status
			this->database.resetExtractingStatus(this->cmdJson["urllist"].GetUint64());

			return ServerCommandResponse("Extracting status reset.");
		}

		return ServerCommandResponse::toBeConfirmed(
				"Are you sure that you want to reset the extracting status of this URL list?"
		);
	}

	// server command startanalyzer(website, urllist, config):
	//  start an analyzer using the specified website, URL list, module and configuration
	Server::ServerCommandResponse Server::cmdStartAnalyzer() {
		// get arguments
		if(!(this->cmdJson.HasMember("website")))
			return ServerCommandResponse::failed("Invalid arguments (\'website\' is missing).");

		if(!(this->cmdJson["website"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'website\' is not a valid number).");

		if(!(this->cmdJson.HasMember("urllist")))
			return ServerCommandResponse::failed("Invalid arguments (\'urllist\' is missing).");

		if(!(this->cmdJson["urllist"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'urllist\' is not a valid number).");

		if(!(this->cmdJson.HasMember("config")))
			return ServerCommandResponse::failed("Invalid arguments (\'config\' is missing).");

		if(!(this->cmdJson["config"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'config\' is not a valid number).");

		const ThreadOptions options(
				"analyzer",
				this->cmdJson["website"].GetUint64(),
				this->cmdJson["urllist"].GetUint64(),
				this->cmdJson["config"].GetUint64()
		);

		// check arguments
		if(!(this->database.isWebsite(options.website)))
			return ServerCommandResponse::failed(
					"Website #"
					+ std::to_string(options.website)
					+ " not found."
			);

		if(!(this->database.isUrlList(options.website, options.urlList)))
			return ServerCommandResponse::failed(
					"URL list #"
					+ std::to_string(options.urlList)
					+ " for website #"
					+ std::to_string(options.website)
					+ " not found."
			);

		if(!(this->database.isConfiguration(options.website, options.config)))
			return ServerCommandResponse::failed(
					"Configuration #"
					+ std::to_string(options.config)
					+ " for website #"
					+ std::to_string(options.website)
					+ " not found."
			);

		// get configuration
		const std::string config(this->database.getConfiguration(options.config));

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
				Module::Analyzer::Algo::initAlgo(
						AlgoThreadProperties(
								algo,
								this->database,
								options
						)
				)
		);

		if(!(this->analyzers.back())) {
			this->analyzers.pop_back();

			return ServerCommandResponse::failed(
					"Algorithm #"
					+ std::to_string(algo)
					+ " not found."
			);
		}

		// start algorithm
		this->analyzers.back()->Module::Thread::start();

		// startanalyzer is a logged command
		this->database.log(
				"analyzer #"
				+ std::to_string(this->analyzers.back()->Module::Thread::getId())
				+ " started by "
				+ this->cmdIp
				+ "."
		);

		return ServerCommandResponse("Analyzer has been started.");
	}

	// server command pauseparser(id): pause a parser by its ID
	Server::ServerCommandResponse Server::cmdPauseAnalyzer() {
		// get argument
		if(!(this->cmdJson.HasMember("id")))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is missing).");

		if(!(this->cmdJson["id"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is not a valid number).");

		const unsigned long id = this->cmdJson["id"].GetUint64();

		// find analyzer
		auto i = std::find_if(this->analyzers.begin(), this->analyzers.end(),
				[&id](const auto& p) {
					return p->Module::Thread::getId() == id;
				}
		);

		if(i == this->analyzers.end())
			return ServerCommandResponse::failed(
					"Could not find analyzer #"
					+ std::to_string(id)
					+ "."
			);

		// pause analyzer
		if((*i)->Module::Thread::pause()) {
			// pauseanalyzer is a logged command
			this->database.log(
					"analyzer #"
					+ std::to_string(id)
					+ " paused by "
					+ this->cmdIp
					+ "."
			);

			return ServerCommandResponse("Analyzer is pausing.");
		}

		// analyzer is not pausable
		return ServerCommandResponse::failed("This algorithm cannot be paused at the moment.");
	}

	// server command unpauseanalyzer(id): unpause a parser by its ID
	Server::ServerCommandResponse Server::cmdUnpauseAnalyzer() {
		// get argument
		if(!(this->cmdJson.HasMember("id")))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is missing).");

		if(!(this->cmdJson["id"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is not a valid number).");

		const unsigned long id = this->cmdJson["id"].GetUint64();

		// find analyzer
		auto i = std::find_if(this->analyzers.begin(), this->analyzers.end(),
				[&id](const auto& p) {
					return p->Module::Thread::getId() == id;
				}
		);

		if(i == this->analyzers.end())
			return ServerCommandResponse::failed(
					"Could not find analyzer #"
					+ std::to_string(id)
					+ "."
			);

		// unpause analyzer
		(*i)->Module::Thread::unpause();

		// unpauseanalyzer is a logged command
		this->database.log(
				"analyzer #"
				+ std::to_string(id)
				+ " unpaused by "
				+ this->cmdIp
				+ "."
		);

		return ServerCommandResponse("Analyzer is unpausing.");
	}

	// server command stopanalyzer(id): stop a parser by its ID
	Server::ServerCommandResponse Server::cmdStopAnalyzer() {
		// get argument
		if(!(this->cmdJson.HasMember("id")))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is missing).");

		if(!(this->cmdJson["id"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is not a valid number).");

		const unsigned long id = this->cmdJson["id"].GetUint64();

		// find analyzer
		auto i = std::find_if(this->analyzers.begin(), this->analyzers.end(),
				[&id](const auto& p) {
					return p->Module::Thread::getId() == id;
				}
		);

		if(i == this->analyzers.end())
			return ServerCommandResponse::failed(
					"Could not find analyzer #"
					+ std::to_string(id)
					+ "."
			);

		// interrupt analyzer
		(*i)->Module::Thread::stop();

		// stopanalyzer is a logged command
		this->database.log(
				"analyzer #"
				+ std::to_string(id)
				+ " stopped by "
				+ this->cmdIp
				+ "."
		);

		return ServerCommandResponse("Analyzer is stopping.");
	}

	// server command resetanalyzingstatus(urllist): reset the parsing status of a ID-specificed URL list
	Server::ServerCommandResponse Server::cmdResetAnalyzingStatus() {
		// get argument
		if(!(this->cmdJson.HasMember("urllist")))
			return ServerCommandResponse::failed("Invalid arguments (\'urllist\' is missing).");

		if(!(this->cmdJson["urllist"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'urllist\' is not a valid number).");

		// resetanalyzingstatus needs to be confirmed
		if(this->cmdJson.HasMember("confirmed")) {
			// reset analyzing status
			this->database.resetAnalyzingStatus(this->cmdJson["urllist"].GetUint64());

			return ServerCommandResponse("Analyzing status reset.");
		}

		return ServerCommandResponse::toBeConfirmed(
				"Are you sure that you want to reset the analyzing status of this URL list?"
		);
	}

	// server command pauseall(): pause all running threads
	Server::ServerCommandResponse Server::cmdPauseAll() {
		unsigned long counter = 0;

		// pause crawlers
		for(const auto& thread : this->crawlers) {
			if(!(thread->isPaused())) {
				thread->Module::Thread::pause();

				this->database.log(
						"crawler #"
						+ std::to_string(thread->getId())
						+ " paused by "
						+ this->cmdIp
						+ "."
				);

				++counter;
			}
		}

		// pause parsers
		for(const auto& thread : this->parsers) {
			if(!(thread->isPaused())) {
				thread->Module::Thread::pause();

				this->database.log(
						"parser #"
						+ std::to_string(thread->getId())
						+ " paused by "
						+ this->cmdIp
						+ "."
				);

				++counter;
			}
		}

		// pause extractors
		for(const auto& thread : this->extractors) {
			if(!(thread->isPaused())) {
				thread->Module::Thread::pause();

				this->database.log(
						"extractor #"
						+ std::to_string(thread->getId())
						+ " paused by "
						+ this->cmdIp
						+ "."
				);

				++counter;
			}
		}

		// pause analyzers
		for(const auto& thread : this->analyzers) {
			if(!(thread->isPaused())) {
				thread->Module::Thread::pause();

				this->database.log(
						"analyzer #"
						+ std::to_string(thread->getId())
						+ " paused by "
						+ this->cmdIp
						+ "."
				);

				++counter;
			}
		}

		switch(counter) {
		case 0:
			return ServerCommandResponse("No threads to pause.");

		case 1:
			return ServerCommandResponse("One thread is pausing.");

		default:
			return ServerCommandResponse(std::to_string(counter) + " threads are pausing.");
		}
	}

	// server command unpauseall(): unpause all paused threads
	Server::ServerCommandResponse Server::cmdUnpauseAll() {
		unsigned long counter = 0;

		// unpause crawlers
		for(const auto& thread : this->crawlers) {
			if(thread->isPaused()) {
				thread->Module::Thread::unpause();

				this->database.log(
						"crawler #"
						+ std::to_string(thread->getId())
						+ " unpaused by "
						+ this->cmdIp
						+ "."
				);

				++counter;
			}
		}

		// unpause parsers
		for(const auto& thread : this->parsers) {
			if(thread->isPaused()) {
				thread->Module::Thread::unpause();

				this->database.log(
						"parser #"
						+ std::to_string(thread->getId())
						+ " unpaused by "
						+ this->cmdIp
						+ "."
				);

				++counter;
			}
		}

		// unpause extractors
		for(const auto& thread : this->extractors) {
			if(thread->isPaused()) {
				thread->Module::Thread::unpause();

				this->database.log(
						"extractor #"
						+ std::to_string(thread->getId())
						+ " unpaused by "
						+ this->cmdIp
						+ "."
				);

				++counter;
			}
		}

		// unpause analyzers
		for(const auto& thread : this->analyzers) {
			if(thread->isPaused()) {
				thread->Module::Thread::unpause();

				this->database.log(
						"analyzer #"
						+ std::to_string(thread->getId())
						+ " unpaused by "
						+ this->cmdIp
						+ "."
				);

				++counter;
			}
		}

		switch(counter) {
		case 0:
			return ServerCommandResponse("No threads to unpause.");

		case 1:
			return ServerCommandResponse("One thread has been unpaused.");

		default:
			return ServerCommandResponse(std::to_string(counter) + " threads have been unpaused.");
		}
	}

	// server command addwebsite([crossdomain], [domain], namespace, name, [dir]): add a website
	Server::ServerCommandResponse Server::cmdAddWebsite() {
		WebsiteProperties properties;
		bool crossDomain = false;
		unsigned long id = 0;

		// get arguments
		if(this->cmdJson.HasMember("crossdomain")) {
			if(!(this->cmdJson["crossdomain"].IsBool()))
				return ServerCommandResponse::failed("Invalid arguments (\'crossdomain\' is not a boolean).");

			crossDomain = this->cmdJson["crossdomain"].GetBool();
		}

		if(!crossDomain) {
			if(!(this->cmdJson.HasMember("domain")))
				return ServerCommandResponse::failed("Invalid arguments (\'domain\' is missing).");

			if(!(this->cmdJson["domain"].IsString()))
				return ServerCommandResponse::failed("Invalid arguments (\'domain\' is not a string).");

			properties.domain = std::string(
					this->cmdJson["domain"].GetString(),
					this->cmdJson["domain"].GetStringLength()
			);

			if(properties.domain.empty())
				return ServerCommandResponse::failed("Domain cannot be empty when website is not cross-domain.");
		}

		if(!(this->cmdJson.HasMember("namespace")))
			return ServerCommandResponse::failed("Invalid arguments (\'namespace\' is missing).");

		if(!(this->cmdJson["namespace"].IsString()))
			return ServerCommandResponse::failed("Invalid arguments (\'namespace\' is not a string).");

		properties.nameSpace = std::string(
				this->cmdJson["namespace"].GetString(),
				this->cmdJson["namespace"].GetStringLength()
		);

		if(!(this->cmdJson.HasMember("name")))
			return ServerCommandResponse::failed("Invalid arguments (\'name\' is missing).");

		if(!(this->cmdJson["name"].IsString()))
			return ServerCommandResponse::failed("Invalid arguments (\'name\' is not a string).");

		properties.name = std::string(
				this->cmdJson["name"].GetString(),
				this->cmdJson["name"].GetStringLength()
		);

		if(this->cmdJson.HasMember("dir")) {
			if(!(this->cmdJson["dir"].IsString()))
				return ServerCommandResponse::failed("Invalid arguments (\'dir\' is not a string).");

			properties.dir = std::string(
					this->cmdJson["dir"].GetString(),
					this->cmdJson["dir"].GetStringLength()
			);

			if(properties.dir.empty())
				return ServerCommandResponse::failed("External directory cannot be empty when used.");
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
			// adding a website using an external directory needs to be confirmed
			if(this->cmdJson.HasMember("confirmed")) {
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
			else
				return ServerCommandResponse::toBeConfirmed(
						"Do you really want to use an external directory?"
				);
		}

		if(!id)
			return ServerCommandResponse::failed("Could not add website to database.");

		return ServerCommandResponse("Website added.", id);
	}

	// server command updatewebsite(id, crossdomain, domain, namespace, name, [dir]): edit a website
	Server::ServerCommandResponse Server::cmdUpdateWebsite() {
		WebsiteProperties properties;
		bool crossDomain = false;

		// get arguments
		if(!(this->cmdJson.HasMember("id")))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is missing).");

		if(!(this->cmdJson["id"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is not a valid number).");

		const unsigned long id = this->cmdJson["id"].GetUint64();

		if(this->cmdJson.HasMember("crossdomain")) {
			if(!(this->cmdJson["crossdomain"].IsBool()))
				return ServerCommandResponse::failed("Invalid arguments (\'crossdomain\' is not a boolean).");

			crossDomain = this->cmdJson["crossdomain"].GetBool();
		}

		if(crossDomain) {
			if(
					this->cmdJson.HasMember("domain")
					&& this->cmdJson["domain"].IsString()
					&& this->cmdJson["domain"].GetStringLength()
			)
				return ServerCommandResponse::failed("Cannot use domain when website is cross-domain.");
		}
		else {
			if(!(this->cmdJson.HasMember("domain")))
				return ServerCommandResponse::failed("Invalid arguments (\'domain\' is missing).");

			if(!(this->cmdJson["domain"].IsString()))
				return ServerCommandResponse::failed("Invalid arguments (\'domain\' is not a string).");

			properties.domain = std::string(
					this->cmdJson["domain"].GetString(),
					this->cmdJson["domain"].GetStringLength()
			);

			if(properties.domain.empty())
				return ServerCommandResponse::failed("Domain cannot be empty when website is not cross-domain.");
		}

		if(!(this->cmdJson.HasMember("namespace")))
			return ServerCommandResponse::failed("Invalid arguments (\'namespace\' is missing).");

		if(!(this->cmdJson["namespace"].IsString()))
			return ServerCommandResponse::failed("Invalid arguments (\'namespace\' is not a string).");

		properties.nameSpace = std::string(
				this->cmdJson["namespace"].GetString(),
				this->cmdJson["namespace"].GetStringLength()
		);

		if(!(this->cmdJson.HasMember("name")))
			return ServerCommandResponse::failed("Invalid arguments (\'name\' is missing).");

		if(!(this->cmdJson["name"].IsString()))
			return ServerCommandResponse::failed("Invalid arguments (\'name\' is not a string).");

		properties.name = std::string(
				this->cmdJson["name"].GetString(),
				this->cmdJson["name"].GetStringLength()
		);

		if(this->cmdJson.HasMember("dir")) {
			if(!(this->cmdJson["dir"].IsString()))
				return ServerCommandResponse::failed("Invalid arguments (\'dir\' is not a string).");

			properties.dir = std::string(
					this->cmdJson["dir"].GetString(),
					this->cmdJson["dir"].GetStringLength()
			);

			if(properties.dir.empty())
				return ServerCommandResponse::failed("External directory cannot be empty when used.");
		}

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
		if(!(this->database.isWebsite(id)))
			return ServerCommandResponse::failed(
					"Website #"
					+ std::to_string(id)
					+ " not found."
			);

		// check whether any thread is using the website
		if(std::find_if(this->crawlers.begin(), this->crawlers.end(), [&id](const auto& p) {
			return p->getWebsite() == id;
		}) != this->crawlers.end())
			return ServerCommandResponse::failed(
					"Website cannot be changed while crawler is active."
			);

		if(std::find_if(this->parsers.begin(), this->parsers.end(), [&id](const auto& p) {
			return p->getWebsite() == id;
		}) != this->parsers.end())
			return ServerCommandResponse::failed(
					"Website cannot be changed while parser is active."
			);

		if(std::find_if(this->extractors.begin(), this->extractors.end(), [&id](const auto& p) {
			return p->getWebsite() == id;
		}) != this->extractors.end())
			return ServerCommandResponse::failed(
					"Website cannot be changed while extractor is active."
			);

		if(std::find_if(this->analyzers.begin(), this->analyzers.end(), [&id](const auto& p) {
			return p->getWebsite() == id;
		}) != this->analyzers.end())
			return ServerCommandResponse::failed(
					"Website cannot be changed while analyzer is active."
			);

		// check for domain and directory change
		const bool domainChanged = this->database.getWebsiteDomain(id) != properties.domain;
		const bool dirChanged = this->database.getWebsiteDataDirectory(id) != properties.dir;

		// check for confirmation if domain or directory have been changed
		if(this->cmdJson.HasMember("confirmed") || (!domainChanged && !dirChanged)) {
			// update website in database
			try {
				this->database.updateWebsite(id, properties);

				return ServerCommandResponse("Website updated.");
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

		// change of domain or directory needs to be confirmed
		std::ostringstream confirmationStrStr;

		// handle domain change
		if(domainChanged) {
			const unsigned long toModify = this->database.getChangedUrlsByWebsiteUpdate(id, properties);
			const unsigned long toDelete = this->database.getLostUrlsByWebsiteUpdate(id, properties);

			if(toModify || toDelete) {
				switch(toModify) {
				case 0:
					break;

				case 1:
					confirmationStrStr << "One URL will be modified.\n";

					break;

				default:
					confirmationStrStr.imbue(std::locale(""));

					confirmationStrStr << toModify << " URLs will be modified.\n";
				}

				switch(toDelete) {
				case 0:
					break;

				case 1:
					confirmationStrStr << "One URL will be IRRECOVERABLY LOST.\n";

					break;

				default:
					confirmationStrStr.imbue(std::locale(""));

					confirmationStrStr << toDelete << " URL(s) will be IRRECOVERABLY LOST.\n";
				}
			}

			confirmationStrStr << "Do you really want to change the domain?";
		}

		// handle directory change
		if(dirChanged) {
			if(domainChanged)
				confirmationStrStr << '\n';

			// check for external directory
			if(properties.dir.empty())
				confirmationStrStr << "Do you really want to change the data directory to default?";
			else if(this->database.getWebsiteDataDirectory(id).empty())
				confirmationStrStr << "Do you really want to change the data directory to an external directory?";
			else
				confirmationStrStr << "Do you really want to change the data directory to another external directory?";
		}

		return ServerCommandResponse::toBeConfirmed(confirmationStrStr.str());
	}

	// server command deletewebsite(id): delete a website and all associated data from the database by its ID
	Server::ServerCommandResponse Server::cmdDeleteWebsite() {
		// check whether the deletion of data is allowed
		if(!(this->settings.dataDeletable))
			return ServerCommandResponse::failed("Not allowed.");

		// get argument
		if(!(this->cmdJson.HasMember("id")))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is missing).");

		if(!(this->cmdJson["id"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is not a valid number).");

		const unsigned long id = this->cmdJson["id"].GetUint64();

		// check website
		if(!(this->database.isWebsite(id)))
			return ServerCommandResponse::failed(
					"Website #"
					+ std::to_string(id)
					+ " not found."
			);

		// check whether any thread is using the website
		if(std::find_if(this->crawlers.begin(), this->crawlers.end(), [&id](const auto& p) {
			return p->getWebsite() == id;
		}) != this->crawlers.end())
			return ServerCommandResponse::failed(
					"Website cannot be deleted while crawler is active."
			);

		if(std::find_if(this->parsers.begin(), this->parsers.end(), [&id](const auto& p) {
			return p->getWebsite() == id;
		}) != this->parsers.end())
			return ServerCommandResponse::failed(
					"Website cannot be deleted while parser is active."
			);

		if(std::find_if(this->extractors.begin(), this->extractors.end(), [&id](const auto& p) {
			return p->getWebsite() == id;
		}) != this->extractors.end())
			return ServerCommandResponse::failed(
					"Website cannot be deleted while extractor is active."
			);

		if(std::find_if(this->analyzers.begin(), this->analyzers.end(), [&id](const auto& p) {
			return p->getWebsite() == id;
		}) != this->analyzers.end())
			return ServerCommandResponse::failed(
					"Website cannot be deleted while analyzer is active."
			);

		// deletewebsite needs to be confirmed
		if(this->cmdJson.HasMember("confirmed")) {
			// delete website from database
			this->database.deleteWebsite(id);

			// deletewebsite is a logged command
			this->database.log(
					"website #"
					+ std::to_string(id)
					+ " deleted by "
					+ this->cmdIp
					+ "."
			);

			return ServerCommandResponse("Website deleted.");
		}

		return ServerCommandResponse::toBeConfirmed(
				"Do you really want to delete this website?\n"
				"!!! All associated data will be lost !!!"
		);
	}

	// server command duplicatewebsite(id, queries): Duplicate a website by its ID (no processed data will be duplicated)
	//  NOTE:	'queries' needs to be a JSON object that includes all modules (as key names)
	//			 and their queries (as key values i.e. arrays with objects including "cat" and "name" keys)
	Server::ServerCommandResponse Server::cmdDuplicateWebsite() {
		// get arguments
		if(!(this->cmdJson.HasMember("id")))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is missing).");

		if(!(this->cmdJson["id"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is not a valid number).");

		if(!(this->cmdJson.HasMember("queries")))
			return ServerCommandResponse::failed("Invalid arguments (\'queries\' is missing).");

		if(!(this->cmdJson["queries"].IsObject()))
			return ServerCommandResponse::failed("Invalid arguments (\'queries\' is not a valid JSON object).");

		const unsigned long id = this->cmdJson["id"].GetUint64();

		// get queries from JSON
		Queries queries;

		for(const auto& pair : this->cmdJson["queries"].GetObject()) {
			const std::string moduleName(
					pair.name.GetString(),
					pair.name.GetStringLength()
			);

			if(moduleName.empty())
				continue;

			if(!pair.value.IsArray())
				return ServerCommandResponse::failed(
						"Invalid arguments (\'"
						+ Helper::Json::stringify(pair.value)
						+ "\' in \'queries\' is not a valid JSON array)."
				);

			auto moduleIt = std::find_if(queries.begin(), queries.end(),
					[&moduleName](const auto& p) {
						return p.first == moduleName;
					}
			);

			if(moduleIt == queries.end()) {
				queries.emplace_back(
					moduleName,
					std::vector<StringString>()
				);

				moduleIt = queries.end();

				--moduleIt;
			}

			for(const auto& queryCatName : pair.value.GetArray()) {
				if(!queryCatName.IsObject())
					return ServerCommandResponse::failed(
							"Invalid arguments (\'"
							+ Helper::Json::stringify(queryCatName)
							+ "\' in \'queries[\'"
							+ moduleName
							+ "\']\' is not a valid JSON object)."
					);

				if(!queryCatName.HasMember("cat"))
					return ServerCommandResponse::failed(
							"Invalid arguments (\'"
							+ Helper::Json::stringify(queryCatName)
							+ "\' in \'queries[\'"
							+ moduleName
							+ "\']\' does not contain \'cat\')."
					);

				if(!queryCatName["cat"].IsString())
					return ServerCommandResponse::failed(
							"Invalid arguments (\'"
							+ Helper::Json::stringify(queryCatName["cat"])
							+ "\' in \'queries[\'"
							+ moduleName
							+ "\']\' is not a valid string)."
					);

				if(!queryCatName.HasMember("name"))
					return ServerCommandResponse::failed(
							"Invalid arguments (\'"
							+ Helper::Json::stringify(queryCatName)
							+ "\' in \'queries[\'"
							+ moduleName
							+ "\']\' does not contain \'name\')."
					);

				if(!queryCatName["name"].IsString())
					return ServerCommandResponse::failed(
							"Invalid arguments (\'"
							+ Helper::Json::stringify(queryCatName["name"])
							+ "\' in \'queries[\'"
							+ moduleName
							+ "\']\' is not a valid string)."
					);

				moduleIt->second.emplace_back(
						std::string(
								queryCatName["cat"].GetString(),
								queryCatName["cat"].GetStringLength()
						),
						std::string(
								queryCatName["name"].GetString(),
								queryCatName["name"].GetStringLength()
						)
				);
			}
		}

		// check website
		if(!(this->database.isWebsite(id)))
			return ServerCommandResponse::failed(
					"Website #"
					+ std::to_string(id)
					+ " not found."
			);

		// duplicate website configuration
		const unsigned long newId = this->database.duplicateWebsite(id, queries);

		if(!newId)
			return ServerCommandResponse::failed("Could not add duplicate to database.");

		return ServerCommandResponse("Website duplicated.", newId);
	}

	// server command addurllist(website, namespace, name): add a URL list to the ID-specified website
	Server::ServerCommandResponse Server::cmdAddUrlList() {

		// get arguments
		if(!(this->cmdJson.HasMember("website")))
			return ServerCommandResponse::failed("Invalid arguments (\'website\' is missing).");

		if(!(this->cmdJson["website"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'website\' is not a valid number).");

		if(!(this->cmdJson.HasMember("namespace")))
			return ServerCommandResponse::failed("Invalid arguments (\'namespace\' is missing).");

		if(!(this->cmdJson["namespace"].IsString()))
			return ServerCommandResponse::failed("Invalid arguments (\'namespace\' is not a string).");

		if(!(this->cmdJson.HasMember("name")))
			return ServerCommandResponse::failed("Invalid arguments (\'name\' is missing).");

		if(!(this->cmdJson["name"].IsString()))
			return ServerCommandResponse::failed("Invalid arguments (\'name\' is not a string).");

		const unsigned long website = this->cmdJson["website"].GetUint64();

		const UrlListProperties properties(
				std::string(
						this->cmdJson["namespace"].GetString(),
						this->cmdJson["namespace"].GetStringLength()
				),
				std::string(
						this->cmdJson["name"].GetString(),
						this->cmdJson["name"].GetStringLength()
				)
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
		if(!(this->database.isWebsite(website)))
			return ServerCommandResponse::failed(
					"Website #"
					+ std::to_string(website)
					+ " not found."
			);

		// add URL list to database
		const unsigned long id = this->database.addUrlList(website, properties);

		if(!id)
			return ServerCommandResponse::failed("Could not add URL list to database.");

		return ServerCommandResponse("URL list added.", id);
	}

	// server command updateurllist(id, namespace, name): edit a URL list
	Server::ServerCommandResponse Server::cmdUpdateUrlList() {
		// get arguments
		if(!(this->cmdJson.HasMember("id")))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is missing).");

		if(!(this->cmdJson["id"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is not a valid number).");

		if(!(this->cmdJson.HasMember("namespace")))
			return ServerCommandResponse::failed("Invalid arguments (\'namespace\' is missing).");

		if(!(this->cmdJson["namespace"].IsString()))
			return ServerCommandResponse::failed("Invalid arguments (\'namespace\' is not a string).");

		if(!(this->cmdJson.HasMember("name")))
			return ServerCommandResponse::failed("Invalid arguments (\'name\' is missing).");

		if(!(this->cmdJson["name"].IsString()))
			return ServerCommandResponse::failed("Invalid arguments (\'name\' is not a string).");

		const unsigned long id = this->cmdJson["id"].GetUint64();

		const UrlListProperties properties(
				std::string(
						this->cmdJson["namespace"].GetString(),
						this->cmdJson["namespace"].GetStringLength()
				),
				std::string(
						this->cmdJson["name"].GetString(),
						this->cmdJson["name"].GetStringLength()
				)
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
		if(!(this->database.isUrlList(id)))
			return ServerCommandResponse::failed(
					"URL list #"
					+ std::to_string(id)
					+ " not found."
			);

		// check whether any thread is using the URL list
		if(std::find_if(this->crawlers.begin(), this->crawlers.end(), [&id](const auto& p) {
			return p->getUrlList() == id;
		}) != this->crawlers.end())
			return ServerCommandResponse::failed(
					"URL list cannot be changed while crawler is active."
			);

		if(std::find_if(this->parsers.begin(), this->parsers.end(), [&id](const auto& p) {
			return p->getUrlList() == id;
		}) != this->parsers.end())
			return ServerCommandResponse::failed(
					"URL list cannot be changed while parser is active."
			);

		if(std::find_if(this->extractors.begin(), this->extractors.end(), [&id](const auto& p) {
			return p->getUrlList() == id;
		}) != this->extractors.end())
			return ServerCommandResponse::failed(
					"URL list cannot be changed while extractor is active."
			);

		if(std::find_if(this->analyzers.begin(), this->analyzers.end(), [&id](const auto& p) {
			return p->getUrlList() == id;
		}) != this->analyzers.end())
			return ServerCommandResponse::failed(
					"URL list cannot be changed while analyzer is active."
			);

		// update URL list in database
		this->database.updateUrlList(id, properties);

		return ServerCommandResponse("URL list updated.");
	}

	// server command deleteurllist(id): delete a URL list and all associated data from the database by its ID
	Server::ServerCommandResponse Server::cmdDeleteUrlList() {
		// check whether the deletion of data is allowed
		if(!(this->settings.dataDeletable))
			return ServerCommandResponse::failed("Not allowed.");

		// get argument
		if(!(this->cmdJson.HasMember("id")))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is missing).");

		if(!(this->cmdJson["id"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is not a valid number).");

		const unsigned long id = this->cmdJson["id"].GetUint64();

		// check URL list
		if(!(this->database.isUrlList(id)))
			ServerCommandResponse::failed(
					"URL list #"
					+ std::to_string(id)
					+ " not found."
			);

		// check whether any thread is using the URL list
		if(std::find_if(this->crawlers.begin(), this->crawlers.end(), [&id](const auto& p) {
			return p->getUrlList() == id;
		}) != this->crawlers.end())
			return ServerCommandResponse::failed(
					"URL list cannot be deleted while crawler is active."
			);

		if(std::find_if(this->parsers.begin(), this->parsers.end(), [&id](const auto& p) {
			return p->getUrlList() == id;
		}) != this->parsers.end())
			return ServerCommandResponse::failed(
					"URL list cannot be deleted while parser is active."
			);

		if(std::find_if(this->extractors.begin(), this->extractors.end(), [&id](const auto& p) {
			return p->getUrlList() == id;
		}) != this->extractors.end())
				return ServerCommandResponse::failed(
						"URL list cannot be deleted while extractor is active."
				);

		if(std::find_if(this->analyzers.begin(), this->analyzers.end(), [&id](const auto& p) {
			return p->getUrlList() == id;
		}) != this->analyzers.end())
			return ServerCommandResponse::failed(
					"URL list cannot be deleted while analyzer is active."
			);

		// deleteurllist needs to be confirmed
		if(this->cmdJson.HasMember("confirmed")) {
			// delete URL list from database
			this->database.deleteUrlList(id);

			// deleteurllist is a logged command
			this->database.log(
					"URL list #"
					+ std::to_string(id)
					+ " deleted by "
					+ this->cmdIp
					+ "."
			);

			return ServerCommandResponse("URL list deleted.");
		}

		return ServerCommandResponse::toBeConfirmed(
				"Do you really want to delete this URL list?\n"
				"!!! All associated data will be lost !!!"
		);
	}

	// server command deleteurls(urllist,query): delete all URLs from the specified URL list that match the specified query
	Server::ServerCommandResponse Server::cmdDeleteUrls() {
		// check whether the deletion of data is allowed
		if(!(this->settings.dataDeletable))
			return ServerCommandResponse::failed("Not allowed.");

		// get arguments
		if(!(this->cmdJson.HasMember("urllist")))
			return ServerCommandResponse::failed("Invalid arguments (\'urllist\' is missing).");

		if(!(this->cmdJson["urllist"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'urllist\' is not a valid number).");

		if(!(this->cmdJson.HasMember("query")))
			return ServerCommandResponse::failed("Invalid arguments (\'query\' is missing).");

		if(!(this->cmdJson["urllist"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'query\' is not a valid number).");

		const unsigned long urlList = this->cmdJson["urllist"].GetUint64();
		const unsigned long query = this->cmdJson["query"].GetUint64();

		// check URL list
		if(!(this->database.isUrlList(urlList)))
			return ServerCommandResponse::failed(
					"URL list #"
					+ std::to_string(urlList)
					+ " not found."
			);

		// get website from URL list
		const auto website = this->database.getWebsiteNamespaceFromUrlList(urlList);

		if(!website.first)
			return ServerCommandResponse::failed(
					"Could not get website for URL list #"
					+ std::to_string(urlList)
					+ "."
			);

		// check query
		if(!(this->database.isQuery(query)))
			return ServerCommandResponse::failed(
					"Query #"
					+ std::to_string(query)
					+ " not found."
			);

		if(!(this->database.isQuery(website.first, query)))
			return ServerCommandResponse::failed(
					"Query #"
					+ std::to_string(query)
					+ " is not valid for website #"
					+ std::to_string(website.first)
					+ "."
			);

		// get query properties
		QueryProperties properties{};

		this->database.getQueryProperties(query, properties);

		// check query type (must be RegEx)
		if(properties.type != "regex")
			return ServerCommandResponse::failed(
				"Query #"
				+ std::to_string(query)
				+ " has invalid type (must be RegEx)."
			);

		// check query result type (must be boolean)
		if(!properties.resultBool)
			return ServerCommandResponse::failed(
				"Query #"
				+ std::to_string(query)
				+ " has invalid result type (must be boolean)."
			);

		std::queue<unsigned long> toDelete;

		try {
			// create RegEx query
			const Query::RegEx query(
					properties.text,
					true,
					false
			);

			// get URLs from URL list
			auto urls = this->database.getUrlsWithIds(urlList);

			// perform query on each URL in the URL list to identify which URLs to delete
			while(!urls.empty()) {
				if(query.getBool(urls.front().second))
					toDelete.push(urls.front().first);

				urls.pop();
			}
		}
		catch(const RegExException& e) {
			return ServerCommandResponse::failed(
					"RegEx error: "
					+ e.whatStr()
			);
		}

		// check for URLs matching the query
		if(toDelete.empty())
			return ServerCommandResponse("The query did not match any URLs in the URL list.");

		// deleteurls needs to be confirmed
		if(this->cmdJson.HasMember("confirmed")) {
			const auto numDeleted = this->database.deleteUrls(urlList, toDelete);

			if(numDeleted == 1)
				return ServerCommandResponse("One URL has been deleted.");
			else {
				std::ostringstream responseStrStr;

				responseStrStr.imbue(std::locale(""));

				responseStrStr << numDeleted << " URLs have been deleted.";

				return ServerCommandResponse(responseStrStr.str());
			}
		}

		if(toDelete.size() == 1)
			return ServerCommandResponse::toBeConfirmed(
					"Do you really want to delete one URL?"
					"\n!!! All associated data will be lost !!!"
			);
		else {
			std::ostringstream responseStrStr;

			responseStrStr.imbue(std::locale(""));

			responseStrStr	<< "Do you really want to delete "
							<< toDelete.size()
							<< " URLs?\n!!! All associated data will be lost !!!";

			return ServerCommandResponse::toBeConfirmed(responseStrStr.str());
		}
	}

	// server command addquery(website, name, query, type, resultbool, resultsingle, resultmulti, resultsubsets, textonly):
	//  add a query
	Server::ServerCommandResponse Server::cmdAddQuery() {
		// get arguments
		if(!(this->cmdJson.HasMember("website")))
			return ServerCommandResponse::failed("Invalid arguments (\'website\' is missing).");

		if(!(this->cmdJson["website"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'website\' is not a valid number).");

		if(!(this->cmdJson.HasMember("name")))
			return ServerCommandResponse::failed("Invalid arguments (\'name\' is missing).");

		if(!(this->cmdJson["name"].IsString()))
			return ServerCommandResponse::failed("Invalid arguments (\'name\' is not a string).");

		if(!(this->cmdJson.HasMember("query")))
			return ServerCommandResponse::failed("Invalid arguments (\'query\' is missing).");

		if(!(this->cmdJson["query"].IsString()))
			return ServerCommandResponse::failed("Invalid arguments (\'query\' is not a string).");

		if(!(this->cmdJson.HasMember("type")))
			return ServerCommandResponse::failed("Invalid arguments (\'type\' is missing).");

		if(!(this->cmdJson["type"].IsString()))
			return ServerCommandResponse::failed("Invalid arguments (\'type\' is not a string).");

		if(!(this->cmdJson.HasMember("resultbool")))
			return ServerCommandResponse::failed("Invalid arguments (\'resultbool\' is missing).");

		if(!(this->cmdJson["resultbool"].IsBool()))
			return ServerCommandResponse::failed("Invalid arguments (\'resultbool\' is not a boolean).");

		if(!(this->cmdJson.HasMember("resultsingle")))
			return ServerCommandResponse::failed("Invalid arguments (\'resultsingle\' is missing).");

		if(!(this->cmdJson["resultsingle"].IsBool()))
			return ServerCommandResponse::failed("Invalid arguments (\'resultsingle\' is not a boolean).");

		if(!(this->cmdJson.HasMember("resultmulti")))
			return ServerCommandResponse::failed("Invalid arguments (\'resultmulti\' is missing).");

		if(!(this->cmdJson["resultmulti"].IsBool()))
			return ServerCommandResponse::failed("Invalid arguments (\'resultmulti\' is not a boolean).");

		if(!(this->cmdJson.HasMember("resultsubsets")))
			return ServerCommandResponse::failed("Invalid arguments (\'resultsubsets\' is missing).");

		if(!(this->cmdJson["resultsubsets"].IsBool()))
			return ServerCommandResponse::failed("Invalid arguments (\'resultsubsets\' is not a boolean).");

		if(!(this->cmdJson.HasMember("textonly")))
			return ServerCommandResponse::failed("Invalid arguments (\'textonly\' is missing).");

		if(!(this->cmdJson["textonly"].IsBool()))
			return ServerCommandResponse::failed("Invalid arguments (\'textonly\' is not a boolean).");

		const unsigned long website = this->cmdJson["website"].GetUint64();

		const QueryProperties properties(
				std::string(
						this->cmdJson["name"].GetString(),
						this->cmdJson["name"].GetStringLength()
				),
				std::string(
						this->cmdJson["query"].GetString(),
						this->cmdJson["query"].GetStringLength()
				),
				std::string(
						this->cmdJson["type"].GetString(),
						this->cmdJson["type"].GetStringLength()
				),
				this->cmdJson["resultbool"].GetBool(),
				this->cmdJson["resultsingle"].GetBool(),
				this->cmdJson["resultmulti"].GetBool(),
				this->cmdJson["resultsubsets"].GetBool(),
				this->cmdJson["textonly"].GetBool()
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
				&& properties.type != "xpathjsonpointer"
				&& properties.type != "xpathjsonpath"
		)
			return ServerCommandResponse::failed("Unknown query type: \'" + properties.type + "\'.");

		// check result type
		if(
				!properties.resultBool
				&& !properties.resultSingle
				&& !properties.resultMulti
				&& !properties.resultSubSets
		)
			return ServerCommandResponse::failed("No result type selected.");

		// check website
		if(website && !(this->database.isWebsite(website)))
			return ServerCommandResponse::failed(
					"Website #"
					+ std::to_string(website)
					+ " not found."
			);

		// add query to database
		const unsigned long id = this->database.addQuery(website, properties);

		if(id)
			return ServerCommandResponse("Query added.", id);

		return ServerCommandResponse("Could not add query to database.");
	}

	// server command updatequery(id, name, query, type, resultbool, resultsingle, resultmulti, resultsubsets, textonly):
	//  edit a query
	Server::ServerCommandResponse Server::cmdUpdateQuery() {
		// get arguments
		if(!(this->cmdJson.HasMember("id")))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is missing).");

		if(!(this->cmdJson["id"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is not a valid number).");

		if(!(this->cmdJson.HasMember("name")))
			return ServerCommandResponse::failed("Invalid arguments (\'name\' is missing).");

		if(!(this->cmdJson["name"].IsString()))
			return ServerCommandResponse::failed("Invalid arguments (\'name\' is not a string).");

		if(!(this->cmdJson.HasMember("query")))
			return ServerCommandResponse::failed("Invalid arguments (\'query\' is missing).");

		if(!(this->cmdJson["query"].IsString()))
			return ServerCommandResponse::failed("Invalid arguments (\'query\' is not a string).");

		if(!(this->cmdJson.HasMember("type")))
			return ServerCommandResponse::failed("Invalid arguments (\'type\' is missing).");

		if(!(this->cmdJson["type"].IsString()))
			return ServerCommandResponse::failed("Invalid arguments (\'type\' is not a string).");

		if(!(this->cmdJson.HasMember("resultbool")))
			return ServerCommandResponse::failed("Invalid arguments (\'resultbool\' is missing).");

		if(!(this->cmdJson["resultbool"].IsBool()))
			return ServerCommandResponse::failed("Invalid arguments (\'resultbool\' is not a boolean).");

		if(!(this->cmdJson.HasMember("resultsingle")))
			return ServerCommandResponse::failed("Invalid arguments (\'resultsingle\' is missing).");

		if(!(this->cmdJson["resultsingle"].IsBool()))
			return ServerCommandResponse::failed("Invalid arguments (\'resultsingle\' is not a boolean).");

		if(!(this->cmdJson.HasMember("resultmulti")))
			return ServerCommandResponse::failed("Invalid arguments (\'resultmulti\' is missing).");

		if(!(this->cmdJson["resultmulti"].IsBool()))
			return ServerCommandResponse::failed("Invalid arguments (\'resultmulti\' is not a boolean).");

		if(!(this->cmdJson.HasMember("resultsubsets")))
			return ServerCommandResponse::failed("Invalid arguments (\'resultsubsets\' is missing).");

		if(!(this->cmdJson["resultsubsets"].IsBool()))
			return ServerCommandResponse::failed("Invalid arguments (\'resultsubsets\' is not a boolean).");

		if(!(this->cmdJson.HasMember("textonly")))
			return ServerCommandResponse::failed("Invalid arguments (\'textonly\' is missing).");

		if(!(this->cmdJson["textonly"].IsBool()))
			return ServerCommandResponse::failed("Invalid arguments (\'textonly\' is not a boolean).");

		const unsigned long id = this->cmdJson["id"].GetUint64();

		const QueryProperties properties(
				std::string(
						this->cmdJson["name"].GetString(),
						this->cmdJson["name"].GetStringLength()
				),
				std::string(
						this->cmdJson["query"].GetString(),
						this->cmdJson["query"].GetStringLength()
				),
				std::string(
						this->cmdJson["type"].GetString(),
						this->cmdJson["type"].GetStringLength()
				),
				this->cmdJson["resultbool"].GetBool(),
				this->cmdJson["resultsingle"].GetBool(),
				this->cmdJson["resultmulti"].GetBool(),
				this->cmdJson["resultsubsets"].GetBool(),
				this->cmdJson["textonly"].GetBool()
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
				&& properties.type != "xpathjsonpointer"
				&& properties.type != "xpathjsonpath"
		)
			return ServerCommandResponse::failed("Unknown query type: \'" + properties.type + "\'.");

		// check result type
		if(
				!properties.resultBool
				&& !properties.resultSingle
				&& !properties.resultMulti
				&& !properties.resultSubSets
		)
			return ServerCommandResponse::failed("No result type selected.");

		// check query
		if(!(this->database.isQuery(id)))
			return ServerCommandResponse::failed(
					"Query #"
					+ std::to_string(id)
					+ " not found."
			);

		// update query in database
		this->database.updateQuery(id, properties);

		return ServerCommandResponse("Query updated.");
	}

	// server command deletequery(id): delete a query from the database by its ID
	Server::ServerCommandResponse Server::cmdDeleteQuery() {
		// check whether the deletion of data is allowed
		if(!(this->settings.dataDeletable))
			return ServerCommandResponse::failed("Not allowed.");

		// get argument
		if(!(this->cmdJson.HasMember("id")))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is missing).");

		if(!(this->cmdJson["id"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is not a valid number).");

		const unsigned long id = this->cmdJson["id"].GetUint64();

		// check query
		if(!(this->database.isQuery(id)))
			return ServerCommandResponse::failed(
					"Query #"
					+ std::to_string(id)
					+ " not found."
			);

		if(this->cmdJson.HasMember("confirmed")) {
			// delete URL list from database
			this->database.deleteQuery(id);

			return ServerCommandResponse("Query deleted.");
		}

		return ServerCommandResponse::toBeConfirmed("Do you really want to delete this query?");
	}

	// server command duplicatequery(id): Duplicate a query by its ID
	Server::ServerCommandResponse Server::cmdDuplicateQuery() {
		// get argument
		if(!(this->cmdJson.HasMember("id")))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is missing).");

		if(!(this->cmdJson["id"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is not a valid number).");

		const unsigned long id = this->cmdJson["id"].GetUint64();

		// check query
		if(!(this->database.isQuery(id)))
			return ServerCommandResponse::failed(
					"Query #"
					+ std::to_string(id)
					+ " not found."
			);

		// duplicate query
		const unsigned long newId = this->database.duplicateQuery(id);

		if(!newId)
			return ServerCommandResponse::failed("Could not add duplicate to database.");

		return ServerCommandResponse("Query duplicated.", newId);
	}

	// server command addconfig(website, module, name, config): Add configuration to database
	Server::ServerCommandResponse Server::cmdAddConfig() {
		// get arguments
		if(!(this->cmdJson.HasMember("website")))
			return ServerCommandResponse::failed("Invalid arguments (\'website\' is missing).");

		if(!(this->cmdJson["website"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'website\' is not a valid number).");

		if(!(this->cmdJson.HasMember("module")))
			return ServerCommandResponse::failed("Invalid arguments (\'module\' is missing).");

		if(!(this->cmdJson["module"].IsString()))
			return ServerCommandResponse::failed("Invalid arguments (\'module\' is not a string).");

		if(!(this->cmdJson.HasMember("name")))
			return ServerCommandResponse::failed("Invalid arguments (\'name\' is missing).");

		if(!(this->cmdJson["name"].IsString()))
			return ServerCommandResponse::failed("Invalid arguments (\'name\' is not a string).");

		if(!(this->cmdJson.HasMember("config")))
			return ServerCommandResponse::failed("Invalid arguments (\'config\' is missing).");

		if(!(this->cmdJson["config"].IsString()))
			return ServerCommandResponse::failed("Invalid arguments (\'config\' is not a string).");

		const unsigned long website = this->cmdJson["website"].GetUint64();

		const ConfigProperties properties(
				std::string(
						this->cmdJson["module"].GetString(),
						this->cmdJson["module"].GetStringLength()
				),
				std::string(
						this->cmdJson["name"].GetString(),
						this->cmdJson["name"].GetStringLength()
				),
				std::string(
						this->cmdJson["config"].GetString(),
						this->cmdJson["config"].GetStringLength()
				)
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
		if(!(this->database.isWebsite(website)))
			return ServerCommandResponse::failed(
					"Website #"
					+ std::to_string(website)
					+ " not found."
			);

		// add configuration to database
		const unsigned long id = this->database.addConfiguration(website, properties);

		if(!id)
			return ServerCommandResponse::failed("Could not add configuration to database.");

		return ServerCommandResponse("Configuration added.", id);
	}

	// server command updateconfig(id, name, config): Update a configuration in the database by its ID
	Server::ServerCommandResponse Server::cmdUpdateConfig() {
		// get arguments
		if(!(this->cmdJson.HasMember("id")))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is missing).");

		if(!(this->cmdJson["id"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is not a valid number).");

		if(!(this->cmdJson.HasMember("name")))
			return ServerCommandResponse::failed("Invalid arguments (\'name\' is missing).");

		if(!(this->cmdJson["name"].IsString()))
			return ServerCommandResponse::failed("Invalid arguments (\'name\' is not a string).");

		if(!(this->cmdJson.HasMember("config")))
			return ServerCommandResponse::failed("Invalid arguments (\'config\' is missing).");

		if(!(this->cmdJson["config"].IsString()))
			return ServerCommandResponse::failed("Invalid arguments (\'config\' is not a string).");

		const unsigned long id = this->cmdJson["id"].GetUint64();

		const ConfigProperties properties(
				std::string(
						this->cmdJson["name"].GetString(),
						this->cmdJson["name"].GetStringLength()
				),
				std::string(
						this->cmdJson["config"].GetString(),
						this->cmdJson["config"].GetStringLength()
				)
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
		if(!(this->database.isConfiguration(id)))
			return ServerCommandResponse::failed(
					"Configuration #"
					+ std::to_string(id)
					+ " not found."
			);

		// update configuration in database
		this->database.updateConfiguration(id, properties);

		return ServerCommandResponse("Configuration updated.");
	}

	// server command deleteconfig(id): delete a configuration from the database by its ID
	Server::ServerCommandResponse Server::cmdDeleteConfig() {
		// check whether the deletion of data is allowed
		if(!(this->settings.dataDeletable))
			return ServerCommandResponse::failed("Not allowed.");

		// get argument
		if(!(this->cmdJson.HasMember("id")))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is missing).");

		if(!(this->cmdJson["id"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is not a valid number).");

		const unsigned long id = this->cmdJson["id"].GetUint64();

		// check configuration
		if(!(this->database.isConfiguration(id)))
			ServerCommandResponse::failed(
					"Configuration #"
					+ std::to_string(id)
					+ " not found."
			);


		// deleteconfig needs to be confirmed
		if(this->cmdJson.HasMember("confirmed")) {
			// delete configuration from database
			this->database.deleteConfiguration(id);

			return ServerCommandResponse("Configuration deleted.");
		}

		return ServerCommandResponse::toBeConfirmed("Do you really want to delete this configuration?");
	}

	// server command duplicateconfig(id): Duplicate a configuration by its ID
	Server::ServerCommandResponse Server::cmdDuplicateConfig() {
		// get argument
		if(!(this->cmdJson.HasMember("id")))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is missing).");

		if(!(this->cmdJson["id"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'id\' is not a valid number).");

		const unsigned long id = this->cmdJson["id"].GetUint64();

		// check configuration
		if(!(this->database.isConfiguration(id)))
			return ServerCommandResponse::failed(
					"Configuration #"
					+ std::to_string(id)
					+ " not found."
			);

		// duplicate configuration
		const unsigned long newId = this->database.duplicateConfiguration(id);

		if(!newId)
			return ServerCommandResponse::failed("Could not add duplicate to database.");

		return ServerCommandResponse("Configuration duplicated.", newId);
	}

	// server command warp(thread, target): Let thread jump to specified ID
	Server::ServerCommandResponse Server::cmdWarp() {
		// get arguments
		if(!(this->cmdJson.HasMember("thread")))
			return ServerCommandResponse::failed("Invalid arguments (\'thread\' is missing).");

		if(!(this->cmdJson["thread"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'thread\' is not a valid number).");

		if(!(this->cmdJson.HasMember("target")))
			return ServerCommandResponse::failed("Invalid arguments (\'target\' is missing).");

		if(!(this->cmdJson["target"].IsUint64()))
			return ServerCommandResponse::failed("Invalid arguments (\'target\' is not a valid number).");

		const unsigned long thread = this->cmdJson["thread"].GetUint64();
		const unsigned long target = this->cmdJson["target"].GetUint64();

		// find thread
		auto c = std::find_if(this->crawlers.begin(), this->crawlers.end(),
				[&thread](const auto& p) {
					return p->Module::Thread::getId() == thread;
				}
		);

		if(c != this->crawlers.end()) {
			(*c)->Module::Thread::warpTo(target);

			return ServerCommandResponse(
					"Crawler #"
					+ std::to_string(thread)
					+ " will warp to #"
					+ std::to_string(target)
					+ "."
			);
		}

		auto p = std::find_if(this->parsers.begin(), this->parsers.end(),
				[&thread](const auto& p) {
					return p->Module::Thread::getId() == thread;
				}
		);

		if(p != this->parsers.end()) {
			(*p)->Module::Thread::warpTo(target);

			return ServerCommandResponse(
					"Parser #"
					+ std::to_string(thread)
					+ " will warp to #"
					+ std::to_string(target)
					+ "."
			);
		}

		auto e = std::find_if(this->extractors.begin(), this->extractors.end(),
				[&thread](const auto& p) {
					return p->Module::Thread::getId() == thread;
				}
		);

		if(e != this->extractors.end()) {
			(*e)->Module::Thread::warpTo(target);

			return ServerCommandResponse(
					"Extractor #"
					+ std::to_string(thread)
					+ " will warp to #"
					+ std::to_string(target)
					+ "."
			);
		}

		auto a = std::find_if(this->analyzers.begin(), this->analyzers.end(),
				[&thread](const auto& p) {
					return p->Module::Thread::getId() == thread;
				}
		);

		if(a != this->analyzers.end())
			return ServerCommandResponse::failed(
					"Time travel is not supported for analyzers."
			);
		else
			return ServerCommandResponse::failed(
					"Could not find thread #"
					+ std::to_string(thread)
					+ "."
			);
	}

	// server command download(filename): Download the specified file from the file cache of the web server
	//  NOTE: Returns the name of the file to download
	Server::ServerCommandResponse Server::cmdDownload() {
		// get argument
		if(!(this->cmdJson.HasMember("filename")))
			return ServerCommandResponse::failed("Invalid arguments (\'filename\' is missing).");

		if(!(this->cmdJson["filename"].IsString()))
			return ServerCommandResponse::failed("Invalid arguments (\'filename\' is not a string).");

		return ServerCommandResponse(
				std::string(
						this->cmdJson["filename"].GetString(),
						this->cmdJson["filename"].GetStringLength()
				)
		);
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

											db.log(0, logStrStr.str());

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

										db.log(0, logStrStr.str());
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
				const std::string datatype(
						json["datatype"].GetString(),
						json["datatype"].GetStringLength()
				);

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

								db.log(0, logStrStr.str());

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
								db.log(0, logStrStr.str());
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
				const std::string datatype(
						json["datatype"].GetString(),
						json["datatype"].GetStringLength()
				);

				const std::string filetype(
						json["filetype"].GetString(),
						json["filetype"].GetStringLength()
				);

				const std::string compression(
						json["compression"].GetString(),
						json["compression"].GetStringLength()
				);

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

							db.log(0, logStrStr.str());
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
						const std::string fileName(
								Helper::Strings::generateRandom(
										this->webServer.fileLength
								)
						);

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

						db.log(0, logStrStr.str());
					}
				}
			}
		}

		// end of worker thread
		MAIN_SERVER_WORKER_END(response)

		this->workerEnd(threadIndex, connection, message, response);
	}

	// server command testquery(query, type, resultbool, resultsingle, resultmulti, textonly, text,
	//	xmlwarnings, datetimeformat, datetimelocale): test temporary query on text
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

			else if(!json.HasMember("resultsubsets"))
				response = ServerCommandResponse::failed("Invalid arguments (\'resultsubsets\' is missing).");

			else if(!json["resultsubsets"].IsBool())
				response = ServerCommandResponse::failed("Invalid arguments (\'resultsubsets\' is not a boolean).");

			else if(!json.HasMember("textonly"))
				response = ServerCommandResponse::failed("Invalid arguments (\'textonly\' is missing).");

			else if(!json["textonly"].IsBool())
				response = ServerCommandResponse::failed("Invalid arguments (\'textonly\' is not a boolean).");

			else if(!json.HasMember("text"))
				response = ServerCommandResponse::failed("Invalid arguments (\'text\' is missing).");

			else if(!json["text"].IsString())
				response = ServerCommandResponse::failed("Invalid arguments (\'text\' is not a string).");

			else if(!json.HasMember("xmlwarnings"))
				response = ServerCommandResponse::failed("Invalid arguments (\'xmlwarnings\' is missing).");

			else if(!json["xmlwarnings"].IsBool())
				response = ServerCommandResponse::failed("Invalid arguments (\'xmlwarnings\' is not a boolean).");

			else if(!json.HasMember("datetimeformat"))
				response = ServerCommandResponse::failed("Invalid arguments (\'datetimeformat\' is missing).");

			else if(!json["datetimeformat"].IsString())
				response = ServerCommandResponse::failed("Invalid arguments (\'datetimeformat\' is not a string).");

			else if(!json.HasMember("datetimelocale"))
				response = ServerCommandResponse::failed("Invalid arguments (\'datetimelocale\' is missing).");

			else if(!json["datetimelocale"].IsString())
				response = ServerCommandResponse::failed("Invalid arguments (\'datetimelocale\' is not a string).");

			else {
				const QueryProperties properties(
						std::string(json["query"].GetString(), json["query"].GetStringLength()),
						std::string(json["type"].GetString(), json["type"].GetStringLength()),
						json["resultbool"].GetBool(),
						json["resultsingle"].GetBool(),
						json["resultmulti"].GetBool(),
						json["resultsubsets"].GetBool(),
						json["textonly"].GetBool()
				);

				const std::string text(
						json["text"].GetString(),
						json["text"].GetStringLength()
				);

				const std::string dateTimeFormat(
						json["datetimeformat"].GetString(),
						json["datetimeformat"].GetStringLength()
				);

				const std::string dateTimeLocale(
						json["datetimelocale"].GetString(),
						json["datetimelocale"].GetStringLength()
				);

				const bool xmlWarnings = json["xmlwarnings"].GetBool();

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
						&& properties.type != "xpathjsonpointer"
						&& properties.type != "xpathjsonpath"
				)
					response = ServerCommandResponse::failed("Unknown query type: \'" + properties.type + "\'.");

				else if(
						!properties.resultBool
						&& !properties.resultSingle
						&& !properties.resultMulti
						&& !properties.resultSubSets
				)
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

							if(properties.resultBool)
								// get boolean result (does at least one match exist?)
								result	+= "BOOLEAN RESULT ("
										+ timer.tickStr()
										+ "): "
										+ (regExTest.getBool(text) ? "true" : "false")
										+ '\n';

							if(properties.resultSingle) {
								// get first result (first full match)
								std::string tempResult;

								regExTest.getFirst(text, tempResult);

								result += "FIRST RESULT (" + timer.tickStr() + "): ";

								if(tempResult.empty())
									result += "[empty]";
								else
									result += tempResult + dateTimeTest(tempResult, dateTimeFormat, dateTimeLocale);

								result.push_back('\n');
							}

							if(properties.resultMulti || properties.resultSubSets) {
								// get all results (all full matches)
								std::vector<std::string> tempResults;

								regExTest.getAll(text, tempResults);

								result += "ALL RESULTS (" + timer.tickStr() + "):";

								if(tempResults.empty())
									result += " [empty]\n";
								else {
									unsigned long counter = 0;
									std::string toAppend(1, '\n');

									for(const auto& tempResult : tempResults) {
										++counter;

										toAppend	+= '['
													+ std::to_string(counter)
													+ "] "
													+ tempResult
													+ dateTimeTest(tempResult, dateTimeFormat, dateTimeLocale)
													+ "\n";
									}

									result += toAppend;
								}
							}
						}
						catch(const RegExException& e) {
							response = ServerCommandResponse::failed("RegEx error: " + e.whatStr());
						}
					}
					else if(properties.type == "xpath") {
						// test XPath expression on text
						try {
							Timer::SimpleHR timer;
							std::queue<std::string> warnings;
							const Query::XPath xPathTest(
									properties.text,
									properties.textOnly
							);

							result = "COMPILING TIME: " + timer.tickStr() + '\n';

							Parsing::XML xmlDocumentTest;

							xmlDocumentTest.setOptions(xmlWarnings, 25);
							xmlDocumentTest.parse(text, true, true, warnings);

							while(!warnings.empty()) {
								result += "WARNING: " + warnings.front() + '\n';

								warnings.pop();
							}

							result += "PARSING TIME: " + timer.tickStr() + '\n';

							if(properties.resultBool)
								// get boolean result (does at least one match exist?)
								result	+= "BOOLEAN RESULT ("
										+ timer.tickStr()
										+ "): "
										+ (xPathTest.getBool(xmlDocumentTest) ? "true" : "false")
										+ '\n';

							if(properties.resultSingle) {
								// get first result (first full match)
								std::string tempResult;

								xPathTest.getFirst(xmlDocumentTest, tempResult);

								result += "FIRST RESULT (" + timer.tickStr() + "): ";

								if(tempResult.empty())
									result += "[empty]";
								else
									result += tempResult + dateTimeTest(tempResult, dateTimeFormat, dateTimeLocale);

								result.push_back('\n');
							}

							if(properties.resultMulti) {
								// get all results (all full matches)
								std::vector<std::string> tempResults;

								xPathTest.getAll(xmlDocumentTest, tempResults);

								result += "ALL RESULTS (" + timer.tickStr() + "):";

								if(tempResults.empty())
									result += " [empty]\n";
								else {
									unsigned long counter = 0;
									std::string toAppend(1, '\n');

									for(const auto& tempResult : tempResults) {
										++counter;

										toAppend	+= '['
													+ std::to_string(counter)
													+ "] "
													+ tempResult
													+ dateTimeTest(tempResult, dateTimeFormat, dateTimeLocale)
													+ '\n';
									}

									result += toAppend;
								}
							}

							if(properties.resultSubSets) {
								// get subsets
								std::vector<Parsing::XML> tempResults;

								xPathTest.getSubSets(xmlDocumentTest, tempResults);

								result += "SUBSETS (" + timer.tickStr() + "):";

								if(tempResults.empty())
									result += " [empty]\n";
								else {
									unsigned long counter = 0;
									std::string toAppend(1, '\n');

									for(const auto& tempResult : tempResults) {
										++counter;

										std::string subsetString;

										tempResult.getContent(subsetString);

										toAppend	+= '['
													+ std::to_string(counter)
													+ "] "
													+ subsetString
													+ '\n';
									}

									result += toAppend;
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
									properties.text,
									properties.textOnly
							);

							result = "COMPILING TIME: " + timer.tickStr() + '\n';

							rapidjson::Document jsonDocumentTest;

							jsonDocumentTest = Helper::Json::parseRapid(text);

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

								result += "FIRST RESULT (" + timer.tickStr() + "): ";

								if(tempResult.empty())
									result += "[empty]";
								else
									result += tempResult + dateTimeTest(tempResult, dateTimeFormat, dateTimeLocale);

								result.push_back('\n');
							}

							if(properties.resultMulti) {
								// get all results (all full matches)
								std::vector<std::string> tempResults;

								JSONPointerTest.getAll(jsonDocumentTest, tempResults);

								result += "ALL RESULTS (" + timer.tickStr() + "):";

								if(tempResults.empty())
									result += " [empty]\n";
								else {
									unsigned long counter = 0;
									std::string toAppend(1, '\n');

									for(const auto& tempResult : tempResults) {
										++counter;

										toAppend	+= '['
													+ std::to_string(counter)
													+ "] "
													+ tempResult
													+ dateTimeTest(tempResult, dateTimeFormat, dateTimeLocale)
													+ '\n';
									}

									result += toAppend;
								}
							}

							if(properties.resultSubSets) {
								// get subsets
								std::vector<rapidjson::Document> tempResults;

								JSONPointerTest.getSubSets(jsonDocumentTest, tempResults);

								result += "SUBSETS (" + timer.tickStr() + "):";

								if(tempResults.empty())
									result += " [empty]\n";
								else {
									unsigned long counter = 0;
									std::string toAppend(1, '\n');

									for(const auto& tempResult : tempResults) {
										++counter;

										toAppend	+= '['
													+ std::to_string(counter)
													+ "] "
													+ Helper::Json::stringify(tempResult)
													+ '\n';
									}

									result += toAppend;
								}
							}
						}
						catch(const JSONPointerException& e) {
							response = ServerCommandResponse::failed("JSONPointer error: " + e.whatStr());
						}
						catch(const JsonException& e) {
							response = ServerCommandResponse::failed("Could not parse JSON: " + e.whatStr() + ".");
						}
					}
					else if(properties.type == "jsonpath") {
						// test JSONPath query on text
						try {
							Timer::SimpleHR timer;

							const Query::JsonPath JSONPathTest(
									properties.text,
									properties.textOnly
							);

							jsoncons::json jsonTest;

							jsonTest = Helper::Json::parseCons(text);

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

								result += "FIRST RESULT (" + timer.tickStr() + "): ";

								if(tempResult.empty())
									result += "[empty]";
								else
									result += tempResult + dateTimeTest(tempResult, dateTimeFormat, dateTimeLocale);

								result.push_back('\n');
							}

							if(properties.resultMulti) {
								// get all results (all full matches)
								std::vector<std::string> tempResults;

								JSONPathTest.getAll(jsonTest, tempResults);

								result += "ALL RESULTS (" + timer.tickStr() + "):";

								if(tempResults.empty())
									result += " [empty]\n";
								else {
									unsigned long counter = 0;
									std::string toAppend(1, '\n');

									for(const auto& tempResult : tempResults) {
										++counter;

										toAppend	+= '['
													+ std::to_string(counter)
													+ "] "
													+ tempResult
													+ dateTimeTest(tempResult, dateTimeFormat, dateTimeLocale)
													+ '\n';
									}

									result += toAppend;
								}
							}

							if(properties.resultSubSets) {
								// get subsets
								std::vector<jsoncons::json> tempResults;

								JSONPathTest.getSubSets(jsonTest, tempResults);

								result += "SUBSETS (" + timer.tickStr() + "):";

								if(tempResults.empty())
									result += " [empty]\n";
								else {
									unsigned long counter = 0;
									std::string toAppend(1, '\n');

									for(const auto& tempResult : tempResults) {
										++counter;

										toAppend	+= '['
													+ std::to_string(counter)
													+ "] "
													+ Helper::Json::stringify(tempResult)
													+ '\n';
									}

									result += toAppend;
								}
							}
						}
						catch(const JSONPathException& e) {
							response = ServerCommandResponse::failed("JSONPath error: " + e.whatStr());
						}
						catch(const JsonException& e) {
							response = ServerCommandResponse::failed("Could not parse JSON: " + e.whatStr() + ".");
						}
					}
					else { // test combined query (XPath + JSONPointer/JSONPath) on text
						// show performance warning
						result =	"NOTE: When using combined queries,"
									" the JSON needs to be parsed every time the query is used."
									"\n\n";

						// split XPath query (first line) from JSON query
						const auto splitPos = properties.text.find('\n');
						const std::string xPathQuery(
								properties.text,
								0,
								splitPos
						);
						std::string jsonQuery;

						if(splitPos != std::string::npos && properties.text.size() > splitPos + 1)
							jsonQuery = properties.text.substr(splitPos + 1);

						result += "using XPath query \'"
								+ xPathQuery
								+ "\'\nusing JSON query \'"
								+ jsonQuery
								+ "\'\n\n";

						// perform XPath expression with single result on text first
						try {
							Timer::SimpleHR timer;
							std::queue<std::string> warnings;

							const Query::XPath xPathTest(
									xPathQuery,
									true
							);

							result += "XPATH COMPILING TIME: " + timer.tickStr() + '\n';

							Parsing::XML xmlDocumentTest;

							xmlDocumentTest.setOptions(xmlWarnings, 25);
							xmlDocumentTest.parse(text, true, true, warnings);

							while(!warnings.empty()) {
								result += "WARNING: " + warnings.front() + '\n';

								warnings.pop();
							}

							result += "HTML/XML PARSING TIME: " + timer.tickStr() + '\n';

							// get first result from XPath (first full match)
							std::string xpathResult;

							xPathTest.getFirst(xmlDocumentTest, xpathResult);

							result += "XPATH RESULT (" + timer.tickStr() + "): ";

							if(xpathResult.empty())
								result += "[empty]\n";
							else {
								result += xpathResult + '\n';

								if(properties.type == "xpathjsonpointer") {
									// test JSONPointer query on XPath result
									Timer::SimpleHR timer;

									const Query::JsonPointer JSONPointerTest(
											jsonQuery,
											properties.textOnly
									);

									result += "JSONPOINTER COMPILING TIME: " + timer.tickStr() + '\n';

									rapidjson::Document jsonDocumentTest;

									jsonDocumentTest = Helper::Json::parseRapid(xpathResult);

									result += "JSON PARSING TIME: " + timer.tickStr() + '\n';

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

										result += "FIRST RESULT (" + timer.tickStr() + "): ";

										if(tempResult.empty())
											result += "[empty]";
										else
											result += tempResult + dateTimeTest(tempResult, dateTimeFormat, dateTimeLocale);

										result.push_back('\n');
									}

									if(properties.resultMulti) {
										// get all results (all full matches)
										std::vector<std::string> tempResults;

										JSONPointerTest.getAll(jsonDocumentTest, tempResults);

										result += "ALL RESULTS (" + timer.tickStr() + "):";

										if(tempResults.empty())
											result += " [empty]\n";
										else {
											unsigned long counter = 0;
											std::string toAppend(1, '\n');

											for(const auto& tempResult : tempResults) {
												++counter;

												toAppend	+= '['
															+ std::to_string(counter)
															+ "] "
															+ tempResult
															+ dateTimeTest(tempResult, dateTimeFormat, dateTimeLocale)
															+ '\n';
											}

											result += toAppend;
										}
									}

									if(properties.resultSubSets) {
										// get subsets
										std::vector<rapidjson::Document> tempResults;

										JSONPointerTest.getSubSets(jsonDocumentTest, tempResults);

										result += "SUBSETS (" + timer.tickStr() + "):";

										if(tempResults.empty())
											result += " [empty]\n";
										else {
											unsigned long counter = 0;
											std::string toAppend(1, '\n');

											for(const auto& tempResult : tempResults) {
												++counter;

												toAppend	+= '['
															+ std::to_string(counter)
															+ "] "
															+ Helper::Json::stringify(tempResult)
															+ '\n';
											}

											result += toAppend;
										}
									}
								}
								else {
									// test JSONPath query on XPath result
									Timer::SimpleHR timer;

									const Query::JsonPath JSONPathTest(
											jsonQuery,
											properties.textOnly
									);

									jsoncons::json jsonTest;

									jsonTest = Helper::Json::parseCons(xpathResult);

									result += "JSON PARSING TIME: " + timer.tickStr() + '\n';

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

										result += "FIRST RESULT (" + timer.tickStr() + "): ";

										if(tempResult.empty())
											result += "[empty]";
										else
											result += tempResult + dateTimeTest(tempResult, dateTimeFormat, dateTimeLocale);

										result.push_back('\n');
									}

									if(properties.resultMulti) {
										// get all results (all full matches)
										std::vector<std::string> tempResults;

										JSONPathTest.getAll(jsonTest, tempResults);

										result += "ALL RESULTS (" + timer.tickStr() + "):";

										if(tempResults.empty())
											result += " [empty]\n";
										else {
											unsigned long counter = 0;
											std::string toAppend(1, '\n');

											for(const auto& tempResult : tempResults) {
												++counter;

												toAppend	+= '['
															+ std::to_string(counter)
															+ "] "
															+ tempResult
															+ dateTimeTest(tempResult, dateTimeFormat, dateTimeLocale)
															+ '\n';
											}

											result += toAppend;
										}
									}

									if(properties.resultSubSets) {
										// get subsets
										std::vector<jsoncons::json> tempResults;

										JSONPathTest.getSubSets(jsonTest, tempResults);

										result += "SUBSETS (" + timer.tickStr() + "):";

										if(tempResults.empty())
											result += " [empty]\n";
										else {
											unsigned long counter = 0;
											std::string toAppend(1, '\n');

											for(const auto& tempResult : tempResults) {
												++counter;

												toAppend	+= '['
															+ std::to_string(counter)
															+ "] "
															+ Helper::Json::stringify(tempResult)
															+ '\n';
											}

											result += toAppend;
										}
									}
								}
							}
						}
						catch(const XPathException& e) {
							response = ServerCommandResponse::failed("XPath error - " + e.whatStr());
						}
						catch(const XMLException& e) {
							response = ServerCommandResponse::failed("Could not parse HTML/XML: " + e.whatStr());
						}
						catch(const JSONPointerException& e) {
							response = ServerCommandResponse::failed("JSONPointer error: " + e.whatStr());
						}
						catch(const JSONPathException& e) {
							response = ServerCommandResponse::failed("JSONPath error: " + e.whatStr());
						}
						catch(const JsonException& e) {
							response = ServerCommandResponse::failed("Could not parse JSON: " + e.whatStr());
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
		const std::string replyString(
				Server::generateReply(
						response,
						message
				)
		);

		// send the reply
		this->webServer.send(connection, 200, "application/json", replyString);

		// set thread status to not running
		{
			std::lock_guard<std::mutex> workersLocked(this->workersLock);

			this->workersRunning.at(threadIndex) = false;
		}
	}

	// private static helper function: get algorithm ID from configuration JSON, throws Main::Exception
	unsigned int Server::getAlgoFromConfig(const rapidjson::Document& json) {
		unsigned int result = 0;

		if(!json.IsArray())
			throw Exception("Server::getAlgoFromConfig(): Configuration is no array");

		// go through all array items i.e. configuration entries
		for(const auto& item : json.GetArray()) {
			bool algoItem = false;

			if(item.IsObject()) {
				// go through all item properties
				for(const auto& property : item.GetObject()) {
					if(property.name.IsString()) {
						const std::string itemName(
								property.name.GetString(),
								property.name.GetStringLength()
						);

						if(itemName == "name") {
							if(property.value.IsString()) {
								if(
										std::string(
												property.value.GetString(),
												property.value.GetStringLength()
										) == "_algo"
								) {
									algoItem = true;

									if(result)
										break;
								}
								else
									break;
							}
						}
						else if(itemName == "value") {
							if(property.value.IsUint()) {
								result = property.value.GetUint();

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

	// private static helper function: test query result for date/time
	std::string Server::dateTimeTest(const std::string& input, const std::string& format, const std::string& locale) {
		if(format.empty())
			return "";

		std::string dateTimeString(input);
		std::string result(" [");

		try {
			Helper::DateTime::convertCustomDateTimeToSQLTimeStamp(
					dateTimeString,
					format,
					locale
			);

			result += dateTimeString;
		}
		catch(const LocaleException& e) {
			result += "LOCALE ERROR: " + e.whatStr();
		}
		catch(const DateTimeException& e) {
			result += "DATE/TIME ERROR: " + e.whatStr();
		}

		result += "]";

		return result;
	}

} /* crawlservpp::Main */
