/*
 *
 * ---
 *
 *  Copyright (C) 2021 Anselm Schmidt (ans[ät]ohai.su)
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

	/*
	 * CONSTRUCTION AND DESTRUCTION
	 */

	//! Constructor setting server, database, and network settings.
	/*!
	 * \param serverSettings Constant reference
	 *   to a structure containing the server
	 *   settings.
	 * \param databaseSettings Constant
	 *   reference to a structure containing
	 *   the database settings.
	 * \param networkSettings Constant reference
	 *   to a structure containing the network
	 *   settings.
	 *
	 * \throws Main::Exception if the
	 *  configuration could not be parsed,
	 *  the configuration is invalid, or an
	 *  unknown thread module is encountered.
	 *
	 * \sa Struct::ServerSettings,
	 *   Struct::DatabaseSettings,
	 *   Struct::NetworkSettings
	 *
	 */
	Server::Server(
			const ServerSettings& serverSettings,
			const DatabaseSettings& databaseSettings,
			const NetworkSettings& networkSettings
	)
			: settings(serverSettings),
			  dbSettings(databaseSettings, debugDir),
			  netSettings(networkSettings),
			  database(dbSettings, "server"),
			  allowed(serverSettings.allowedClients),
			  webServer(cacheDir) {

		// initialize directories
		Server::initCacheDir();
		Server::initDir(cookieDir);
		Server::initDir(downloadDir);
		Server::initDebuggingDir(dbSettings.debugLogging, dbSettings.debugDir);

		// initialize server
		this->initDatabase(serverSettings.sleepOnSqlErrorS);
		this->initCallbacks();
		this->initWebServer(serverSettings.port, serverSettings.corsOrigins);
		this->initThreads();
		this->initStartLogging();
	}

	//! Destructor interrupting and waiting for all threads.
	Server::~Server() {
		this->clearModuleThreads();
		this->clearWorkerThreads();
		this->clearLogShutdown();
	}

	/*
	 * GETTERS
	 */

	//! Gets the status of the server
	/*!
	 * \returns Reference to the string
	 *   containing the status of the server.
	 */
	const std::string& Server::getStatus() const {
		return this->status;
	}

	//! Gets the up-time of the server in seconds.
	/*!
	 * \returns The number of seconds that have
	 *   passed since the server has been
	 *   started.
	 */
	std::int64_t Server::getUpTime() const {
		return static_cast<std::int64_t>(
				std::chrono::duration_cast<std::chrono::seconds>(
						std::chrono::steady_clock::now()
						- this->uptimeStart
				).count()
		);
	}

	//! Gets the number of active module threads.
	/*!
	 * \returns The number of module threads
	 *   currently active on the server.
	 */
	std::size_t Server::getActiveThreads() const {
		std::size_t result{};

		// count active module threads
		result += Server::countModuleThreads(this->crawlers);
		result += Server::countModuleThreads(this->parsers);
		result += Server::countModuleThreads(this->extractors);
		result += Server::countModuleThreads(this->analyzers);

		return result;
	}

	//! Gets the number of active worker threads.
	/*!
	 * \returns The number of worker threads
	 *   currently active on the server.
	 */
	std::size_t Server::getActiveWorkers() const {
		std::lock_guard<std::mutex> workersLocked{this->workersLock};

		return std::count(
				this->workersRunning.cbegin(),
				this->workersRunning.cend(),
				true
		);
	}

	/*
	 * SERVER TICK
	 */

	//! Perform a server tick.
	/*!
	 * Polls the web server, handles database
	 *  exceptions, checks for finished threads
	 *  to be cleared, checks the connection to
	 *  the database, gets information about
	 *  the status of the server, and performs
	 *  server commands received from the
	 *  frontend(s).
	 *
	 * \returns True, if the server is still
	 *   running. False otherwise.
	 */
	bool Server::tick() {
		this->tickPollWebServer();
		this->tickRemoveFinishedModuleThreads();
		this->tickRemoveFinishedWorkerThreads();
		this->tickReconnectIfOffline();

		return this->running;
	}

	/*
	 * SETTER (private)
	 */

	// set status of the server
	void Server::setStatus(const std::string& statusMsg) {
		this->status = statusMsg;
	}

	/*
	 * EVENT HANDLERS (private)
	 */

	// handle accept event, throws Main::Exception
	void Server::onAccept(ConnectionPtr connection) {
		const std::string ip{Server::getIp(connection, "onAccept")};

		// check authorization
		if(this->allowed != "*") {
			if(this->allowed.find(ip) == std::string::npos) {
				WebServer::close(connection, true);

				if(this->offline) {
					std::cout << "\nserver rejected client "
							<< ip << "." << std::flush;
				}
				else {
					try {
						this->database.log("rejected client " + ip + ".");
					}
					catch(const DatabaseException& e) {
						// try to re-connect once on database exception
						try {
							this->database.checkConnection();

							this->database.log(
									"re-connected to database after error: "
									+ std::string(e.view())
							);

							this->database.log(
									"rejected client "
									+ ip
									+ "."
							);
						}
						catch(const DatabaseException& e) {
							std::cout << "\nserver rejected client "
									<< ip << "." << std::flush;

							this->offline = true;
						}
					}
				}
			}
			else {
				if(this->offline) {
					std::cout << "\nserver accepted client "
							<< ip << "." << std::flush;
				}
				else {
					try {
						this->database.log(
								"accepted client "
								+ ip
								+ "."
						);
					}
					catch(const DatabaseException& e) {
						// try to re-connect once on database exception
						try {
							this->database.checkConnection();

							this->database.log(
									"re-connected to database after error: "
									+ std::string(e.view())
							);
							this->database.log(
									"accepted client "
									+ ip
									+ "."
							);
						}
						catch(const DatabaseException& e) {
							std::cout << "\nserver rejected client "
									+ ip
									+ "." << std::flush;

							this->offline = true;
						}
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
		const std::string ip{Server::getIp(connection, "onRequest")};

		// check authorization
		if(this->allowed != "*") {
			if(this->allowed.find(ip) == std::string::npos) {
				this->database.log("Client " + ip + " rejected.");

				WebServer::close(connection, true);
			}
		}

		// check for GET request
		if(method == "GET") {
			this->webServer.send(
					connection,
					statusHttpCode,
					statusHttpContentType,
					this->getStatus()
			);
		}
		// check for POST request
		else if(method == "POST") {
			// parse JSON
			bool threadStarted{false};
			bool fileDownload{false};

			const std::string reply(
					this->cmd(
							connection,
							body,
							threadStarted,
							fileDownload
					)
			);

			// send reply, unless a thread has been started
			if(fileDownload) {
				this->webServer.sendFile(connection, reply, data);
			}
			else if(!threadStarted) {
				this->webServer.send(
						connection,
						replyHttpCode,
						replyHttpContentType,
						reply
				);
			}
		}
		else if(method == "OPTIONS") {
			this->webServer.send(connection, optionsHttpCode, "", "");
		}
	}

	/*
	 * SERVER COMMANDS (private)
	 */

	// run a server command, return reply, throws Main::Exception
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
			response = ServerCommandResponse::failed(
					"Database is offline."
			);
		}
		else {
			this->cmdIp = Server::getIp(connection, "cmd");

			// parse JSON
			try {
				this->cmdJson = Helper::Json::parseRapid(msgBody);

				if(!(this->cmdJson.IsObject())) {
					response = ServerCommandResponse::failed(
							"Parsed JSON is not an object."
					);
				}
			}
			catch(const JsonException& e) {
				response = ServerCommandResponse::failed(
						"Could not parse JSON: "
						+ std::string(e.view())
						+ "."
				);
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
										std::lock_guard<std::mutex> workersLocked{
												this->workersLock
										};

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
										std::lock_guard<std::mutex> workersLocked{
											this->workersLock
										};

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
										std::lock_guard<std::mutex> workersLocked{
											this->workersLock
										};

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
								else if(command == "deleteurls") {
									// check whether the deletion of data is allowed
									if(this->settings.dataDeletable) {
										// create a worker thread for deleting URLs (to not block the server)
										{
											std::lock_guard<std::mutex> workersLocked{
												this->workersLock
											};

											this->workersRunning.push_back(true);

											this->workers.emplace_back(
													&Server::cmdDeleteUrls,
													this,
													connection,
													this->workers.size(),
													msgBody
											);
										}

										threadStartedTo = true;
									}
									else {
										response = ServerCommandResponse::failed("Not allowed.");
									}
								}
								else if(command == "testquery") {
									// create a worker thread for testing query (to not block the server)
									{
										std::lock_guard<std::mutex> workersLocked{
											this->workersLock
										};

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
								else if(command == "ping") {
									response = ServerCommandResponse("pong");
								}
								else if(!command.empty()) {
									// unknown command: debug the command and its arguments
									response = ServerCommandResponse::failed(
											"Unknown command '"
											+ command
											+ "'."
									);
								}
								else {
									response = ServerCommandResponse::failed(
											"Empty command."
									);
								}
							}
						}
						catch(const std::exception &e) {
							// exceptions caused by server commands should not kill the server
							//  (and are attributed to the frontend)
							this->database.log("frontend", e.what());

							response = ServerCommandResponse::failed(e.what());
						}
					}
					else {
						response = ServerCommandResponse::failed(
								"Invalid command: Name is not a string."
						);
					}
				}
				else {
					response = ServerCommandResponse::failed(
							"No command specified."
					);
				}
			}

			// clear IP and JSON
			std::string().swap(this->cmdIp);

			rapidjson::Value(rapidjson::kObjectType).Swap(this->cmdJson);
		}

		// return the reply
		return Server::generateReply(response, msgBody);
	}

	// run a basic server command, return whether such a command has been found, throws Main::Exception
	//  NOTE: Server::cmdJson and Server::cmdIp need to be set!
	bool Server::cmd(const std::string& name, ServerCommandResponse& response) {
		/*
		 * NOTE:	Use the MAIN_SERVER_CMD macro to add commands to the server
		 * 			and bind them to the respective member function returning a
		 * 			server response of the type Server::ServerCommandResponse.
		 */

		MAIN_SERVER_CMD("kill", this->cmdKill);
		MAIN_SERVER_CMD("allow", this->cmdAllow);
		MAIN_SERVER_CMD("disallow", this->cmdDisallow);

		MAIN_SERVER_CMD("log", this->cmdLog);
		MAIN_SERVER_CMD("clearlogs", this->cmdClearLogs);

		MAIN_SERVER_CMD("startcrawler", this->cmdStartCrawler);
		MAIN_SERVER_CMD("pausecrawler", this->cmdPauseCrawler);
		MAIN_SERVER_CMD("unpausecrawler", this->cmdUnpauseCrawler);
		MAIN_SERVER_CMD("stopcrawler", this->cmdStopCrawler);

		MAIN_SERVER_CMD("startparser", this->cmdStartParser);
		MAIN_SERVER_CMD("pauseparser", this->cmdPauseParser);
		MAIN_SERVER_CMD("unpauseparser", this->cmdUnpauseParser);
		MAIN_SERVER_CMD("stopparser", this->cmdStopParser);
		MAIN_SERVER_CMD("resetparsingstatus", this->cmdResetParsingStatus);

		MAIN_SERVER_CMD("startextractor", this->cmdStartExtractor);
		MAIN_SERVER_CMD("pauseextractor", this->cmdPauseExtractor);
		MAIN_SERVER_CMD("unpauseextractor", this->cmdUnpauseExtractor);
		MAIN_SERVER_CMD("stopextractor", this->cmdStopExtractor);
		MAIN_SERVER_CMD("resetextractingstatus", this->cmdResetExtractingStatus);

		MAIN_SERVER_CMD("startanalyzer", this->cmdStartAnalyzer);
		MAIN_SERVER_CMD("pauseanalyzer", this->cmdPauseAnalyzer);
		MAIN_SERVER_CMD("unpauseanalyzer", this->cmdUnpauseAnalyzer);
		MAIN_SERVER_CMD("stopanalyzer", this->cmdStopAnalyzer);
		MAIN_SERVER_CMD("resetanalyzingstatus", this->cmdResetAnalyzingStatus);

		MAIN_SERVER_CMD("pauseall", this->cmdPauseAll);
		MAIN_SERVER_CMD("unpauseall", this->cmdUnpauseAll);

		MAIN_SERVER_CMD("addwebsite", this->cmdAddWebsite);
		MAIN_SERVER_CMD("updatewebsite", this->cmdUpdateWebsite);
		MAIN_SERVER_CMD("deletewebsite", this->cmdDeleteWebsite);
		MAIN_SERVER_CMD("duplicatewebsite", this->cmdDuplicateWebsite);

		MAIN_SERVER_CMD("addurllist", this->cmdAddUrlList);
		MAIN_SERVER_CMD("updateurllist", this->cmdUpdateUrlList);
		MAIN_SERVER_CMD("deleteurllist", this->cmdDeleteUrlList);

		MAIN_SERVER_CMD("addquery", this->cmdAddQuery);
		MAIN_SERVER_CMD("updatequery", this->cmdUpdateQuery);
		MAIN_SERVER_CMD("movequery", this->cmdMoveQuery);
		MAIN_SERVER_CMD("deletequery", this->cmdDeleteQuery);
		MAIN_SERVER_CMD("duplicatequery", this->cmdDuplicateQuery);

		MAIN_SERVER_CMD("addconfig", this->cmdAddConfig);
		MAIN_SERVER_CMD("updateconfig", this->cmdUpdateConfig);
		MAIN_SERVER_CMD("deleteconfig", this->cmdDeleteConfig);
		MAIN_SERVER_CMD("duplicateconfig", this->cmdDuplicateConfig);

		MAIN_SERVER_CMD("listdicts", Server::cmdListDicts);
		MAIN_SERVER_CMD("listmdls", Server::cmdListMdls);

		MAIN_SERVER_CMD("warp", this->cmdWarp);

		return false;
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

		return ServerCommandResponse::toBeConfirmed(
				"Are you sure to kill the server?"
		);
	}

	// server command allow(ip): allow acces for the specified IP(s)
	Server::ServerCommandResponse Server::cmdAllow() {
		// get argument
		std::string ip;
		std::string error;

		if(!(this->getArgument("ip", ip, false, true, error))) {
			return ServerCommandResponse::failed(error);
		}

		// allow needs to be confirmed
		if(this->cmdJson.HasMember("confirmed")) {
			// add IP(s)
			this->allowed += "," + ip;

			// allow is a logged command
			this->database.log(
					ip
					+ " allowed by "
					+ this->cmdIp
					+ "."
			);

			return ServerCommandResponse(
					"Allowed IPs: "
					+ this->allowed + "."
			);
		}

		return ServerCommandResponse::toBeConfirmed(
				"Do you really want to allow "
				+ ip
				+ " access to the server?"
		);
	}

	// server command disallow: revoke access from all manually added IPs
	Server::ServerCommandResponse Server::cmdDisallow() {
		// reset allowed IP(s)
		this->allowed = this->settings.allowedClients;

		// disallow is a logged command
		this->database.log("Allowed IPs reset by " + this->cmdIp + ".");

		return ServerCommandResponse("Allowed IP(s): " + this->allowed + ".");
	}

	// server command log(entry): write a log entry by the frontend into the database
	Server::ServerCommandResponse Server::cmdLog() {
		// get argument
		std::string entry;
		std::string error;

		if(!(this->getArgument("entry", entry, false, false, error))) {
			return ServerCommandResponse::failed(error);
		}

		// write log entry
		this->database.log("frontend", entry);

		return ServerCommandResponse("Wrote log entry: " + entry);
	}

	// server command clearlog([module]):	remove all log entries or the log entries of a specific module
	// 										(or all log entries when module is empty/not given)
	Server::ServerCommandResponse Server::cmdClearLogs() {
		// check whether the clearing of logs is allowed
		if(!(this->settings.logsDeletable)) {
			return ServerCommandResponse::failed("Not allowed.");
		}

		// get argument
		std::string module;
		std::string error;

		this->getArgument("module", module, true, false, error);

		// clearlog needs to be confirmed
		if(this->cmdJson.HasMember("confirmed")) {
			this->database.clearLogs(module);

			// clearlog is a logged command
			if(!module.empty()) {
				this->database.log(
						"Logs of "
						+ module
						+ " cleared by "
						+ this->cmdIp
						+ "."
				);

				return ServerCommandResponse("Logs of " + module + " cleared.");
			}

			this->database.log("All logs cleared by " + this->cmdIp + ".");

			return ServerCommandResponse("All logs cleared.");
		}

		std::ostringstream replyStrStr;

		replyStrStr << "Are you sure to delete ";

		const auto num{this->database.getNumberOfLogEntries(module)};

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
		std::uint64_t website{};
		std::uint64_t urlList{};
		std::uint64_t config{};
		std::string error;

		if(
				!(this->getArgument("website", website, error))
				|| !(this->getArgument("urllist", urlList, error))
				|| !(this->getArgument("config", config, error))
		) {
			return ServerCommandResponse::failed(error);
		}

		// check arguments
		ServerCommandResponse response;

		if(
				!Server::checkWebsite(this->database, website, response)
				|| !Server::checkUrlList(this->database, website, urlList, response)
				|| !(this->checkConfig(website, config, response))
		) {
			return response;
		}

#ifndef MAIN_SERVER_DEBUG_NOCRAWLERS
		const ThreadOptions options{"crawler", website, urlList, config};

		// create crawler
		this->crawlers.push_back(
				std::make_unique<Module::Crawler::Thread>(
						this->database,
						cookieDir,
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
#else
		return ServerCommandResponse("Crawlers are deactivated.");
#endif
	}

	// server command pausecrawler(id): pause a crawler by its ID
	Server::ServerCommandResponse Server::cmdPauseCrawler() {
		// get argument
		std::uint64_t id{};
		std::string error;

		if(!(this->getArgument("id", id, error))) {
			return ServerCommandResponse::failed(error);
		}

		// find crawler
		auto it{
			std::find_if(this->crawlers.cbegin(), this->crawlers.cend(),
					[&id](const auto& p) {
						return p->Module::Thread::getId() == id;
					}
			)
		};

		if(it == this->crawlers.cend()) {
			return ServerCommandResponse::failed(
					"Could not find crawler #"
					+ std::to_string(id)
					+ "."
			);
		}

		// pause crawler
		(*it)->Module::Thread::pause();

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
		std::uint64_t id{};
		std::string error;

		if(!(this->getArgument("id", id, error))) {
			return ServerCommandResponse::failed(error);
		}

		// find crawler
		auto it{
			std::find_if(this->crawlers.cbegin(), this->crawlers.cend(),
					[&id](const auto& p) {
						return p->Module::Thread::getId() == id;
					}
			)
		};

		if(it == this->crawlers.cend()) {
			return ServerCommandResponse::failed(
					"Could not find crawler #"
					+ std::to_string(id)
					+ "."
			);
		}

		// unpause crawler
		(*it)->Module::Thread::unpause();

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
		std::uint64_t id{};
		std::string error;

		if(!(this->getArgument("id", id, error))) {
			return ServerCommandResponse::failed(error);
		}

		// find crawler
		auto it{
			std::find_if(this->crawlers.cbegin(), this->crawlers.cend(),
					[&id](const auto& p) {
						return p->Module::Thread::getId() == id;
					}
			)
		};

		if(it == this->crawlers.cend()) {
			return ServerCommandResponse::failed(
					"Could not find crawler #"
					+ std::to_string(id)
					+ "."
			);
		}

		// interrupt crawler
		(*it)->Module::Thread::stop();

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
		std::uint64_t website{};
		std::uint64_t urlList{};
		std::uint64_t config{};
		std::string error;

		if(
				!(this->getArgument("website", website, error))
				|| !(this->getArgument("urllist", urlList, error))
				|| !(this->getArgument("config", config, error))
		) {
			return ServerCommandResponse::failed(error);
		}

		// check arguments
		ServerCommandResponse response;

		if(
				!Server::checkWebsite(this->database, website, response)
				|| !Server::checkUrlList(this->database, website, urlList, response)
				|| !(this->checkConfig(website, config, response))
		) {
			return response;
		}

#ifndef MAIN_SERVER_DEBUG_NOPARSERS
		const ThreadOptions options{"parser", website, urlList, config};

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
#else
		return ServerCommandResponse("Parsers are deactivated.");
#endif
	}

	// server command pauseparser(id): pause a parser by its ID
	Server::ServerCommandResponse Server::cmdPauseParser() {
		// get argument
		std::uint64_t id{};
		std::string error;

		if(!(this->getArgument("id", id, error))) {
			return ServerCommandResponse::failed(error);
		}

		// find parser
		auto it{
			std::find_if(this->parsers.cbegin(), this->parsers.cend(),
					[&id](const auto& p) {
						return p->Module::Thread::getId() == id;
					}
			)
		};

		if(it == this->parsers.cend()) {
			return ServerCommandResponse::failed(
					"Could not find parser #"
					+ std::to_string(id)
					+ "."
			);
		}

		// pause parser
		(*it)->Module::Thread::pause();

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
		std::uint64_t id{};
		std::string error;

		if(!(this->getArgument("id", id, error))) {
			return ServerCommandResponse::failed(error);
		}

		// find parser
		auto it{
			std::find_if(this->parsers.cbegin(), this->parsers.cend(),
					[&id](const auto& p) {
						return p->Module::Thread::getId() == id;
					}
			)
		};

		if(it == this->parsers.cend()) {
			return ServerCommandResponse::failed(
					"Could not find parser #"
					+ std::to_string(id)
					+ "."
			);
		}

		// unpause parser
		(*it)->Module::Thread::unpause();

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
		std::uint64_t id{};
		std::string error;

		if(!(this->getArgument("id", id, error))) {
			return ServerCommandResponse::failed(error);
		}

		// find parser
		auto it{
			std::find_if(this->parsers.cbegin(), this->parsers.cend(),
					[&id](const auto& p) {
						return p->Module::Thread::getId() == id;
					}
			)
		};

		if(it == this->parsers.cend()) {
			return ServerCommandResponse::failed(
					"Could not find parser #"
					+ std::to_string(id)
					+ "."
			);
		}

		// interrupt parser
		(*it)->Module::Thread::stop();

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
		std::uint64_t urlList{};
		std::string error;

		if(!(this->getArgument("urllist", urlList, error))) {
			return ServerCommandResponse::failed(error);
		}

		// resetparsingstatus needs to be confirmed
		if(this->cmdJson.HasMember("confirmed")) {
			// reset parsing status
			this->database.resetParsingStatus(urlList);

			return ServerCommandResponse("Parsing status reset.");
		}

		return ServerCommandResponse::toBeConfirmed(
				"Are you sure that you want to reset"
				" the parsing status of this URL list?"
		);
	}

	// server command startextractor(website, urllist, config):
	//  start an extractor using the specified website, URL list and configuration
	Server::ServerCommandResponse Server::cmdStartExtractor() {
		// get arguments
		std::uint64_t website{};
		std::uint64_t urlList{};
		std::uint64_t config{};
		std::string error;

		if(
				!(this->getArgument("website", website, error))
				|| !(this->getArgument("urllist", urlList, error))
				|| !(this->getArgument("config", config, error))
		) {
			return ServerCommandResponse::failed(error);
		}

		// check arguments
		ServerCommandResponse response;

		if(
				!Server::checkWebsite(this->database, website, response)
				|| !Server::checkUrlList(this->database, website, urlList, response)
				|| !(this->checkConfig(website, config, response))
		) {
			return response;
		}

#ifndef MAIN_SERVER_DEBUG_NOEXTRACTORS
		const ThreadOptions options{"extractor", website, urlList, config};

		// create extractor
		this->extractors.push_back(
				std::make_unique<Module::Extractor::Thread>(
						this->database,
						cookieDir,
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
#else
		return ServerCommandResponse("Extractors are deactivated.");
#endif
	}

	// server command pauseextractor(id): pause an extractor by its ID
	Server::ServerCommandResponse Server::cmdPauseExtractor() {
		// get argument
		std::uint64_t id{};
		std::string error;

		if(!(this->getArgument("id", id, error))) {
			return ServerCommandResponse::failed(error);
		}

		// find extractor
		auto it{
			std::find_if(this->extractors.cbegin(), this->extractors.cend(),
					[&id](const auto& p) {
						return p->Module::Thread::getId() == id;
					}
			)
		};

		if(it == this->extractors.cend()) {
			return ServerCommandResponse::failed(
					"Could not find extractor #"
					+ std::to_string(id)
					+ "."
			);
		}

		// pause parser
		(*it)->Module::Thread::pause();

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
		std::uint64_t id{};
		std::string error;

		if(!(this->getArgument("id", id, error))) {
			return ServerCommandResponse::failed(error);
		}

		// find extractor
		auto it{
			std::find_if(this->extractors.cbegin(), this->extractors.cend(),
					[&id](const auto& p) {
						return p->Module::Thread::getId() == id;
					}
			)
		};

		if(it == this->extractors.cend()) {
			return ServerCommandResponse::failed(
					"Could not find extractor #"
					+ std::to_string(id)
					+ "."
			);
		}

		// unpause extractor
		(*it)->Module::Thread::unpause();

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
		std::uint64_t id{};
		std::string error;

		if(!(this->getArgument("id", id, error))) {
			return ServerCommandResponse::failed(error);
		}

		// find extractor
		auto it{
			std::find_if(this->extractors.cbegin(), this->extractors.cend(),
					[&id](const auto& p) {
						return p->Module::Thread::getId() == id;
					}
			)
		};

		if(it == this->extractors.cend()) {
			return ServerCommandResponse::failed(
					"Could not find extractor #"
					+ std::to_string(id)
					+ "."
			);
		}

		// interrupt extractor
		(*it)->Module::Thread::stop();

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
		std::uint64_t urlList{};
		std::string error;

		if(!(this->getArgument("urllist", urlList, error))) {
			return ServerCommandResponse::failed(error);
		}

		// resetextractingstatus needs to be confirmed
		if(this->cmdJson.HasMember("confirmed")) {
			// reset extracting status
			this->database.resetExtractingStatus(urlList);

			return ServerCommandResponse("Extracting status reset.");
		}

		return ServerCommandResponse::toBeConfirmed(
				"Are you sure that you want to reset"
				" the extracting status of this URL list?"
		);
	}

	// server command startanalyzer(website, urllist, config):
	//  start an analyzer using the specified website, URL list, module and configuration
	Server::ServerCommandResponse Server::cmdStartAnalyzer() {
		// get arguments
		std::uint64_t website{};
		std::uint64_t urlList{};
		std::uint64_t config{};
		std::string error;

		if(
				!(this->getArgument("website", website, error))
				|| !(this->getArgument("urllist", urlList, error))
				|| !(this->getArgument("config", config, error))
		) {
			return ServerCommandResponse::failed(error);
		}

		// check arguments
		ServerCommandResponse response;

		if(
				!Server::checkWebsite(this->database, website, response)
				|| !Server::checkUrlList(this->database, website, urlList, response)
				|| !(this->checkConfig(website, config, response))
		) {
			return response;
		}

		// get analyzer configuration
		const std::string analyzerConfig{
				this->database.getConfiguration(config)
		};

		// check configuration JSON
		rapidjson::Document configJson;

		try {
			configJson = Helper::Json::parseRapid(analyzerConfig);
		}
		catch(const JsonException& e) {
			return ServerCommandResponse::failed(
					"Could not parse analyzing configuration: "
					+ std::string(e.view())
					+ "."
			);
		}

		if(!configJson.IsArray()) {
			return ServerCommandResponse::failed(
					"Parsed analyzing configuration is not an array."
			);
		}

		// get algorithm from configuration
		const auto algo{Server::getAlgoFromConfig(configJson)};

		if(algo == 0) {
			return ServerCommandResponse::failed(
					"Analyzing configuration does not include an algorithm."
			);
		}

#ifndef MAIN_SERVER_DEBUG_NOANALYZERS
		const ThreadOptions options{"analyzer", website, urlList, config};

		// try to create algorithm thread
		this->analyzers.push_back(
				Module::Analyzer::Algo::initAlgo(
						AlgoThreadProperties(
								algo,
								options,
								this->database
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
#else
		return ServerCommandResponse("Analyzers are deactivated.");
#endif
	}

	// server command pauseparser(id): pause a parser by its ID
	Server::ServerCommandResponse Server::cmdPauseAnalyzer() {
		// get argument
		std::uint64_t id{};
		std::string error;

		if(!(this->getArgument("id", id, error))) {
			return ServerCommandResponse::failed(error);
		}

		// find analyzer
		auto it{
			std::find_if(this->analyzers.cbegin(), this->analyzers.cend(),
					[&id](const auto& p) {
						return p->Module::Thread::getId() == id;
					}
			)
		};

		if(it == this->analyzers.cend()) {
			return ServerCommandResponse::failed(
					"Could not find analyzer #"
					+ std::to_string(id)
					+ "."
			);
		}

		// pause analyzer
		if((*it)->Module::Thread::pause()) {
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
		return ServerCommandResponse::failed(
				"This algorithm cannot be paused at the moment."
		);
	}

	// server command unpauseanalyzer(id): unpause a parser by its ID
	Server::ServerCommandResponse Server::cmdUnpauseAnalyzer() {
		// get argument
		std::uint64_t id{};
		std::string error;

		if(!(this->getArgument("id", id, error))) {
			return ServerCommandResponse::failed(error);
		}

		// find analyzer
		auto it{
			std::find_if(this->analyzers.cbegin(), this->analyzers.cend(),
					[&id](const auto& p) {
						return p->Module::Thread::getId() == id;
					}
			)
		};

		if(it == this->analyzers.cend()) {
			return ServerCommandResponse::failed(
					"Could not find analyzer #"
					+ std::to_string(id)
					+ "."
			);
		}

		// unpause analyzer
		(*it)->Module::Thread::unpause();

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
		std::uint64_t id{};
		std::string error;

		if(!(this->getArgument("id", id, error))) {
			return ServerCommandResponse::failed(error);
		}

		// find analyzer
		auto it{
			std::find_if(this->analyzers.cbegin(), this->analyzers.cend(),
					[&id](const auto& p) {
						return p->Module::Thread::getId() == id;
					}
			)
		};

		if(it == this->analyzers.cend()) {
			return ServerCommandResponse::failed(
					"Could not find analyzer #"
					+ std::to_string(id)
					+ "."
			);
		}

		// interrupt analyzer
		(*it)->Module::Thread::stop();

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
		std::uint64_t urlList{};
		std::string error;

		if(!(this->getArgument("urllist", urlList, error))) {
			return ServerCommandResponse::failed(error);
		}

		// resetanalyzingstatus needs to be confirmed
		if(this->cmdJson.HasMember("confirmed")) {
			// reset analyzing status
			this->database.resetAnalyzingStatus(urlList);

			return ServerCommandResponse("Analyzing status reset.");
		}

		return ServerCommandResponse::toBeConfirmed(
				"Are you sure that you want to reset"
				" the analyzing status of this URL list?"
		);
	}

	// server command pauseall(): pause all running threads
	Server::ServerCommandResponse Server::cmdPauseAll() {
		std::size_t counter{};

		// pause crawlers
		for(const auto& thread : this->crawlers) {
			if(!(thread->isPaused())) {
				if(thread->Module::Thread::pause()) {
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
		}

		// pause parsers
		for(const auto& thread : this->parsers) {
			if(!(thread->isPaused())) {
				if(thread->Module::Thread::pause()) {
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
		}

		// pause extractors
		for(const auto& thread : this->extractors) {
			if(!(thread->isPaused())) {
				if(thread->Module::Thread::pause()) {
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
		}

		// pause analyzers
		for(const auto& thread : this->analyzers) {
			if(!(thread->isPaused())) {
				if(thread->Module::Thread::pause()) {
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
		}

		switch(counter) {
		case 0:
			return ServerCommandResponse("No threads to pause.");

		case 1:
			return ServerCommandResponse("One thread is pausing.");

		default:
			return ServerCommandResponse(
					std::to_string(counter)
					+ " threads are pausing."
			);
		}
	}

	// server command unpauseall(): unpause all paused threads
	Server::ServerCommandResponse Server::cmdUnpauseAll() {
		std::size_t counter{};

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
			return ServerCommandResponse(
					std::to_string(counter)
					+ " threads have been unpaused."
			);
		}
	}

	// server command addwebsite([crossdomain], [domain], namespace, name, [dir]): add a website
	Server::ServerCommandResponse Server::cmdAddWebsite() {
		// get arguments
		bool isCrossDomain{false};
		std::string domain;
		std::string nameSpace;
		std::string name;
		std::string dir;
		std::string error;
		std::uint64_t id{};

		if(
				!(this->getArgument("crossdomain", isCrossDomain, true, error))
				|| !(this->getArgument("namespace", nameSpace, false, true, error))
				|| !(this->getArgument("name", name, false, true, error))
				|| (!isCrossDomain && !(this->getArgument("domain", domain, true, false, error)))
				|| !(this->getArgument("dir", dir, true, false, error))
		) {
			return ServerCommandResponse::failed(error);
		}

		if(isCrossDomain && !domain.empty()) {
			// custom error message if domain is set for cross-domain website
			return ServerCommandResponse::failed(
					"Cannot use domain when website is cross-domain"
			);
		}

		if(!isCrossDomain && domain.empty()) {
			// custom error message if domain is empty and website is not cross-domain
			return ServerCommandResponse::failed(
					"Domain cannot be empty when website is not cross-domain."
			);
		}

		if(!dir.empty()) {
			// trim path and remove last separator if necessary
			Helper::Strings::trim(dir);

			if(dir.size() > 1 && dir.back() == Helper::FileSystem::getPathSeparator()) {
				dir.pop_back();
			}

			// check for empty path
			if(dir.empty()) {
				return ServerCommandResponse::failed(
						"External directory cannot be empty when used."
				);
			}
		}

		// check domain name
		if(!(Helper::Strings::checkDomainName(domain))) {
			return ServerCommandResponse::failed(
					"Invalid character(s) in domain."
			);
		}

		// check namespace name
		if(nameSpace.length() < minNameSpaceLength) {
			return ServerCommandResponse::failed(
					"Website namespace has to be at least "
					+ std::string(minNameSpaceLengthString)
					+ " characters long."
			);
		}

		if(!(Helper::Strings::checkSQLName(nameSpace))) {
			return ServerCommandResponse::failed(
					"Invalid character(s) in website namespace."
			);
		}

		// correct and check domain name if necessary
		if(!isCrossDomain) {
			Server::correctDomain(domain);

			if(domain.empty()) {
				return ServerCommandResponse::failed("Domain is empty.");
			}
		}

		// check for external directory
		if(dir.empty()) {
			// add website to database
			id = this->database.addWebsite({domain, nameSpace, name, dir});
		}
		else {
			// adding a website using an external directory needs to be confirmed
			if(this->cmdJson.HasMember("confirmed")) {
				// try adding website to the database using an external directory
				try {
					id = this->database.addWebsite({domain, nameSpace, name, dir});
				}
				catch(const IncorrectPathException& e) {
					return ServerCommandResponse::failed(
							"Incorrect path for external directory.\n\n"
							+ std::string(e.view())
					);
				}
				catch(const PrivilegesException& e) {
					return ServerCommandResponse::failed(
							"The MySQL user used by the server needs to have"
							" FILE privilege to use an external directory.\n\n"
							+ std::string(e.view())
					);
				}
				catch(const WrongArgumentsException& e) {
					return ServerCommandResponse::failed(
							"The MySQL server did not accept the external data directory.\n"
							"Make sure it is not located inside its main data directory\n"
							"as such an external directory cannot be symlinked to.\n\n"
							+ std::string(e.view())
					);
				}
				catch(const StorageEngineException& e) {
					return ServerCommandResponse::failed(
							"Could not access the external directory. Make sure that\n"
							"* the MySQL server has permission to write into the directory\n"
							"* the AppArmor profile of the MySQL server allows access to"
							" the directory (if applicable)\n"
							"* file-per-table tablespace (innodb_file_per_table) is enabled\n\n"
							+ std::string(e.view())
					);
				}
			}
			else {
				std::string confirmation(
						"Do you really want to use an external directory?"
				);

				if(!(this->database.checkDataDir(dir))) {
					confirmation += "\n\n"
									"WARNING: The external directory seems to be"
									" unknown to the MySQL server.\n"
									"Add it to 'innodb_directories' to ensure"
									" fail-safe operations.";
				}

				return ServerCommandResponse::toBeConfirmed(confirmation);
			}
		}

		if(id == 0) {
			return ServerCommandResponse::failed("Could not add website to database.");
		}

		return ServerCommandResponse("Website added.", id);
	}

	// server command updatewebsite(id, crossdomain, domain, namespace, name, [dir]): edit a website
	Server::ServerCommandResponse Server::cmdUpdateWebsite() {
		// get arguments
		std::uint64_t id{};
		bool isCrossDomain{false};
		std::string domain;
		std::string nameSpace;
		std::string name;
		std::string dir;
		std::string error;

		if(
				!(this->getArgument("id", id, error))
				|| !(this->getArgument("crossdomain", isCrossDomain, true, error))
				|| (!isCrossDomain && !(this->getArgument("domain", domain, true, false, error)))
				|| !(this->getArgument("namespace", nameSpace, false, true, error))
				|| !(this->getArgument("name", name, false, true, error))
				|| !(this->getArgument("dir", dir, true, false, error))
		) {
			return ServerCommandResponse::failed(error);
		}

		if(isCrossDomain && !domain.empty()) {
			// custom error message if domain is set for cross-domain website
			return ServerCommandResponse::failed(
					"Cannot use domain when website is cross-domain"
			);
		}

		if(!isCrossDomain && domain.empty()) {
			// custom error message if domain is empty and website is not cross-domain
			return ServerCommandResponse::failed(
					"Domain cannot be empty when website is not cross-domain."
			);
		}

		if(!dir.empty()) {
			// trim path and remove last separator if necessary
			Helper::Strings::trim(dir);

			if(
					dir.size() > 1
					&& dir.back() == Helper::FileSystem::getPathSeparator()
			) {
				dir.pop_back();
			}

			// check for empty path
			if(dir.empty()) {
				return ServerCommandResponse::failed(
						"External directory cannot be empty when used."
				);
			}
		}

		// check namespace name
		ServerCommandResponse response;

		if(!Server::checkNameSpace(nameSpace, response)) {
			return response;
		}

		// correct and check domain name if necessary
		if(!isCrossDomain) {
			Server::correctDomain(domain);

			if(domain.empty()) {
				return ServerCommandResponse::failed("Domain is empty.");
			}
		}

		// check website or whether any thread is using it
		if(
				!Server::checkWebsite(this->database, id, response)
				|| this->isWebsiteInUse(id, response)
		) {
			return response;
		}

		// check for domain and directory change
		const bool domainChanged{this->database.getWebsiteDomain(id) != domain};
		const bool dirChanged{this->database.getWebsiteDataDirectory(id) != dir};

		const WebsiteProperties properties(domain, nameSpace, name, dir);

		// check for confirmation if domain or directory have been changed
		if(this->cmdJson.HasMember("confirmed") || (!domainChanged && !dirChanged)) {
			// update website in database
			try {
				this->database.updateWebsite(id, properties);

				return ServerCommandResponse("Website updated.");
			}
			catch(const IncorrectPathException &e) {
				return ServerCommandResponse::failed(
						"Incorrect path for external directory.\n\n"
						 + std::string(e.view())
				);
			}
			catch(const PrivilegesException &e) {
				return ServerCommandResponse::failed(
						"The MySQL user used by the server needs to have"
						" FILE privilege to use an external directory.\n\n"
						 + std::string(e.view())
				);
			}
			catch(const WrongArgumentsException& e) {
				return ServerCommandResponse::failed(
						"The MySQL server did not accept the external directory.\n"
						"Make sure it is not located inside its main data directory\n"
						"as such an external directory cannot be symlinked to.\n\n"
						+ std::string(e.view())
				);
			}
			catch(const StorageEngineException &e) {
				return ServerCommandResponse::failed(
						"Could not access the external directory. Make sure that\n"
						"* the MySQL server has permission to write into the directory\n"
						"* the AppArmor profile of the MySQL server allows access to"
						" the directory (if applicable)\n"
						"* file-per-table tablespace (innodb_file_per_table) is enabled\n\n"
						+ std::string(e.view())
				);
			}
		}

		// change of domain or directory needs to be confirmed
		std::ostringstream confirmationStrStr;

		// handle domain change
		if(domainChanged) {
			const auto toModify{
				this->database.getChangedUrlsByWebsiteUpdate(id, properties)
			};
			const auto toDelete{
				this->database.getLostUrlsByWebsiteUpdate(id, properties)
			};

			if(toModify > 0 || toDelete > 0) {
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
			if(domainChanged) {
				confirmationStrStr << '\n';
			}

			// check for external directory
			if(dir.empty()) {
				confirmationStrStr <<	"Do you really want to change"
										" the data directory to default?";
			}
			else {
				if(this->database.getWebsiteDataDirectory(id).empty()) {
					confirmationStrStr <<	"Do you really want to change"
											" the data directory to an external"
											" directory?";
				}
				else {
					confirmationStrStr <<	"Do you really want to change"
											" the data directory to another"
											" external directory?";
				}

				if(!(this->database.checkDataDir(dir))) {
					confirmationStrStr <<	"\n\n"
											"WARNING: The external directory"
											" seems to be unknown to the MySQL server\n"
											" - add it to 'innodb_directories'"
											" to ensure fail-safe operations.";
				}
			}

			confirmationStrStr <<	"\n\n"
									"NOTE: This change might occupy the server"
									" for a while\n"
									" - no other commands will be processed"
									" during that time.";
		}

		return ServerCommandResponse::toBeConfirmed(confirmationStrStr.str());
	}

	// server command deletewebsite(id): delete the ID-specified website and all associated data from the database
	Server::ServerCommandResponse Server::cmdDeleteWebsite() {
		// check whether the deletion of data is allowed
		if(!(this->settings.dataDeletable)) {
			return ServerCommandResponse::failed("Not allowed.");
		}

		// get argument
		std::uint64_t id{};
		std::string error;

		if(!(this->getArgument("id", id, error))) {
			return ServerCommandResponse::failed(error);
		}

		// check website or whether any thread is using it
		ServerCommandResponse response;

		if(
				!Server::checkWebsite(this->database, id, response)
				|| this->isWebsiteInUse(id, response)
		) {
			return response;
		}

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
		std::uint64_t id{};
		std::string error;

		if(!(this->getArgument("id", id, error))) {
			return ServerCommandResponse::failed(error);
		}

		if(!(this->cmdJson.HasMember("queries"))) {
			return ServerCommandResponse::failed(
					"Invalid arguments ('queries' is missing)."
			);
		}

		if(!(this->cmdJson["queries"].IsObject())) {
			return ServerCommandResponse::failed(
					"Invalid arguments ('queries' is not a valid JSON object)."
			);
		}

		// get queries from JSON
		Queries queries;

		for(const auto& pair : this->cmdJson["queries"].GetObject()) {
			const std::string moduleName(
					pair.name.GetString(),
					pair.name.GetStringLength()
			);

			if(moduleName.empty()) {
				continue;
			}

			if(!pair.value.IsArray()) {
				return ServerCommandResponse::failed(
						"Invalid arguments ('"
						+ Helper::Json::stringify(pair.value)
						+ "' in 'queries' is not a valid JSON array)."
				);
			}

			auto moduleIt{
				std::find_if(queries.begin(), queries.end(),
						[&moduleName](const auto& p) {
							return p.first == moduleName;
						}
				)
			};

			if(moduleIt == queries.end()) {
				queries.emplace_back(
					moduleName,
					std::vector<StringString>()
				);

				moduleIt = queries.end();

				--moduleIt;
			}

			for(const auto& queryCatName : pair.value.GetArray()) {
				if(!queryCatName.IsObject()) {
					return ServerCommandResponse::failed(
							"Invalid arguments ('"
							+ Helper::Json::stringify(queryCatName)
							+ "' in 'queries['"
							+ moduleName
							+ "']' is not a valid JSON object)."
					);
				}

				if(!queryCatName.HasMember("cat")) {
					return ServerCommandResponse::failed(
							"Invalid arguments ('"
							+ Helper::Json::stringify(queryCatName)
							+ "' in 'queries['"
							+ moduleName
							+ "']' does not contain 'cat')."
					);
				}

				if(!queryCatName["cat"].IsString()) {
					return ServerCommandResponse::failed(
							"Invalid arguments ('"
							+ Helper::Json::stringify(queryCatName["cat"])
							+ "' in 'queries['"
							+ moduleName
							+ "']' is not a valid string)."
					);
				}

				if(!queryCatName.HasMember("name")) {
					return ServerCommandResponse::failed(
							"Invalid arguments ('"
							+ Helper::Json::stringify(queryCatName)
							+ "' in 'queries['"
							+ moduleName
							+ "']' does not contain 'name')."
					);
				}

				if(!queryCatName["name"].IsString()) {
					return ServerCommandResponse::failed(
							"Invalid arguments ('"
							+ Helper::Json::stringify(queryCatName["name"])
							+ "' in 'queries['"
							+ moduleName
							+ "']' is not a valid string)."
					);
				}

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
		ServerCommandResponse response;

		if(!Server::checkWebsite(this->database, id, response)) {
			return response;
		}

		// duplicate website configuration
		const auto newId{this->database.duplicateWebsite(id, queries)};

		if(newId == 0) {
			return ServerCommandResponse::failed(
					"Could not add duplicate to the database."
			);
		}

		return ServerCommandResponse("Website duplicated.", newId);
	}

	// server command addurllist(website, namespace, name): add a URL list to the ID-specified website
	Server::ServerCommandResponse Server::cmdAddUrlList() {
		// get arguments
		std::uint64_t website{};
		std::string nameSpace;
		std::string name;
		std::string error;

		if(
				!(this->getArgument("website", website, error))
				|| !(this->getArgument("namespace", nameSpace, false, true, error))
				|| !(this->getArgument("name", name, false, true, error))
		) {
			return ServerCommandResponse::failed(error);
		}

		// check namespace name
		ServerCommandResponse response;

		if(!Server::checkNameSpace(nameSpace, response)) {
			return response;
		}

		// check website
		if(!Server::checkWebsite(this->database, website, response)) {
			return response;
		}

		// add URL list to database
		const auto id{this->database.addUrlList(website, {nameSpace, name})};

		if(id == 0) {
			return ServerCommandResponse::failed(
					"Could not add URL list to database."
			);
		}

		return ServerCommandResponse("URL list added.", id);
	}

	// server command updateurllist(id, namespace, name): edit a URL list
	Server::ServerCommandResponse Server::cmdUpdateUrlList() {
		// get arguments
		std::uint64_t id{};
		std::string nameSpace;
		std::string name;
		std::string error;

		if(
				!(this->getArgument("id", id, error))
				|| !(this->getArgument("namespace", nameSpace, false, true, error))
				|| !(this->getArgument("name", name, false, true, error))
		) {
			return ServerCommandResponse::failed(error);
		}

		// check namespace
		if(nameSpace.length() < minNameSpaceLength) {
			return ServerCommandResponse::failed(
					"Namespace of URL list has to be at least three characters long."
			);
		}

		if(!(Helper::Strings::checkSQLName(nameSpace))) {
			return ServerCommandResponse::failed(
					"Invalid character(s) in namespace of URL list."
			);
		}

		if(nameSpace == "config") {
			return ServerCommandResponse::failed(
					"Namespace of URL list cannot be 'config'."
			);
		}

		// check URL list
		ServerCommandResponse response;

		if(!Server::checkUrlList(this->database, id, response)) {
			return response;
		}

		if(this->isUrlListInUse(id, response)) {
			return response;
		}

		// update URL list in database
		this->database.updateUrlList(id, {nameSpace, name});

		return ServerCommandResponse("URL list updated.");
	}

	// server command deleteurllist(id): delete a URL list and all associated data from the database by its ID
	Server::ServerCommandResponse Server::cmdDeleteUrlList() {
		// check whether the deletion of data is allowed
		if(!(this->settings.dataDeletable)) {
			return ServerCommandResponse::failed("Not allowed.");
		}

		// get argument
		std::uint64_t id{};
		std::string error;

		if(!(this->getArgument("id", id, error))) {
			return ServerCommandResponse::failed(error);
		}

		ServerCommandResponse response;

		// check URL list
		if(!Server::checkUrlList(this->database, id, response)) {
			return response;
		}

		// check whether any thread is using the URL list
		if(this->isUrlListInUse(id, response)) {
			return response;
		}

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

	// server command addquery(website, name, query, type, resultbool, resultsingle, resultmulti, resultsubsets, textonly):
	//  add a query
	Server::ServerCommandResponse Server::cmdAddQuery() {
		// get arguments
		std::uint64_t website{};
		std::string name;
		std::string query;
		std::string type;
		bool resultBool{false};
		bool resultSingle{false};
		bool resultMulti{false};
		bool resultSubSets{false};
		bool textOnly{false};
		std::string error;

		if(
				!(this->getArgument("website", website, error))
				|| !(this->getArgument("name", name, false, true, error))
				|| !(this->getArgument("query", query, false, true, error))
				|| !(this->getArgument("type", type, false, true, error))
				|| !(this->getArgument("resultbool", resultBool, false, error))
				|| !(this->getArgument("resultsingle", resultSingle, false, error))
				|| !(this->getArgument("resultmulti", resultMulti, false, error))
				|| !(this->getArgument("resultsubsets", resultSubSets, false, error))
				|| !(this->getArgument("textonly", textOnly, false, error))
		) {
			return ServerCommandResponse::failed(error);
		}

		if(
				type != "regex"
				&& type != "xpath"
				&& type != "jsonpointer"
				&& type != "jsonpath"
				&& type != "xpathjsonpointer"
				&& type != "xpathjsonpath"
		) {
			return ServerCommandResponse::failed(
					"Unknown query type: '"
					+ type
					+ "'."
			);
		}

		// check result type
		if(
				!resultBool
				&& !resultSingle
				&& !resultMulti
				&& !resultSubSets
		) {
			return ServerCommandResponse::failed(
					"No result type selected."
			);
		}

		// check website
		ServerCommandResponse response;

		if(website > 0 && !Server::checkWebsite(this->database, website, response)) {
			return response;
		}

		// add query to database
		const auto id{
			this->database.addQuery(website, {
					name,
					query,
					type,
					resultBool,
					resultSingle,
					resultMulti,
					resultSubSets,
					textOnly
			})
		};

		if(id > 0) {
			return ServerCommandResponse("Query added.", id);
		}

		return ServerCommandResponse("Could not add query to database.");
	}

	// server command updatequery(id, name, query, type, resultbool, resultsingle, resultmulti, resultsubsets, textonly):
	//  edit a query
	Server::ServerCommandResponse Server::cmdUpdateQuery() {
		// get arguments
		std::uint64_t id{};
		std::string name;
		std::string query;
		std::string type;
		bool resultBool{false};
		bool resultSingle{false};
		bool resultMulti{false};
		bool resultSubSets{false};
		bool textOnly{false};
		std::string error;

		if(
				!(this->getArgument("id", id, error))
				|| !(this->getArgument("name", name, false, true, error))
				|| !(this->getArgument("query", query, false, true, error))
				|| !(this->getArgument("type", type, false, true, error))
				|| !(this->getArgument("resultbool", resultBool, false, error))
				|| !(this->getArgument("resultsingle", resultSingle, false, error))
				|| !(this->getArgument("resultmulti", resultMulti, false, error))
				|| !(this->getArgument("resultsubsets", resultSubSets, false, error))
				|| !(this->getArgument("textonly", textOnly, false, error))
		) {
			return ServerCommandResponse::failed(error);
		}

		if(
				type != "regex"
				&& type != "xpath"
				&& type != "jsonpointer"
				&& type != "jsonpath"
				&& type != "xpathjsonpointer"
				&& type != "xpathjsonpath"
		) {
			return ServerCommandResponse::failed(
					"Unknown query type: '" + type + "'."
			);
		}

		// check result type
		if(
				!resultBool
				&& !resultSingle
				&& !resultMulti
				&& !resultSubSets
		) {
			return ServerCommandResponse::failed(
					"No result type selected."
			);
		}

		// check query
		ServerCommandResponse response;

		if(!Server::checkQuery(this->database, id, response)) {
			return response;
		}

		// update query in database
		this->database.updateQuery(id, {
				name,
				query,
				type,
				resultBool,
				resultSingle,
				resultMulti,
				resultSubSets,
				textOnly
		});

		// if necessary, reset threads currently using this query
		std::ostringstream responseStream;
		bool addNewLine{true};

		responseStream << "Query updated.";

		for(auto& crawler : this->crawlers) {
			if(crawler->isQueryUsed(id)) {
				crawler->Module::Thread::reset();

				if(addNewLine) {
					responseStream << '\n';

					addNewLine = false;
				}

				responseStream << "\nCrawler #" << crawler->getId() << " will be reset.";
			}
		}

		for(auto& parser : this->parsers) {
			if(parser->isQueryUsed(id)) {
				parser->Module::Thread::reset();

				if(addNewLine) {
					responseStream << '\n';

					addNewLine = false;
				}

				responseStream << "\nParser #" << parser->getId() << " will be reset.";
			}
		}

		for(auto& extractor : this->extractors) {
			if(extractor->isQueryUsed(id)) {
				extractor->Module::Thread::reset();

				if(addNewLine) {
					responseStream << '\n';

					addNewLine = false;
				}

				responseStream << "\nExtractor #" << extractor->getId() << " will be reset.";
			}
		}

		for(auto& analyzer : this->analyzers) {
			if(analyzer->isQueryUsed(id)) {
				analyzer->Module::Thread::reset();

				if(addNewLine) {
					responseStream << '\n';

					addNewLine = false;
				}

				responseStream << "\nAnalyzer #" << analyzer->getId() << " will be reset.";
			}
		}

		return ServerCommandResponse(responseStream.str());
	}

	// server command movequery(id, to): move a query to another website by their IDs
	Server::ServerCommandResponse Server::cmdMoveQuery() {
		std::uint64_t id{};
		std::uint64_t to{};
		std::string error;

		// get arguments
		if(
				!(this->getArgument("id", id, error))
				|| !(this->getArgument("to", to, error))
		) {
			return ServerCommandResponse::failed(error);
		}

		// check arguments
		ServerCommandResponse response;

		if(
				!Server::checkQuery(this->database, id, response)
				|| (to > 0 && !Server::checkWebsite(this->database, to, response))
		) {
			return response;
		}

		// move query needs to be confirmed
		if(this->cmdJson.HasMember("confirmed")) {
			// move query to the other website
			this->database.moveQuery(id, to);

			return ServerCommandResponse("Query moved.");
		}

		return ServerCommandResponse::toBeConfirmed(
				"Do you really want to move this query?"
		);
	}

	// server command deletequery(id): delete a query from the database by its ID
	Server::ServerCommandResponse Server::cmdDeleteQuery() {
		// check whether the deletion of data is allowed
		if(!(this->settings.dataDeletable)) {
			return ServerCommandResponse::failed("Not allowed.");
		}

		// get argument
		std::uint64_t id{};
		std::string error;

		if(!(this->getArgument("id", id, error))) {
			return ServerCommandResponse::failed(error);
		}

		// check query
		ServerCommandResponse response;

		if(!Server::checkQuery(this->database, id, response)) {
			return response;
		}

		// deletequery needs to be confirmed
		if(this->cmdJson.HasMember("confirmed")) {
			// delete URL list from database
			this->database.deleteQuery(id);

			return ServerCommandResponse("Query deleted.");
		}

		return ServerCommandResponse::toBeConfirmed(
				"Do you really want to delete this query?"
		);
	}

	// server command duplicatequery(id): Duplicate a query by its ID
	Server::ServerCommandResponse Server::cmdDuplicateQuery() {
		// get argument
		std::uint64_t id{};
		std::string error;

		if(!(this->getArgument("id", id, error))) {
			return ServerCommandResponse::failed(error);
		}

		// check query
		ServerCommandResponse response;

		if(!Server::checkQuery(this->database, id, response)) {
			return response;
		}

		// duplicate query
		const auto newId{this->database.duplicateQuery(id)};

		if(newId == 0) {
			return ServerCommandResponse::failed(
					"Could not add duplicate to database."
			);
		}

		return ServerCommandResponse("Query duplicated.", newId);
	}

	// server command addconfig(website, module, name, config): Add configuration to database
	Server::ServerCommandResponse Server::cmdAddConfig() {
		// get arguments
		std::uint64_t website{};
		std::string module;
		std::string name;
		std::string config;
		std::string error;

		if(
				!(this->getArgument("website", website, error))
				|| !(this->getArgument("module", module, false, true, error))
				|| !(this->getArgument("name", name, false, true, error))
				|| !(this->getArgument("config", config, false, true, error))
		) {
			return ServerCommandResponse::failed(error);
		}

		// check configuration JSON
		rapidjson::Document configJson;

		try {
			configJson = Helper::Json::parseRapid(config);
		}
		catch(const JsonException& e) {
			return ServerCommandResponse::failed(
					"Could not parse JSON: "
					+ std::string(e.view())
					+ "."
			);
		}

		if(!configJson.IsArray()) {
			return ServerCommandResponse::failed(
					"Parsed JSON is not an array."
			);
		}

		// check analyzer configuration for algorithm
		if(
				module == "analyzer"
				&& Server::getAlgoFromConfig(configJson) == 0
		) {
				return ServerCommandResponse::failed(
						"No algorithm selected."
				);
		}

		// check website
		ServerCommandResponse response;

		if(!Server::checkWebsite(this->database, website, response)) {
			return response;
		}

		// add configuration to database
		const auto id{
			this->database.addConfiguration(
					website,
					{module, name, config}
			)
		};

		if(id == 0) {
			return ServerCommandResponse::failed(
					"Could not add configuration to database."
			);
		}

		return ServerCommandResponse("Configuration added.", id);
	}

	// server command updateconfig(id, name, config): Update a configuration in the database by its ID
	Server::ServerCommandResponse Server::cmdUpdateConfig() {
		// get arguments
		std::uint64_t id{};
		std::string name;
		std::string config;
		std::string error;

		if(
				!(this->getArgument("id", id, error))
				|| !(this->getArgument("name", name, false, true, error))
				|| !(this->getArgument("config", config, false, true, error))
		) {
			return ServerCommandResponse::failed(error);
		}

		// check configuration JSON
		rapidjson::Document configJson;

		try {
			configJson = Helper::Json::parseRapid(config);
		}
		catch(const JsonException& e) {
			return ServerCommandResponse::failed("Could not parse JSON.");
		}

		if(!configJson.IsArray()) {
			return ServerCommandResponse::failed("Parsed JSON is not an array.");
		}

		// check configuration
		ServerCommandResponse response;

		if(!(this->checkConfig(id, response))) {
			return response;
		}

		// update configuration in database
		this->database.updateConfiguration(id, {{}, name, config});

		// if necessary, reset threads currently using this configuration
		std::ostringstream responseStream;
		bool addNewLine{true};

		responseStream << "Configuration updated.";

		for(auto& crawler : this->crawlers) {
			if(crawler->getConfig() == id) {
				crawler->Module::Thread::reset();

				if(addNewLine) {
					responseStream << '\n';

					addNewLine = false;
				}

				responseStream << "\nCrawler #" << crawler->getId() << " will be reset.";
			}
		}

		for(auto& parser : this->parsers) {
			if(parser->getConfig() == id) {
				parser->Module::Thread::reset();

				if(addNewLine) {
					responseStream << '\n';

					addNewLine = false;
				}

				responseStream << "\nParser #" << parser->getId() << " will be reset.";
			}
		}

		for(auto& extractor : this->extractors) {
			if(extractor->getConfig() == id) {
				extractor->Module::Thread::reset();

				if(addNewLine) {
					responseStream << '\n';

					addNewLine = false;
				}

				responseStream << "\nExtractor #" << extractor->getId() << " will be reset.";
			}
		}

		for(auto& analyzer : this->analyzers) {
			if(analyzer->getConfig() == id) {
				analyzer->Module::Thread::reset();

				if(addNewLine) {
					responseStream << '\n';

					addNewLine = false;
				}

				responseStream << "\nAnalyzer #" << analyzer->getId() << " will be reset.";
			}
		}

		return ServerCommandResponse(responseStream.str());
	}

	// server command deleteconfig(id): delete a configuration from the database by its ID
	Server::ServerCommandResponse Server::cmdDeleteConfig() {
		// check whether the deletion of data is allowed
		if(!(this->settings.dataDeletable)) {
			return ServerCommandResponse::failed("Not allowed.");
		}

		// get argument
		std::uint64_t id{};
		std::string error;

		if(!(this->getArgument("id", id, error))) {
			return ServerCommandResponse::failed(error);
		}

		// check configuration
		ServerCommandResponse response;

		if(!(this->checkConfig(id, response))) {
			return response;
		}

		// deleteconfig needs to be confirmed
		if(this->cmdJson.HasMember("confirmed")) {
			// delete configuration from database
			this->database.deleteConfiguration(id);

			return ServerCommandResponse("Configuration deleted.");
		}

		return ServerCommandResponse::toBeConfirmed(
				"Do you really want to delete this configuration?"
		);
	}

	// server command duplicateconfig(id): Duplicate a configuration by its ID
	Server::ServerCommandResponse Server::cmdDuplicateConfig() {
		// get argument
		std::uint64_t id{};
		std::string error;

		if(!(this->getArgument("id", id, error))) {
			return ServerCommandResponse::failed(error);
		}

		// check configuration
		ServerCommandResponse response;

		if(!(this->checkConfig(id, response))) {
			return response;
		}

		// duplicate configuration
		const auto newId{this->database.duplicateConfiguration(id)};

		if(newId == 0) {
			return ServerCommandResponse::failed(
					"Could not add duplicate to database."
			);
		}

		return ServerCommandResponse("Configuration duplicated.", newId);
	}

	// server command listdicts: Retrieve list of dictionaries available to the server
	Server::ServerCommandResponse Server::cmdListDicts() {
		auto dictionaries{
			Helper::FileSystem::listFilesInPath(
					dictDir,
					""
			)
		};

		// remove directory from file names
		for(auto& dictionary : dictionaries) {
			dictionary = dictionary.substr(dictDir.length() + 1);
		}

		return ServerCommandResponse(Helper::Json::stringify(dictionaries));
	}

	// server command listmdls: Retrieve list of language models available to the server
	Server::ServerCommandResponse Server::cmdListMdls() {
		auto models{
			Helper::FileSystem::listFilesInPath(
					mdlDir,
					""
			)
		};

		// remove directory from file names
		for(auto& model : models) {
			model = model.substr(mdlDir.length() + 1);
		}

		return ServerCommandResponse(Helper::Json::stringify(models));
	}

	// server command warp(thread, target): Let thread jump to specified ID
	Server::ServerCommandResponse Server::cmdWarp() {
		// get argument
		std::uint64_t thread{};
		std::uint64_t target{};
		std::string error;

		if(
				!(this->getArgument("thread", thread, error))
				|| !(this->getArgument("target", target, error))
		) {
			return ServerCommandResponse::failed(error);
		}

		// find thread
		auto c{
			std::find_if(this->crawlers.cbegin(), this->crawlers.cend(),
					[&thread](const auto& p) {
						return p->Module::Thread::getId() == thread;
					}
			)
		};

		if(c != this->crawlers.cend()) {
			(*c)->Module::Thread::warpTo(target);

			return ServerCommandResponse(
					"Crawler #"
					+ std::to_string(thread)
					+ " will warp to #"
					+ std::to_string(target)
					+ "."
			);
		}

		auto p{
			std::find_if(this->parsers.cbegin(), this->parsers.cend(),
					[&thread](const auto& p) {
						return p->Module::Thread::getId() == thread;
					}
			)
		};

		if(p != this->parsers.cend()) {
			(*p)->Module::Thread::warpTo(target);

			return ServerCommandResponse(
					"Parser #"
					+ std::to_string(thread)
					+ " will warp to #"
					+ std::to_string(target)
					+ "."
			);
		}

		auto e{
			std::find_if(this->extractors.cbegin(), this->extractors.cend(),
					[&thread](const auto& p) {
						return p->Module::Thread::getId() == thread;
					}
			)
		};

		if(e != this->extractors.cend()) {
			(*e)->Module::Thread::warpTo(target);

			return ServerCommandResponse(
					"Extractor #"
					+ std::to_string(thread)
					+ " will warp to #"
					+ std::to_string(target)
					+ "."
			);
		}

		auto a{
			std::find_if(this->analyzers.cbegin(), this->analyzers.cend(),
					[&thread](const auto& p) {
						return p->Module::Thread::getId() == thread;
					}
			)
		};

		if(a != this->analyzers.cend()) {
			return ServerCommandResponse::failed(
					"Time travel is not supported for analyzers."
			);
		}

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
		std::string fileName;
		std::string error;

		if(!(this->getArgument("filename", fileName, false, true, error))) {
			return ServerCommandResponse::failed(error);
		}

		return ServerCommandResponse(fileName);
	}

	// server command import(datatype, filetype, compression, filename, [...]): import data from a file into the database
	void Server::cmdImport(
			ConnectionPtr connection,
			std::size_t threadIndex,
			const std::string& message
	) {
		DatabaseSettings dbSettingsCopy(this->dbSettings);
		ServerCommandResponse response;
		rapidjson::Document json;

		// begin of worker thread
		MAIN_SERVER_WORKER_BEGIN

		if(Server::workerBegin(message, json, response)) {
			// get arguments
			std::string dataType;
			std::string fileType;
			std::string compression;
			std::string fileName;
			std::string error;

			if(
					!Server::getArgument(json, "datatype", dataType, false, true, error)
					|| !Server::getArgument(json, "filetype", fileType, false, true, error)
					|| !Server::getArgument(json, "compression", compression, false, true, error)
					|| !Server::getArgument(json, "filename", fileName, false, true, error)
			) {
				response = ServerCommandResponse::failed(error);
			}
			else {
				// start timer
				Timer::Simple timer;

				// generate full file name to import from
				std::string fullFileName;

				fullFileName.reserve(cacheDir.length() + fileName.length() + 1);

				fullFileName = cacheDir;

				fullFileName += Helper::FileSystem::getPathSeparator();
				fullFileName += fileName;

				std::string content;

				// check file name and whether file exists
				if(Helper::FileSystem::contains(cacheDir, fullFileName)) {
					if(Helper::FileSystem::isValidFile(fullFileName)) {
						content = Data::File::read(fullFileName, true);

						if(compression != "none") {
							if(compression == "gzip") {
								content = Data::Compression::Gzip::decompress(content);
							}
							else if(compression == "zlib") {
								content = Data::Compression::Zlib::decompress(content);
							}
							else {
								response = ServerCommandResponse::failed(
										"Unknown compression type: '"
										+ compression
										+ "'."
								);
							}
						}
					}
					else {
						response = ServerCommandResponse::failed(
								"File does not exist: '"
								+ fileName
								+ "'."
						);
					}
				}
				else {
					response = ServerCommandResponse::failed(
							"Invalid file name: '"
							+ fileName
							+ "'."
					);
				}

				if(!response.fail) {
					if(dataType == "urllist") {
						// get arguments for importing a URL list
						std::uint64_t website{};
						std::uint64_t target{};

						if(
								!Server::getArgument(json, "website", website, error)
								|| !Server::getArgument(json, "urllist-target", target, error)
						) {
							response = ServerCommandResponse::failed(error);
						}
						else {
							// import URL list
							std::queue<std::string> urls;

							if(fileType == "text") {
								// import URL list from text file
								bool hasHeader{false};

								if(!Server::getArgument(
										json,
										"is-firstline-header",
										hasHeader,
										false,
										error)
								) {
									response = ServerCommandResponse::failed(error);
								}
								else {
									urls = Data::ImportExport::Text::importList(
											content,
											hasHeader,
											true
									);
								}
							}
							else {
								response = ServerCommandResponse::failed(
										"Unknown file type: '"
										+ fileType
										+ "'."
								);
							}

							if(!response.fail) {
								// create new database connection for worker thread
								Module::Database db(dbSettingsCopy, "worker");

								Server::initWorkerDatabase(db);

								// check website
								if(Server::checkWebsite(db, website, response)) {
									// check URL list
									if(target > 0) {
										Server::checkUrlList(db, website, target, response);
									}
									else {
										// get arguments for URL list creation
										std::string urlListNameSpace;
										std::string urlListName;

										if(
												!Server::getArgument(
														json,
														"urllist-namespace",
														urlListNameSpace,
														false,
														true,
														error
												) || !Server::getArgument(
														json,
														"urllist-name",
														urlListName,
														false,
														true,
														error
												)
										) {
											response = ServerCommandResponse::failed(error);
										}
										else {
											// add new URL list
											target = db.addUrlList(
													website,
													{urlListNameSpace, urlListName}
											);
										}
									}

									if(!response.fail) {
										std::size_t added{};

										if(!urls.empty()) {
											// write to log
											std::ostringstream logStrStr;

											logStrStr << "importing ";

											if(urls.size() == 1) {
												logStrStr << "one URL";
											}
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
											responseStrStr << "Added no new URL";
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
					else {
						response = ServerCommandResponse::failed(
								"Unknown data type: '"
								+ dataType
								+ "'."
						);
					}
				}
			}
		}

		// end of worker thread
		MAIN_SERVER_WORKER_END(response)

		this->workerEnd(threadIndex, connection, message, response);
	}

	// server command merge(datatype, [...]): merge two tables in the database
	void Server::cmdMerge(
			ConnectionPtr connection,
			std::size_t threadIndex,
			const std::string& message
	) {
		DatabaseSettings dbSettingsCopy(this->dbSettings);
		ServerCommandResponse response;
		rapidjson::Document json;

		// begin of worker thread
		MAIN_SERVER_WORKER_BEGIN

		if(Server::workerBegin(message, json, response)) {
			// get arguments
			std::string dataType;
			std::string error;

			if(!Server::getArgument(json, "datatype", dataType, false, true, error)) {
				response = ServerCommandResponse::failed(error);
			}
			else if(dataType == "urllist") {
				// get arguments for merging two URL lists
				std::uint64_t website{};
				std::uint64_t source{};
				std::uint64_t target{};

				if(
						!Server::getArgument(json, "website", website, error)
						|| !Server::getArgument(json, "urllist-source", source, error)
						|| !Server::getArgument(json, "urllist-target", target, error)
				) {
					response = ServerCommandResponse::failed(error);
				}
				else {
					// merge URL lists
					if(source == target) {
						response = ServerCommandResponse::failed(
								"A URL list cannot be merged with itself."
						);
					}
					else {
						// create new database connection for worker thread
						Module::Database db(dbSettingsCopy, "worker");

						Server::initWorkerDatabase(db);

						// check website and URL lists
						if(
								Server::checkWebsite(db, website, response)
								&& Server::checkUrlList(db, website, source, response)
								&& Server::checkUrlList(db, website, target, response)
						) {
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
							const auto added{db.mergeUrls(target, urls)};
							const std::string timerStr{timer.tickStr()};

							logStrStr << "completed (added ";

							switch(added) {
							case 0:
								response = ServerCommandResponse(
										"No new URL added after "
										+ timerStr
										+ "."
								);

								logStrStr << "no new URL";

								break;

							case 1:
								response = ServerCommandResponse(
										"One new URL added after "
										+ timerStr
										+ "."
								);

								logStrStr << "one new URL";

								break;

							default:
								std::ostringstream responseStrStr;

								responseStrStr.imbue(std::locale(""));

								responseStrStr
										<< added
										<< " new URLs added after "
										<< timerStr
										<< ".";

								response = ServerCommandResponse(responseStrStr.str());

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

		// end of worker thread
		MAIN_SERVER_WORKER_END(response)

		this->workerEnd(threadIndex, connection, message, response);
	}

	// server command export(datatype, filetype, compression, [...]): export data from the database into a file
	void Server::cmdExport(
			ConnectionPtr connection,
			std::size_t threadIndex,
			const std::string& message
	) {
		DatabaseSettings dbSettingsCopy(this->dbSettings);
		ServerCommandResponse response;
		rapidjson::Document json;

		// begin of worker thread
		MAIN_SERVER_WORKER_BEGIN

		if(Server::workerBegin(message, json, response)) {
			// get arguments
			std::string dataType;
			std::string fileType;
			std::string compression;

			if(
					Server::cmdExportGetArguments(
							json,
							dataType,
							fileType,
							compression,
							response
					)
			) {
				std::string content;

				// create new database connection for worker thread
				Module::Database db(dbSettingsCopy, "worker");

				Server::initWorkerDatabase(db);

				// start timer
				Timer::Simple timer;

				// retrieve data from the database and convert it for the file type
				if(
						Server::cmdExportRetrieveAndConvert(
								json,
								dataType,
								fileType,
								db,
								content,
								response
						)
						&& Server::cmdExportCompress(
								dataType,
								compression,
								content,
								response
						)
				) {
					// write data to file and log success
					Server::cmdExportWrite(content, response);
					Server::cmdExportLogSuccess(db, content.size(), timer.tickStr());
				}
			}
		}

		// end of worker thread
		MAIN_SERVER_WORKER_END(response)

		this->workerEnd(threadIndex, connection, message, response);
	}

	// server command deleteurls(urllist,query): delete all URLs from the specified URL list that match the specified query
	void Server::cmdDeleteUrls(
			ConnectionPtr connection,
			std::size_t threadIndex,
			const std::string& message
	) {
		DatabaseSettings dbSettingsCopy(this->dbSettings);
		ServerCommandResponse response;
		rapidjson::Document json;

		// begin of worker thread
		MAIN_SERVER_WORKER_BEGIN

		if(Server::workerBegin(message, json, response)) {
			// get arguments
			std::uint64_t urlList{};
			std::uint64_t query{};

			if(
					Server::cmdDeleteUrlsGetArguments(
							json,
							urlList,
							query,
							response
					)
			) {
				// create new database connection for worker thread
				Module::Database db(dbSettingsCopy, "worker");

				Server::initWorkerDatabase(db);

				// check arguments
				std::uint64_t website{};
				std::string regEx;
				std::queue<std::uint64_t> toDelete;

				if(
						Server::checkUrlList(db, urlList, response)
						&& Server::cmdDeleteUrlsGetWebsite(db, urlList, website, response)
						&& Server::checkQuery(db, website, query, response)
						&& Server::cmdDeleteUrlsGetQuery(db, query, regEx, response)
						&& Server::cmdDeleteUrlsGetUrls(db, urlList, regEx, toDelete, response)
				) {
					// deleteurls needs to be confirmed
					if(json.HasMember("confirmed")) {
						response = Server::cmdDeleteUrlsDelete(db, urlList, toDelete);
					}
					else {
						response = Server::cmdDeleteUrlsConfirm(toDelete.size());
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
	void Server::cmdTestQuery(
			ConnectionPtr connection,
			std::size_t threadIndex,
			const std::string& message
	) {
		ServerCommandResponse response;
		rapidjson::Document json;

		// begin of worker thread
		MAIN_SERVER_WORKER_BEGIN

		if(Server::workerBegin(message, json, response)) {
			// get arguments
			std::string query;
			std::string type;
			bool resultBool{false};
			bool resultSingle{false};
			bool resultMulti{false};
			bool resultSubSets{false};
			bool textOnly{false};
			std::string text;
			bool xmlWarnings{false};
			bool fixCData{true};
			bool fixComments{true};
			bool removeXml{false};
			std::string dateTimeFormat;
			std::string dateTimeLocale;
			std::string error;

			if(
					!Server::getArgument(json, "query", query, false, true, error)
					|| !Server::getArgument(json, "type", type, false, true, error)
					|| !Server::getArgument(json, "resultbool", resultBool, false, error)
					|| !Server::getArgument(json, "resultsingle", resultSingle, false, error)
					|| !Server::getArgument(json, "resultmulti", resultMulti, false, error)
					|| !Server::getArgument(json, "resultsubsets", resultSubSets, false, error)
					|| !Server::getArgument(json, "textonly", textOnly, false, error)
					|| !Server::getArgument(json, "text", text, false, false, error)
					|| !Server::getArgument(json, "xmlwarnings", xmlWarnings, false, error)
					|| !Server::getArgument(json, "fixcdata", fixCData, false, error)
					|| !Server::getArgument(json, "fixcomments", fixComments, false, error)
					|| !Server::getArgument(json, "removexml", removeXml, false, error)
					|| !Server::getArgument(json, "datetimeformat", dateTimeFormat, true, false, error)
					|| !Server::getArgument(json, "datetimelocale", dateTimeLocale, true, false, error)
			) {
				response = ServerCommandResponse::failed(error);
			}
			else {
				// check result type
				if(
						type != "regex"
						&& type != "xpath"
						&& type != "jsonpointer"
						&& type != "jsonpath"
						&& type != "xpathjsonpointer"
						&& type != "xpathjsonpath"
				) {
					response = ServerCommandResponse::failed(
							"Unknown query type: '"
							+ type
							+ "'."
					);
				}
				else if(
						!resultBool
						&& !resultSingle
						&& !resultMulti
						&& !resultSubSets
				) {
					response = ServerCommandResponse::failed(
							"No result type selected."
					);
				}
				else {
					// test query
					std::string result;

					if(type == "regex") {
						// test RegEx expression on text
						try {
							Timer::SimpleHR timer;

							const Query::RegEx regExTest(
									query,
									resultBool || resultSingle,
									resultMulti
							);

							result = "COMPILING TIME: " + timer.tickStr() + '\n';

							if(resultBool) {
								// get boolean result (does at least one match exist?)
								result	+= "BOOLEAN RESULT ("
										+ timer.tickStr()
										+ "): "
										+ (regExTest.getBool(text) ? "true" : "false")
										+ '\n';
							}

							if(resultSingle) {
								// get first result (first full match)
								std::string tempResult;

								regExTest.getFirst(text, tempResult);

								result += "FIRST RESULT (" + timer.tickStr() + "): ";

								if(tempResult.empty()) {
									result += "[empty]";
								}
								else {
									result += tempResult
											+ dateTimeTest(
													tempResult,
													dateTimeFormat,
													dateTimeLocale
											);
								}

								result.push_back('\n');
							}

							if(resultMulti || resultSubSets) {
								// get all results (all full matches)
								std::vector<std::string> tempResults;

								regExTest.getAll(text, tempResults);

								result += "ALL RESULTS (" + timer.tickStr() + "):";

								if(tempResults.empty()) {
									result += " [empty]\n";
								}
								else {
									std::size_t counter{};
									std::string toAppend(1, '\n');

									for(const auto& tempResult : tempResults) {
										++counter;

										toAppend	+= '['
													+ std::to_string(counter)
													+ "] "
													+ tempResult
													+ dateTimeTest(
															tempResult,
															dateTimeFormat,
															dateTimeLocale
													) + "\n";
									}

									result += toAppend;
								}
							}
						}
						catch(const RegExException& e) {
							response = ServerCommandResponse::failed(
									"RegEx error: "
									+ std::string(e.view())
							);
						}
					}
					else if(type == "xpath") {
						// test XPath expression on text
						try {
							Timer::SimpleHR timer;
							std::queue<std::string> warnings;
							const Query::XPath xPathTest(
									query,
									textOnly
							);

							result = "COMPILING TIME: " + timer.tickStr() + '\n';

							Parsing::XML xmlDocumentTest;

							xmlDocumentTest.setOptions(xmlWarnings, xmlWarningsDefault);
							xmlDocumentTest.parse(text, fixCData, fixComments, removeXml, warnings);

							while(!warnings.empty()) {
								result += "WARNING: " + warnings.front() + '\n';

								warnings.pop();
							}

							result += "PARSING TIME: " + timer.tickStr() + '\n';

							if(resultBool) {
								// get boolean result (does at least one match exist?)
								result	+= "BOOLEAN RESULT ("
										+ timer.tickStr()
										+ "): "
										+ (xPathTest.getBool(xmlDocumentTest) ? "true" : "false")
										+ '\n';
							}

							if(resultSingle) {
								// get first result (first full match)
								std::string tempResult;

								xPathTest.getFirst(xmlDocumentTest, tempResult);

								result += "FIRST RESULT (" + timer.tickStr() + "): ";

								if(tempResult.empty()) {
									result += "[empty]";
								}
								else {
									result += tempResult
											+ dateTimeTest(
													tempResult,
													dateTimeFormat,
													dateTimeLocale
											);
								}

								result.push_back('\n');
							}

							if(resultMulti) {
								// get all results (all full matches)
								std::vector<std::string> tempResults;

								xPathTest.getAll(xmlDocumentTest, tempResults);

								result += "ALL RESULTS (" + timer.tickStr() + "):";

								if(tempResults.empty()) {
									result += " [empty]\n";
								}
								else {
									std::size_t counter{};
									std::string toAppend(1, '\n');

									for(const auto& tempResult : tempResults) {
										++counter;

										toAppend	+= '['
													+ std::to_string(counter)
													+ "] "
													+ tempResult
													+ dateTimeTest(
															tempResult,
															dateTimeFormat,
															dateTimeLocale
													) + '\n';
									}

									result += toAppend;
								}
							}

							if(resultSubSets) {
								// get subsets
								std::vector<Parsing::XML> tempResults;

								xPathTest.getSubSets(xmlDocumentTest, tempResults);

								result += "SUBSETS (" + timer.tickStr() + "):";

								if(tempResults.empty()) {
									result += " [empty]\n";
								}
								else {
									std::size_t counter{};
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
							response = ServerCommandResponse::failed(
									"XPath error - "
									+ std::string(e.view())
							);
						}
						catch(const XMLException& e) {
							response = ServerCommandResponse::failed(
									"XML error: "
									+ std::string(e.view())
							);
						}
					}
					else if(type == "jsonpointer") {
						// test JSONPointer query on text
						try {
							Timer::SimpleHR timer;

							const Query::JsonPointer JSONPointerTest(
									query,
									textOnly
							);

							result = "COMPILING TIME: " + timer.tickStr() + '\n';

							rapidjson::Document jsonDocumentTest;

							jsonDocumentTest = Helper::Json::parseRapid(text);

							result += "PARSING TIME: " + timer.tickStr() + '\n';

							if(resultBool) {
								// get boolean result (does at least one match exist?)
								result	+= "BOOLEAN RESULT ("
										+ timer.tickStr() + "): "
										+ (JSONPointerTest.getBool(jsonDocumentTest) ? "true" : "false")
										+ '\n';
							}

							if(resultSingle) {
								// get first result (first full match)
								std::string tempResult;

								JSONPointerTest.getFirst(jsonDocumentTest, tempResult);

								result += "FIRST RESULT (" + timer.tickStr() + "): ";

								if(tempResult.empty()) {
									result += "[empty]";
								}
								else {
									result += tempResult
											+ dateTimeTest(
													tempResult,
													dateTimeFormat,
													dateTimeLocale
											);
								}

								result.push_back('\n');
							}

							if(resultMulti) {
								// get all results (all full matches)
								std::vector<std::string> tempResults;

								JSONPointerTest.getAll(jsonDocumentTest, tempResults);

								result += "ALL RESULTS (" + timer.tickStr() + "):";

								if(tempResults.empty()) {
									result += " [empty]\n";
								}
								else {
									std::size_t counter{};
									std::string toAppend(1, '\n');

									for(const auto& tempResult : tempResults) {
										++counter;

										toAppend	+= '['
													+ std::to_string(counter)
													+ "] "
													+ tempResult
													+ dateTimeTest(
															tempResult,
															dateTimeFormat,
															dateTimeLocale
													) + '\n';
									}

									result += toAppend;
								}
							}

							if(resultSubSets) {
								// get subsets
								std::vector<rapidjson::Document> tempResults;

								JSONPointerTest.getSubSets(jsonDocumentTest, tempResults);

								result += "SUBSETS (" + timer.tickStr() + "):";

								if(tempResults.empty()) {
									result += " [empty]\n";
								}
								else {
									std::size_t counter{};
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
							response = ServerCommandResponse::failed(
									"JSONPointer error: "
									+ std::string(e.view())
							);
						}
						catch(const JsonException& e) {
							response = ServerCommandResponse::failed(
									"Could not parse JSON: "
									+ std::string(e.view()) + "."
							);
						}
					}
					else if(type == "jsonpath") {
						// test JSONPath query on text
						try {
							Timer::SimpleHR timer;

							const Query::JsonPath JSONPathTest(
									query,
									textOnly
							);

							jsoncons::json jsonTest;

							jsonTest = Helper::Json::parseCons(text);

							result += "PARSING TIME: " + timer.tickStr() + '\n';

							if(resultBool) {
								// get boolean result (does at least one match exist?)
								result	+= "BOOLEAN RESULT ("
										+ timer.tickStr()
										+ "): "
										+ (JSONPathTest.getBool(jsonTest) ? "true" : "false")
										+ '\n';
							}

							if(resultSingle) {
								// get first result (first full match)
								std::string tempResult;

								JSONPathTest.getFirst(jsonTest, tempResult);

								result += "FIRST RESULT (" + timer.tickStr() + "): ";

								if(tempResult.empty()) {
									result += "[empty]";
								}
								else {
									result += tempResult
											+ dateTimeTest(
													tempResult,
													dateTimeFormat,
													dateTimeLocale
											);
								}

								result.push_back('\n');
							}

							if(resultMulti) {
								// get all results (all full matches)
								std::vector<std::string> tempResults;

								JSONPathTest.getAll(jsonTest, tempResults);

								result += "ALL RESULTS (" + timer.tickStr() + "):";

								if(tempResults.empty()) {
									result += " [empty]\n";
								}
								else {
									std::size_t counter{};
									std::string toAppend(1, '\n');

									for(const auto& tempResult : tempResults) {
										++counter;

										toAppend	+= '['
													+ std::to_string(counter)
													+ "] "
													+ tempResult
													+ dateTimeTest(
															tempResult,
															dateTimeFormat,
															dateTimeLocale
													)
													+ '\n';
									}

									result += toAppend;
								}
							}

							if(resultSubSets) {
								// get subsets
								std::vector<jsoncons::json> tempResults;

								JSONPathTest.getSubSets(jsonTest, tempResults);

								result += "SUBSETS (" + timer.tickStr() + "):";

								if(tempResults.empty()) {
									result += " [empty]\n";
								}
								else {
									std::size_t counter{};
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
							response = ServerCommandResponse::failed(
									"JSONPath error: "
									+ std::string(e.view())
							);
						}
						catch(const JsonException& e) {
							response = ServerCommandResponse::failed(
									"Could not parse JSON: "
									+ std::string(e.view())
									+ "."
							);
						}
					}
					else { // test combined query (XPath + JSONPointer/JSONPath) on text
						// show performance warning
						result =	"NOTE: When using combined queries,"
									" the JSON needs to be parsed every time the query is used."
									"\n\n";

						// split XPath query (first line) from JSON query
						const auto splitPos{query.find('\n')};
						const std::string xPathQuery(
								query,
								0,
								splitPos
						);
						std::string jsonQuery;

						if(splitPos != std::string::npos && query.size() > splitPos + 1) {
							jsonQuery = query.substr(splitPos + 1);
						}

						result += "using XPath query '"
								+ xPathQuery
								+ "'\nusing JSON query '"
								+ jsonQuery
								+ "'\n\n";

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

							xmlDocumentTest.setOptions(xmlWarnings, xmlWarningsDefault);
							xmlDocumentTest.parse(text, fixCData, fixComments, removeXml, warnings);

							while(!warnings.empty()) {
								result += "WARNING: " + warnings.front() + '\n';

								warnings.pop();
							}

							result += "HTML/XML PARSING TIME: " + timer.tickStr() + '\n';

							// get first result from XPath (first full match)
							std::string xpathResult;

							xPathTest.getFirst(xmlDocumentTest, xpathResult);

							result += "XPATH RESULT (" + timer.tickStr() + "): ";

							if(xpathResult.empty()) {
								result += "[empty]\n";
							}
							else {
								result += xpathResult + '\n';

								if(type == "xpathjsonpointer") {
									// test JSONPointer query on XPath result
									Timer::SimpleHR timer;

									const Query::JsonPointer JSONPointerTest(
											jsonQuery,
											textOnly
									);

									result += "JSONPOINTER COMPILING TIME: " + timer.tickStr() + '\n';

									rapidjson::Document jsonDocumentTest;

									jsonDocumentTest = Helper::Json::parseRapid(xpathResult);

									result += "JSON PARSING TIME: " + timer.tickStr() + '\n';

									if(resultBool) {
										// get boolean result (does at least one match exist?)
										result	+= "BOOLEAN RESULT ("
												+ timer.tickStr() + "): "
												+ (JSONPointerTest.getBool(jsonDocumentTest) ? "true" : "false")
												+ '\n';
									}

									if(resultSingle) {
										// get first result (first full match)
										std::string tempResult;

										JSONPointerTest.getFirst(jsonDocumentTest, tempResult);

										result += "FIRST RESULT (" + timer.tickStr() + "): ";

										if(tempResult.empty()) {
											result += "[empty]";
										}
										else {
											result += tempResult
													+ dateTimeTest(
															tempResult,
															dateTimeFormat,
															dateTimeLocale
													);
										}

										result.push_back('\n');
									}

									if(resultMulti) {
										// get all results (all full matches)
										std::vector<std::string> tempResults;

										JSONPointerTest.getAll(jsonDocumentTest, tempResults);

										result += "ALL RESULTS (" + timer.tickStr() + "):";

										if(tempResults.empty()) {
											result += " [empty]\n";
										}
										else {
											std::size_t counter{};
											std::string toAppend(1, '\n');

											for(const auto& tempResult : tempResults) {
												++counter;

												toAppend	+= '['
															+ std::to_string(counter)
															+ "] "
															+ tempResult
															+ dateTimeTest(
																	tempResult,
																	dateTimeFormat,
																	dateTimeLocale
															) + '\n';
											}

											result += toAppend;
										}
									}

									if(resultSubSets) {
										// get subsets
										std::vector<rapidjson::Document> tempResults;

										JSONPointerTest.getSubSets(jsonDocumentTest, tempResults);

										result += "SUBSETS (" + timer.tickStr() + "):";

										if(tempResults.empty()) {
											result += " [empty]\n";
										}
										else {
											std::size_t counter{};
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
											textOnly
									);

									jsoncons::json jsonTest;

									jsonTest = Helper::Json::parseCons(xpathResult);

									result += "JSON PARSING TIME: " + timer.tickStr() + '\n';

									if(resultBool) {
										// get boolean result (does at least one match exist?)
										result	+= "BOOLEAN RESULT ("
												+ timer.tickStr()
												+ "): "
												+ (JSONPathTest.getBool(jsonTest) ? "true" : "false")
												+ '\n';
									}

									if(resultSingle) {
										// get first result (first full match)
										std::string tempResult;

										JSONPathTest.getFirst(jsonTest, tempResult);

										result += "FIRST RESULT (" + timer.tickStr() + "): ";

										if(tempResult.empty()) {
											result += "[empty]";
										}
										else {
											result += tempResult
													+ dateTimeTest(
															tempResult,
															dateTimeFormat,
															dateTimeLocale
													);
										}

										result.push_back('\n');
									}

									if(resultMulti) {
										// get all results (all full matches)
										std::vector<std::string> tempResults;

										JSONPathTest.getAll(jsonTest, tempResults);

										result += "ALL RESULTS (" + timer.tickStr() + "):";

										if(tempResults.empty()) {
											result += " [empty]\n";
										}
										else {
											std::size_t counter{};
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

									if(resultSubSets) {
										// get subsets
										std::vector<jsoncons::json> tempResults;

										JSONPathTest.getSubSets(jsonTest, tempResults);

										result += "SUBSETS (" + timer.tickStr() + "):";

										if(tempResults.empty()) {
											result += " [empty]\n";
										}
										else {
											std::size_t counter{};
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
							response = ServerCommandResponse::failed(
									"XPath error - "
									+ std::string(e.view())
							);
						}
						catch(const XMLException& e) {
							response = ServerCommandResponse::failed(
									"Could not parse HTML/XML: "
									+ std::string(e.view())
							);
						}
						catch(const JSONPointerException& e) {
							response = ServerCommandResponse::failed(
									"JSONPointer error: "
									+ std::string(e.view())
							);
						}
						catch(const JSONPathException& e) {
							response = ServerCommandResponse::failed(
									"JSONPath error: "
									+ std::string(e.view())
							);
						}
						catch(const JsonException& e) {
							response = ServerCommandResponse::failed(
									"Could not parse JSON: "
									+ std::string(e.view())
							);
						}
					}

					if(!response.fail) {
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

	/*
	 * SERVER INITIALIZATION (private)
	 */

	// clear or delete cache directory
	void Server::initCacheDir() {
		if(Helper::FileSystem::isValidDirectory(cacheDir)) {
			Helper::FileSystem::clearDirectory(cacheDir);
		}
		else {
			Helper::FileSystem::createDirectory(cacheDir);
		}
	}

	// create directory if it does not exist
	void Server::initDir(std::string_view directory) {
		Helper::FileSystem::createDirectoryIfNotExists(directory);
	}

	// create and save debug directory if needed
	void Server::initDebuggingDir(bool isEnabled, std::string_view directory) {
		if(isEnabled) {
			Helper::FileSystem::createDirectoryIfNotExists(directory);
		}
	}

	// initialize database connection for the server
	void Server::initDatabase(std::uint16_t sleepOnSqlErrorS) {
		// set database option (for server only)
		this->database.setSleepOnError(sleepOnSqlErrorS);

		// connect to database and initialize it
		this->database.connect();
		this->database.initializeSql();
		this->database.prepare();
		this->database.update();

		// change state to online
		this->offline = false;
	}

	// set callbacks
	void Server::initCallbacks() {
		this->webServer.setAcceptCallback([this](ConnectionPtr connectionPtr) {
			return this->onAccept(connectionPtr);
		});

		this->webServer.setLogCallback(
				[this](const std::string& entry) {
			return this->database.log(entry);
		});

		this->webServer.setRequestCallback(
				[this](
						ConnectionPtr connection,
						const std::string& method,
						const std::string& body,
						void * data
				) {
					return this->onRequest(connection, method, body, data);
				}
		);
	}

	// initialize mongoose embedded web server, bind it to port and set CORS string
	void Server::initWebServer(const std::string& port, const std::string& corsOrigins) {
		this->webServer.initHTTP(port);

		this->webServer.setCORS(corsOrigins);

		// set initial status
		this->setStatus("crawlserv++ is ready");
	}

	// load threads from database
	void Server::initThreads() {
		for(const auto& thread : this->database.getThreads()) {
			if(thread.options.module == "crawler") {
#ifndef MAIN_SERVER_DEBUG_NOCRAWLERS
				// continue crawler thread
				this->continueModuleThread(thread, this->crawlers);
#endif
			}
			else if(thread.options.module == "parser") {
#ifndef MAIN_SERVER_DEBUG_NOPARSERS
				// continue parser thread
				this->continueParserThread(thread);
#endif
			}
#ifndef MAIN_SERVER_DEBUG_NOEXTRACTORS
			else if(thread.options.module == "extractor") {
				// continue extractor thread
				this->continueModuleThread(thread, this->extractors);
#endif
			}
			else if(thread.options.module == "analyzer") {
#ifndef MAIN_SERVER_DEBUG_NOANALYZERS
				// continue analyzer thread
				this->continueAnalyzerThread(thread);
#endif
			}
			else {
				throw Exception(
						"Unknown thread module '"
						+ thread.options.module
						+ "'"
				);
			}
		}
	}

	// start logging
	void Server::initStartLogging() {
		this->database.log(
				"successfully started and connected to database [MySQL v"
				+ this->database.getMysqlVersion()
				+ "; datadir='"
				+ this->database.getDataDir()
				+ "'; maxAllowedPacketSize="
				+ std::to_string(this->database.getMaxAllowedPacketSize())
				+ "; connection_id="
				+ std::to_string(this->database.getConnectionId())
				+ "]."
		);

		// save start time for up-time calculation
		this->uptimeStart = std::chrono::steady_clock::now();
	}

	/*
	 * SERVER CLEANUP (private)
	 */

	// interrupt, wait for and clear module threads
	void Server::clearModuleThreads() {
		// interrupt module threads
		Server::interruptModuleThreads(this->crawlers);
		Server::interruptModuleThreads(this->parsers);
		Server::interruptModuleThreads(this->extractors);
		Server::interruptModuleThreads(this->analyzers);

		// wait for (and clear) module threads
		std::queue<std::string> logEntries;

		Server::waitForModuleThreads(this->crawlers, "crawler", logEntries);
		Server::waitForModuleThreads(this->parsers, "parser", logEntries);
		Server::waitForModuleThreads(this->extractors, "extrator", logEntries);
		Server::waitForModuleThreads(this->analyzers, "analyzer", logEntries);

		// write final log entries for module threads
		while(!logEntries.empty()) {
			try {
				this->database.log(logEntries.front());
			}
			catch(const DatabaseException& e) {
				std::cout	<< '\n'
							<< logEntries.front()
							<< "\nCould not write to log: "
							<< e.view()
							<< std::flush;
			}

			logEntries.pop();
		}
	}

	// wait for and clear worker threads
	void Server::clearWorkerThreads() {
		for(auto& worker : this->workers) {
			if(worker.joinable()) {
				worker.join();
			}
		}

		this->workers.clear();
	}

	// log final shutdown message, including server up-time
	void Server::clearLogShutdown() {
		try {
			this->database.log("shuts down after up-time of "
					+ Helper::DateTime::secondsToString(this->getUpTime()) + ".");
		}
		catch(const DatabaseException& e) {
			std::cout << "server shuts down after up-time of"
					<< Helper::DateTime::secondsToString(this->getUpTime()) << "."
					<< "\nCould not write to log: " << e.view() << std::flush;
		}
		catch(...) {
			std::cout << "server shuts down after up-time of"
					<< Helper::DateTime::secondsToString(this->getUpTime()) << "."
					<< "\nERROR: Unknown exception in Server::~Server()" << std::flush;
		}
	}

	/*
	 * SERVER TICK (private)
	 */

	// poll web server
	void Server::tickPollWebServer() {
		try {
			this->webServer.poll(webServerPollTimeOutMs);
		}
		catch(const DatabaseException& e) {
			// try to re-connect once on database exception
			try {
				this->database.checkConnection();
				this->database.log(
						"re-connected to database after error: "
						+ std::string(e.view())
				);
			}
			catch(const DatabaseException& e) {
				// database is offline
				this->offline = true;
			}
		}
	}

	// remove module threads if they have been successfully shut down
	void Server::tickRemoveFinishedModuleThreads() {
		Server::removeFinishedModuleThreads(this->crawlers);
		Server::removeFinishedModuleThreads(this->parsers);
		Server::removeFinishedModuleThreads(this->extractors);
		Server::removeFinishedModuleThreads(this->analyzers);
	}

	// check whether worker threads were terminated
	void Server::tickRemoveFinishedWorkerThreads() {
		if(!(this->workers.empty())) {
			std::lock_guard<std::mutex> workersLocked{this->workersLock};
			bool erased{false};

			for(std::size_t i{}; i < this->workers.size(); ++i) {
				if(erased) {
					--i;

					erased = false;
				}

				auto& worker{this->workers[i]};

				if(!(this->workersRunning[i])) {
					if(worker.joinable()) {
						worker.join();
					}

					this->workers.erase(this->workers.begin() + i);
					this->workersRunning.erase(this->workersRunning.begin() + i);

					erased = true;
				}
			}
		}
	}

	// try to re-connect to database if it is offline
	void Server::tickReconnectIfOffline() {
		if(this->offline) {
			try {
				this->database.checkConnection();

				this->offline = false;
			}
			catch(const DatabaseException &e) {}
		}
	}

	/*
	 * INTERNAL HELPER FUNCTIONS (private)
	 */

	// continue a parser thread
	void Server::continueParserThread(const ThreadDatabaseEntry &entry) {
		this->parsers.push_back(
				std::make_unique<Module::Parser::Thread>(
						this->database,
						entry.options,
						entry.status
				)
		);

		this->parsers.back()->Module::Thread::start();

		// write to log
		this->database.log(
				"parser #"
				+ std::to_string(entry.status.id)
				+ " continued."
		);
	}

	// continue an analyzer thread
	void Server::continueAnalyzerThread(const ThreadDatabaseEntry &entry) {
		// get JSON
		const std::string config(
				this->database.getConfiguration(
						entry.options.config
				)
		);

		// parse JSON
		rapidjson::Document configJson;

		try {
			configJson = Helper::Json::parseRapid(config);
		}
		catch(const JsonException& e) {
			throw Exception(
					"Could not parse configuration: "
					+ std::string(e.view())
			);
		}

		if(!configJson.IsArray()) {
			throw Exception(
					"Parsed configuration JSON is not an array."
			);
		}

		// try to add algorithm according to parsed algorithm ID
		this->analyzers.push_back(
				Module::Analyzer::Algo::initAlgo(
						AlgoThreadProperties(
								Server::getAlgoFromConfig(configJson),
								entry.options,
								entry.status,
								this->database
						)
				)
		);

		if(!(this->analyzers.back())) {
			this->analyzers.pop_back();

			this->database.log(
					"[WARNING] Unknown algorithm for analyzer #"
					+ std::to_string(entry.status.id)
					+ " ignored when loading threads."
			);

			return;
		}

		// start algorithm (and get its ID)
		this->analyzers.back()->Module::Thread::start();

		// write to log
		this->database.log(
				"analyzer #"
				+ std::to_string(entry.status.id)
				+ " continued."
		);
	}

	// check whether the server is connected and get the IP address of the connection
	std::string Server::getIp(const ConnectionPtr connection, std::string_view function) {
		if(connection == nullptr) {
			std::string exceptionString{
				"Server::"
			};

			exceptionString += function;
			exceptionString += "(): No connection specified";

			throw Exception(exceptionString);
		}

		return WebServer::getIP(connection);
	}

	// check whether the given website is being used by a thread
	bool Server::isWebsiteInUse(std::uint64_t website, ServerCommandResponse& responseTo) const {
		if(
				std::find_if(
						this->crawlers.cbegin(),
						this->crawlers.cend(),
						[&website](const auto& p) {
							return p->getWebsite() == website;
						}
				) != this->crawlers.cend()
		) {
			responseTo = ServerCommandResponse::failed(
					"Website cannot be changed while crawler is active."
			);

			return true;
		}

		if(
				std::find_if(
						this->parsers.cbegin(),
						this->parsers.cend(),
						[&website](const auto& p) {
							return p->getWebsite() == website;
						}
				) != this->parsers.cend()
		) {
			responseTo = ServerCommandResponse::failed(
					"Website cannot be changed while parser is active."
			);

			return true;
		}

		if(
				std::find_if(
						this->extractors.cbegin(),
						this->extractors.cend(),
						[&website](const auto& p) {
							return p->getWebsite() == website;
						}
				) != this->extractors.cend()
		) {
			responseTo = ServerCommandResponse::failed(
					"Website cannot be changed while extractor is active."
			);

			return true;
		}

		if(
				std::find_if(
						this->analyzers.cbegin(),
						this->analyzers.cend(),
						[&website](const auto& p) {
							return p->getWebsite() == website;
						}
				) != this->analyzers.cend()
		) {
			responseTo = ServerCommandResponse::failed(
					"Website cannot be changed while analyzer is active."
			);

			return true;
		}

		return false;
	}

	// check whether the given URL list is being used by a thread
	bool Server::isUrlListInUse(std::uint64_t urlList, ServerCommandResponse& responseTo) const {
		// check whether any thread is using the URL list
		if(
				std::find_if(
						this->crawlers.cbegin(),
						this->crawlers.cend(),
						[&urlList](const auto& p) {
							return p->getUrlList() == urlList;
						}
				) != this->crawlers.cend()
		) {
			responseTo = ServerCommandResponse::failed(
					"URL list cannot be changed while crawler is active."
			);

			return true;
		}

		if(
				std::find_if(
						this->parsers.cbegin(),
						this->parsers.cend(),
						[&urlList](const auto& p) {
							return p->getUrlList() == urlList;
						}
				) != this->parsers.cend()
		) {
			responseTo = ServerCommandResponse::failed(
					"URL list cannot be changed while parser is active."
			);

			return true;
		}

		if(
				std::find_if(
						this->extractors.cbegin(),
						this->extractors.cend(),
						[&urlList](const auto& p) {
							return p->getUrlList() == urlList;
						}
				) != this->extractors.cend()
		) {
			responseTo = ServerCommandResponse::failed(
					"URL list cannot be changed while extractor is active."
			);

			return true;
		}

		if(
				std::find_if(
						this->analyzers.cbegin(),
						this->analyzers.cend(),
						[&urlList](const auto& p) {
							return p->getUrlList() == urlList;
						}
				) != this->analyzers.cend()
		) {
			responseTo = ServerCommandResponse::failed(
					"URL list cannot be changed while analyzer is active."
			);

			return true;
		}

		return false; /* URL list not in use */
	}

	// check configuration
	bool Server::checkConfig(
			std::uint64_t config,
			ServerCommandResponse& responseTo
	) {
		if(!(this->database.isConfiguration(config))) {
			responseTo = ServerCommandResponse::failed(
					"Configuration #"
					+ std::to_string(config)
					+ " not found."
			);

			return false;
		}

		return true;
	}

	// check configuration for given website
	bool Server::checkConfig(
			std::uint64_t website,
			std::uint64_t config,
			ServerCommandResponse& responseTo
	) {
		if(!(this->database.isConfiguration(website, config))) {
			responseTo = ServerCommandResponse::failed(
					"Configuration #"
					+ std::to_string(config)
					+ " for website #"
					+ std::to_string(website)
					+ " not found."
			);

			return false;
		}

		return true;
	}

	// get command argument (string)
	bool Server::getArgument(
			const std::string& name,
			std::string& out,
			bool optional,
			bool notEmpty,
			std::string& outError
	) {
		return Server::getArgument(this->cmdJson, name, out, optional, notEmpty, outError);
	}

	// get command argument (unsigned 64-bit integer)
	bool Server::getArgument(const std::string& name, std::uint64_t& out, std::string& outError) {
		return Server::getArgument(this->cmdJson, name, out, outError);
	}

	// get command argument (boolean value)
	bool Server::getArgument(const std::string& name, bool& out, bool optional, std::string& outError) {
		return Server::getArgument(this->cmdJson, name, out, optional, outError);
	}

	// end of worker thread
	void Server::workerEnd(
			std::size_t threadIndex,
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
		this->webServer.send(
				connection,
				replyHttpCode,
				replyHttpContentType,
				replyString
		);

		// set thread status to not running
		{
			std::lock_guard<std::mutex> workersLocked{this->workersLock};

			this->workersRunning.at(threadIndex) = false;
		}
	}

	// create database connection for worker thread
	void Server::initWorkerDatabase(Module::Database& db) {
		db.setSleepOnError(this->settings.sleepOnSqlErrorS);

		db.connect();
		db.prepare();
	}

	/*
	 * INTERNAL STATIC HELPER FUNCTIONS (private)
	 */

	// begin of worker thread, returns whether (re-)parsing command JSON was successful
	bool Server::workerBegin(
			const std::string& message,
			rapidjson::Document& json,
			ServerCommandResponse& response
	) {
		// (re-)parse command JSON for the worker thread
		try {
			json = Helper::Json::parseRapid(message);

			if(!json.IsObject()) {
				response = ServerCommandResponse::failed(
						"Parsed JSON is not an object."
				);
			}
		}
		catch(const JsonException& e) {
			response = ServerCommandResponse::failed(
					"Could not parse JSON: "
					+ std::string(e.view())
					+ "."
			);
		}

		return !response.fail;
	}

	// get command argument (string) from given JSON document
	bool Server::getArgument(
					const rapidjson::Document& json,
					const std::string& name,
					std::string& out,
					bool optional,
					bool notEmpty,
					std::string& outError
	) {
		if(!json.HasMember(name)) {
			if(optional) {
				out.clear();

				return true;
			}

			outError = "Invalid arguments ('";

			outError += name;
			outError += "' is missing).";

			return false;
		}

		if(!(json[name].IsString())) {
			outError = "Invalid arguments ('";

			outError += name;
			outError += "' is not a string).";

			return false;
		}

		const auto length = json[name].GetStringLength();

		if(notEmpty && length == 0) {
			outError = "Invalid arguments ('";

			outError += name;
			outError += "' is empty).";

			return false;
		}

		out = std::string(json[name].GetString(), length);

		return true;
	}

	// get command argument (unsigned 64-bit integer) from given JSON document
	bool Server::getArgument(
			const rapidjson::Document& json,
			const std::string& name,
			std::uint64_t& out,
			std::string& outError
	) {
		if(!json.HasMember(name)) {
			outError = "Invalid arguments ('";

			outError += name;
			outError += "' is missing).";

			return false;
		}

		if(!(json[name].IsUint64())) {
			outError = "Invalid arguments ('";

			outError += name;
			outError += "' is not a valid unsigned 64-bit integer number).";

			return false;
		}

		out = json[name].GetUint64();

		return true;
	}

	// get command argument (boolean value) from given JSON document
	bool Server::getArgument(
			const rapidjson::Document& json,
			const std::string& name,
			bool& out,
			bool optional,
			std::string& outError
	) {
		if(!json.HasMember(name)) {
			if(optional) {
				out = false;

				return true;
			}

			outError = "Invalid arguments ('";

			outError += name;
			outError += "' is missing).";

			return false;
		}

		if(!(json[name].IsBool())) {
			outError = "Invalid arguments ('";

			outError += name;
			outError += "' is not a valid boolean value).";

			return false;
		}

		out = json[name].GetBool();

		return true;
	}

	// remove protocol and trailing slash(es) from the given domain
	void Server::correctDomain(std::string& inOut) {
		while(
				inOut.length() >= httpString.length()
				&& inOut.substr(0, httpString.length()) == httpString
		) {
			inOut = inOut.substr(httpString.length());
		}

		while(
				inOut.length() >= httpsString.length()
				&& inOut.substr(0, httpsString.length()) == httpsString
		) {
			inOut = inOut.substr(httpsString.length());
		}

		while(!inOut.empty() && inOut.back() == '/') {
			inOut.pop_back();
		}
	}

	// check name of website namespace
	bool Server::checkNameSpace(
			const std::string& name,
			ServerCommandResponse& responseTo
	) {
		if(name.length() < minNameSpaceLength) {
			responseTo = ServerCommandResponse::failed(
					"Website namespace has to be at least "
					+ std::string(minNameSpaceLengthString)
					+ " characters long."
			);

			return false;
		}

		if(!(Helper::Strings::checkSQLName(name))) {
			responseTo = ServerCommandResponse::failed(
					"Invalid character(s) in namespace of URL list."
			);

			return false;
		}

		if(name == "config") {
			responseTo = ServerCommandResponse::failed(
					"Namespace of URL list cannot be 'config'."
			);

			return false;
		}

		return true;
	}

	// get algorithm ID from configuration JSON, throws Main::Exception
	std::uint32_t Server::getAlgoFromConfig(const rapidjson::Document& json) {
		std::uint32_t result{};

		if(!json.IsArray()) {
			throw Exception("Server::getAlgoFromConfig(): Configuration is no array");
		}

		// go through all array items i.e. configuration entries
		for(const auto& item : json.GetArray()) {
			bool algoItem{false};

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

									if(result > 0) {
										break;
									}
								}
								else {
									break;
								}
							}
						}
						else if(itemName == "value") {
							if(property.value.IsUint()) {
								result = property.value.GetUint();

								if(algoItem) {
									break;
								}
							}
						}
					}
				}

				if(algoItem) {
					break;
				}

				result = 0;
			}
		}

		return result;
	}

	// generate server reply
	std::string Server::generateReply(
			const ServerCommandResponse& response,
			const std::string& msgBody
	) {
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
		else if(response.id > 0) {
			reply.Key("id");
			reply.Uint64(response.id);
		}

		reply.Key("text");
		reply.String(response.text.c_str(), response.text.size());

		reply.EndObject();

		return std::string(replyBuffer.GetString(), replyBuffer.GetLength());
	}

	// test query result for date/time
	std::string Server::dateTimeTest(
			const std::string& input,
			const std::string& format,
			const std::string& locale
	) {
		if(format.empty()) {
			return "";
		}

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
			result += "LOCALE ERROR: ";
			result += e.view();
		}
		catch(const DateTimeException& e) {
			result += "DATE/TIME ERROR: ";
			result += e.view();
		}

		result += "]";

		return result;
	}

	// check and get command arguments for exporting
	bool Server::cmdExportGetArguments(
			const rapidjson::Document& json,
			std::string& dataTypeTo,
			std::string& fileTypeTo,
			std::string& compressionTo,
			ServerCommandResponse& responseTo
	) {
		std::string error;

		if(
				!Server::getArgument(json, "datatype", dataTypeTo, false, true, error)
				|| !Server::getArgument(json, "filetype", fileTypeTo, false, true, error)
				|| !Server::getArgument(json, "compression", compressionTo, false, true, error)
		) {
			responseTo = ServerCommandResponse::failed(error);

			return false;
		}

		return true;
	}

	// retrieve data from database and convert it according to the specified file type
	bool Server::cmdExportRetrieveAndConvert(
			const rapidjson::Document& json,
			const std::string& dataType,
			const std::string& fileType,
			Module::Database& db,
			std::string& contentTo,
			ServerCommandResponse& responseTo
	) {
		std::queue<std::string> urls;
		std::string tableName;
		std::vector<std::vector<std::string>> tableContent;
		bool isColumnNames{true};

		// get data according to its type
		if(dataType == "urllist") {
			if(!Server::cmdExportRetrieveUrlList(json, db, urls, responseTo)) {
				return false;
			}
		}
		else if(dataType == "analyzed") {
			if(
					!Server::cmdExportRetrieveTable(
							dataType,
							json,
							db,
							tableName,
							tableContent,
							isColumnNames,
							responseTo
					)
			) {
				return false;
			}
		}
		else {
			responseTo = ServerCommandResponse::failed(
					"Unknown data type: '"
					+ dataType
					+ "'."
			);

			return false;
		}

		// create content according to file and data type
		if(fileType == "text") {
			if(dataType == "urllist") {
				if(
						!Server::cmdExportUrlListAsText(
								json,
								urls,
								contentTo,
								responseTo
						)
				) {
					return false;
				}
			}
			else {
				responseTo = ServerCommandResponse::failed(
						"Cannot export '"
						+ dataType
						+ "' as text file."
				);

				return false;
			}
		}
		else if(fileType == "spreadsheet") {
			if(dataType == "urllist") {
				if(
						!Server::cmdExportUrlListAsSpreadsheet(
								json,
								urls,
								contentTo,
								responseTo
						)
				) {
					return false;
				}
			}
			else if(dataType == "analyzed") {
				Server::cmdExportTableAsSpreadsheet(
						tableName,
						tableContent,
						contentTo,
						isColumnNames
				);
			}
			else {
				responseTo = ServerCommandResponse::failed(
						"Cannot export '"
						+ dataType
						+ "' as spreadsheet."
				);

				return false;
			}
		}
		else {
			responseTo = ServerCommandResponse::failed(
					"Unknown file type: '"
					+ fileType
					+ "'."
			);

			return false;
		}

		return true;
	}

	// compress content for export
	bool Server::cmdExportCompress(
			const std::string& dataType,
			const std::string& compression,
			std::string contentInOut,
			ServerCommandResponse& responseTo
	) {
		if(compression == "none") {
			return true;
		}

		if(compression == "gzip") {
			contentInOut = Data::Compression::Gzip::compress(contentInOut);
		}
		else if(compression == "zlib") {
			contentInOut = Data::Compression::Zlib::compress(contentInOut);
		}
		else if(compression == "zip") {
			std::vector<std::pair<std::string, std::string>> namedContents{
				{ dataType, contentInOut }
			};

			contentInOut = Data::Compression::Zip::compress(namedContents);
		}
		else {
			responseTo = ServerCommandResponse::failed(
					"Unknown compression type: '"
					+ compression
					+ "'."
			);

			return false;
		}

		return true;
	}

	// write export data to file with random file name
	void Server::cmdExportWrite(
			const std::string& content,
			ServerCommandResponse& responseTo
	) {
		// generate random file name
		const std::string fileName(
				Helper::Strings::generateRandom(
						randFileNameLength
				)
		);

		std::string fullFileName;

		fullFileName.reserve(
				cacheDir.length()
				+ fileName.length()
				+ 1 // for path seperator
		);

		fullFileName = cacheDir;

		fullFileName += Helper::FileSystem::getPathSeparator();
		fullFileName += fileName;

		// write file
		Data::File::write(fullFileName, content, true);

		// return file name
		responseTo = ServerCommandResponse(fileName);
	}

	// log successful export
	void Server::cmdExportLogSuccess(
			Module::Database& db,
			std::size_t size,
			const std::string& timeString
	) {
		// write to log
		std::ostringstream logStrStr;

		logStrStr << "complete (generated ";

		switch(size) {
		case 0:
			logStrStr << "empty file";

			break;

		case 1:
			logStrStr << "one byte";

			break;

		default:
			logStrStr.imbue(std::locale(""));

			logStrStr << size << " bytes";
		}

		logStrStr << " after " << timeString << ").";

		db.log(0, logStrStr.str());
	}

	// retrieve URL list for export
	bool Server::cmdExportRetrieveUrlList(
			const rapidjson::Document& json,
			Module::Database& db,
			std::queue<std::string>& urlsTo,
			ServerCommandResponse& responseTo
	) {
		std::uint64_t website{};
		std::uint64_t urlList{};

		if(
				Server::cmdExportGetUrlListArguments(
						json,
						website,
						urlList,
						responseTo
				)
				&& Server::cmdExportCheckWebsiteUrlList(db, website, urlList, responseTo)
		) {
			// get URLs
			urlsTo = db.getUrls(urlList);

			// write to log
			Server::cmdExportLog(db, "URL", "URLs", "URL list", urlsTo.size());

			return true;
		}

		return false;
	}

	// retrieve data from target table
	bool Server::cmdExportRetrieveTable(
			std::string_view type,
			const rapidjson::Document& json,
			Module::Database& db,
			std::string& nameTo,
			std::vector<std::vector<std::string>>& contentTo,
			bool& isColumnNamesTo,
			ServerCommandResponse& responseTo
	) {
		std::uint64_t website{};
		std::uint64_t urlList{};
		std::uint64_t tableId{};

		if(
				Server::cmdExportGetTableArguments(
						json,
						website,
						urlList,
						tableId,
						isColumnNamesTo,
						responseTo
				)
				&& Server::cmdExportCheckWebsiteUrlList(
						db,
						website,
						urlList,
						responseTo
				)
				&& Server::cmdExportCheckTargetTable(
						db,
						type,
						website,
						urlList,
						tableId,
						responseTo
				)
		) {
			// get content of table
			Server::cmdExportGetTableContent(
					db,
					type,
					website,
					urlList,
					tableId,
					nameTo,
					contentTo,
					isColumnNamesTo
			);

			// write to log
			Server::cmdExportLog(db, "row", "rows", "table", contentTo.size());

			return true;
		}

		return false;
	}

	// get arguments for exporting a URL list
	bool Server::cmdExportGetUrlListArguments(
			const rapidjson::Document& json,
			std::uint64_t& websiteTo,
			std::uint64_t& urlListTo,
			ServerCommandResponse& responseTo
	) {
		std::string error;

		if(
				Server::getArgument(json, "website", websiteTo, error)
				&& Server::getArgument(json, "urllist-source", urlListTo, error)
		) {
			return true;
		}

		responseTo = ServerCommandResponse::failed(error);

		return false;
	}

	// get arguments for exporting data from a table
	bool Server::cmdExportGetTableArguments(
			const rapidjson::Document& json,
			std::uint64_t& websiteTo,
			std::uint64_t& urlListTo,
			std::uint64_t& tableTo,
			bool& isColumnNamesTo,
			ServerCommandResponse& responseTo
	) {
		std::string error;

		if(
				Server::getArgument(json, "website", websiteTo, error)
				&& Server::getArgument(json, "urllist", urlListTo, error)
				&& Server::getArgument(json, "source", tableTo, error)
				&& Server::getArgument(json, "column-names", isColumnNamesTo, true, error)
		) {
			return true;
		}

		responseTo = ServerCommandResponse::failed(error);

		return false;
	}

	bool Server::cmdExportCheckWebsiteUrlList(
			Module::Database& db,
			std::uint64_t websiteId,
			std::uint64_t urlListId,
			ServerCommandResponse& responseTo
	) {
		return Server::checkWebsite(db, websiteId, responseTo)
			&& Server::checkUrlList(db, websiteId, urlListId, responseTo);
	}

	// check the existence of a target table for the specified website and URL list
	bool Server::cmdExportCheckTargetTable(
			Module::Database& db,
			std::string_view dataType,
			std::uint64_t websiteId,
			std::uint64_t urlListId,
			std::uint64_t tableId,
			ServerCommandResponse& responseTo
	) {
		if(db.isTargetTable(dataType, websiteId, urlListId, tableId)) {
			return true;
		}

		responseTo = ServerCommandResponse::failed(
				"Invalid source table ID."
		);

		return false;
	}

	// get the content of a target table from the database
	void Server::cmdExportGetTableContent(
			Module::Database& db,
			std::string_view dataType,
			std::uint64_t websiteId,
			std::uint64_t urlListId,
			std::uint64_t tableId,
			std::string& nameTo,
			std::vector<std::vector<std::string>>& contentTo,
			bool isIncludeColumnNames
	) {
		// create table name
		nameTo = db.getTargetTableName(dataType, tableId);

		std::string fullTableName{"crawlserv_"};

		fullTableName += db.getWebsiteNamespace(websiteId);
		fullTableName += "_";
		fullTableName += db.getUrlListNamespace(urlListId);
		fullTableName += "_analyzed_";
		fullTableName += nameTo;

		// get table columns and content
		db.readTableAsStrings(fullTableName, contentTo, isIncludeColumnNames);
	}

	// log number of entries to export
	void Server::cmdExportLog(
			Module::Database& db,
			std::string_view entryType,
			std::string_view entryTypes,
			std::string_view listType,
			std::uint64_t entryNum
	) {
		std::ostringstream logStrStr;

		logStrStr << "exporting ";

		switch(entryNum) {
		case 0:
			logStrStr << "empty " << listType;

			break;

		case 1:
			logStrStr << "one " << entryType;

			break;

		default:
			logStrStr.imbue(std::locale(""));

			logStrStr << entryNum << " " << entryTypes;
		}

		logStrStr << "...";

		db.log(0, logStrStr.str());
	}

	// export URL list as plain text
	bool Server::cmdExportUrlListAsText(
			const rapidjson::Document& json,
			std::queue<std::string>& data,
			std::string& contentTo,
			ServerCommandResponse& responseTo
	) {
		// get optional first-line header
		std::optional<std::string> optHeader;

		if(!Server::cmdExportGetFirstLineHeader(json, optHeader, responseTo)) {
			return false;
		}

		// format data
		contentTo = Data::ImportExport::Text::exportList(data, optHeader, true);

		return true;
	}

	// export URL list as spreadsheet
	bool Server::cmdExportUrlListAsSpreadsheet(
			const rapidjson::Document& json,
			std::queue<std::string>& data,
			std::string& contentTo,
			ServerCommandResponse& responseTo
	) {
		// get optional first-line header
		std::optional<std::string> optHeader;

		if(!Server::cmdExportGetFirstLineHeader(json, optHeader, responseTo)) {
			return false;
		}

		// format data
		Data::ImportExport::OpenDocument::NamedTable table;

		table.first = "URLs";

		if(optHeader) {
			table.second.emplace_back(
					std::vector<std::string>{*optHeader}
			);
		}

		while(!data.empty()) {
			table.second.emplace_back(
					std::vector<std::string>{data.front()}
			);

			data.pop();
		}

		contentTo = Data::ImportExport::OpenDocument::exportSpreadsheet(
				{ table },
				optHeader.has_value()
		);

		return true;
	}

	// export table as spreadsheet
	void Server::cmdExportTableAsSpreadsheet(
			const std::string& tableName,
			const std::vector<std::vector<std::string>>& tableContent,
			std::string& contentTo,
			bool isColumnNames
	) {
		contentTo = Data::ImportExport::OpenDocument::exportSpreadsheet(
				{ { tableName, tableContent } },
				isColumnNames
		);
	}

	// get optional first-line header for exporting
	bool Server::cmdExportGetFirstLineHeader(
			const rapidjson::Document& json,
			std::optional<std::string>& optHeaderTo,
			ServerCommandResponse& responseTo
	) {
		bool writeHeader{false};
		std::string error;

		if(
				!Server::getArgument(
						json,
						"write-firstline-header",
						writeHeader,
						false,
						error
				)
		) {
			responseTo = ServerCommandResponse::failed(error);

			return false;
		}

		if(!writeHeader) {
			// no header to write
			return true;
		}

		std::string header;

		if(
				!Server::getArgument(
						json,
						"firstline-header",
						header,
						false,
						false,
						error
				)
		) {
			responseTo = ServerCommandResponse::failed(error);

			return false;
		}

		optHeaderTo = { header };

		return true;
	}

	// check and get command arguments for deleting URLs
	bool Server::cmdDeleteUrlsGetArguments(
			const rapidjson::Document& json,
			std::uint64_t& urlListTo,
			std::uint64_t& queryTo,
			ServerCommandResponse& responseTo
	) {
		std::string error;

		if(
				Server::getArgument(json, "urllist", urlListTo, error)
				&& Server::getArgument(json, "query", queryTo, error)
		) {
			return true;
		}

		responseTo = ServerCommandResponse::failed(error);

		return false;
	}

	// get and check website for deleting URLs
	bool Server::cmdDeleteUrlsGetWebsite(
		Module::Database& db,
		std::uint64_t urlList,
		std::uint64_t& websiteTo,
		ServerCommandResponse& responseTo
	) {
		websiteTo = db.getWebsiteFromUrlList(urlList);

		if(websiteTo > 0) {
			return true;
		}

		responseTo = ServerCommandResponse::failed(
				"Could not get website for URL list #"
				+ std::to_string(urlList)
				+ "."
		);

		return false;
	}

	// get and check query for deleting URLs
	bool Server::cmdDeleteUrlsGetQuery(
			Module::Database& db,
			std::uint64_t query,
			std::string& regExTo,
			ServerCommandResponse& responseTo
	) {
		QueryProperties properties;

		db.getQueryProperties(query, properties);

		// check query type (must be RegEx)
		if(properties.type != "regex") {
			responseTo = ServerCommandResponse::failed(
					"Query #"
					+ std::to_string(query)
					+ " has invalid type (must be RegEx)."
			);

			return false;
		}

		// check query result type (must be boolean)
		if(!properties.resultBool) {
			responseTo = ServerCommandResponse::failed(
					"Query #"
					+ std::to_string(query)
					+ " has invalid result type (must be boolean)."
			);

			return false;
		}

		regExTo = properties.text;

		return true;
	}

	// use RegEx query to get URLs to be deleted from URL list
	bool Server::cmdDeleteUrlsGetUrls(
			Module::Database& db,
			std::uint64_t urlList,
			const std::string& regEx,
			std::queue<std::uint64_t>& toDeleteTo,
			ServerCommandResponse& responseTo
	) {
		try {
			// create RegEx query
			const Query::RegEx query(regEx, true, false);

			// get URLs from URL list
			auto urls{db.getUrlsWithIds(urlList)};

			// perform query on each URL in the URL list to identify which URLs to delete
			while(!urls.empty()) {
				if(query.getBool(urls.front().second)) {
					toDeleteTo.push(urls.front().first);
				}

				urls.pop();
			}
		}
		catch(const RegExException& e) {
			responseTo = ServerCommandResponse::failed(
					"RegEx error: "
					+ std::string(e.view())
			);

			return false;
		}

		// check for URLs matching the query
		if(toDeleteTo.empty()) {
			responseTo = ServerCommandResponse(
					"The query did not match any URLs in the URL list."
			);

			return false;
		}

		return true;
	}

	// delete URLs from URL list
	Struct::ServerCommandResponse Server::cmdDeleteUrlsDelete(
			Module::Database& db,
			std::uint64_t urlList,
			std::queue<std::uint64_t>& toDelete
	) {
		// delete URLs
		const auto numDeleted{db.deleteUrls(urlList, toDelete)};

		// return number of deleted URLs
		if(numDeleted == 1) {
			return ServerCommandResponse("One URL has been deleted.");
		}

		std::ostringstream responseStrStr;

		responseStrStr.imbue(std::locale(""));

		responseStrStr << numDeleted << " URLs have been deleted.";

		return ServerCommandResponse(responseStrStr.str());
	}

	// confirm deleting URLs from URL list
	Struct::ServerCommandResponse Server::cmdDeleteUrlsConfirm(std::size_t number) {
		// return number of URLs to be deleted when confirmed
		if(number == 1) {
			return ServerCommandResponse::toBeConfirmed(
					"Do you really want to delete one URL?"
					"\n!!! All associated data will be lost !!!"
			);
		}

		std::ostringstream responseStrStr;

		responseStrStr.imbue(std::locale(""));

		responseStrStr	<< "Do you really want to delete "
						<< number
						<< " URLs?\n!!! All associated data will be lost !!!";

		return ServerCommandResponse::toBeConfirmed(responseStrStr.str());
	}

} /* namespace crawlservpp::Main */
