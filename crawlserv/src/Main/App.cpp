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
 * App.cpp
 *
 * The main application class that processes command line arguments, shows the initial header and component versions, loads the
 *  configuration from the argument-specified configuration file and creates as well as starts the server.
 *
 *  Created on: Oct 26, 2018
 *      Author: ans
 */

#include "App.hpp"

namespace crawlservpp::Main {
	std::atomic<int> App::interruptionSignal(0);

	// constructor: show header, check arguments, load configuration file, get database password, initialize and run the server
	App::App(int argc, char * argv[]) noexcept : running(true), showVersionsOnly(false) {
		try {
			ServerSettings serverSettings;
			DatabaseSettings dbSettings;
			NetworkSettings networkSettings;

			std::string error;

#ifdef _WIN32
			signal(SIGINT, App::signal);
			signal(SIGTERM, App::signal);
#else
			struct sigaction sigIntHandler;

			sigIntHandler.sa_handler = App::signal;

			sigemptyset(&sigIntHandler.sa_mask);

			sigIntHandler.sa_flags = 0;

			sigaction(SIGINT, &sigIntHandler, nullptr);
			sigaction(SIGTERM, &sigIntHandler, nullptr);
#endif

			// check number of arguments
			this->checkArgumentNumber(argc);

			// check argument
			if(std::string(argv[1]) == "-v")
				this->showVersionsOnly = true;

			// show header
			this->outputHeader(this->showVersionsOnly);

			if(this->showVersionsOnly)
				return;

			// load configuration file
			this->loadConfig(argv[1], serverSettings, dbSettings, networkSettings);

			// get password
			if(this->getPassword(dbSettings)) {
				if(this->running.load()) {
					// create server and run!
					this->server = std::make_unique<Server>(serverSettings, dbSettings, networkSettings);

					std::cout << "Server is up and running." << std::flush;
				}
			}
			else
				this->running.store(false);
		}
		catch(const std::exception& e) {
			std::cout << "[ERROR] " << e.what() << std::endl;

			this->running.store(false);
		}
		catch(...) {
			std::cout << "[ERROR] Unknown exception in App::App()" << std::endl;

			this->running.store(false);
		}
	}

	// destructor: clean-up server
	App::~App() {
		if(this->server) {
			// server up-time message
			std::cout << "\nUp-time: " << Helper::DateTime::secondsToString(server->getUpTime()) << ".";

			const auto threads = this->server->getActiveThreads();
			const auto workers = this->server->getActiveWorkers();

			if(threads || workers) {
				std::cout << "\n> Waiting for threads (";

				if(threads) {
					std::cout << threads << " module thread";

					if(threads > 1)
						std::cout << "s";
				}

				if(threads && workers)
					std::cout << ", ";

				if(workers) {
					std::cout << workers << " worker thread";

					if(workers > 1)
						std::cout << "s";
				}

				std::cout << " active)..." << std::flush;
			}

			this->server.reset();
		}

		// quit message
		std::cout << "\nBye bye." << std::endl;
	}

	// run app
	int App::run() noexcept {
		if(this->showVersionsOnly)
			return EXIT_SUCCESS;

		if(this->server) {
			try {
				while(this->server->tick() && this->running.load()) {
					if(App::interruptionSignal.load())
						this->shutdown();
				}

				return EXIT_SUCCESS;
			}
			catch(const std::exception& e) {
				std::cout << "\n[ERROR] " << e.what() << std::flush;
			}
			catch(...) {
				std::cout << "\n[ERROR] Unknown exception in App::run()" << std::flush;
			}
		}

		return EXIT_FAILURE;
	}

	// static signal handler (forward the signal to the class)
	void App::signal(int signalNumber) {
		App::interruptionSignal.store(signalNumber);
	}

	// in-class signal handler
	void App::shutdown() {
		std::cout << "\n[SHUTDOWN] ";

		const auto signal = App::interruptionSignal.load();

		switch(signal) {
		case SIGINT:
			std::cout << "Interruption request signal (SIGINT)";

			break;

		case SIGTERM:
			std::cout << "Termination request signal (SIGTERM)";

			break;

		default:
			std::cout << "Unknown signal (#" << signal << ")";
		}

		std::cout << " received." << std::flush;

		this->running.store(false);
	}

	// helper function: get database password from user, return false on cancel
	bool App::getPassword(DatabaseSettings& dbSettings) {
		// prompt password for database
		std::cout << "Enter password for " << dbSettings.user << "@" << dbSettings.host << ":" << dbSettings.port << ": ";

		char input = 0;
		bool inputLoop = true;
		bool inputCancel = false;

		do {
			switch(input = Helper::Portability::getch()) {
			case '\r':
				// ignore carriage return
				break;

			case '\n':
				// ENTER: end input loop
				for(std::string::size_type n = 0; n < dbSettings.password.size(); n++)
					std::cout << '\b';

				std::cout << "[DONE]" << std::string(dbSettings.password.size() - 6, ' ') << std::flush;

				inputLoop = false;

				break;

			case '\b':
			case 127:
				// BACKSPACE/DELETE: delete last character from password (if it exists)
				if(!dbSettings.password.empty()) {
					dbSettings.password.pop_back();

					std::cout << "\b \b" << std::flush;
				}

				break;

			case -1:
			case 3:
			case 27:
				// CTRL+C, ESCAPE -> cancel and end input loop
				std::cout << "[CANCELLED]";

				inputCancel = true;
				inputLoop = false;

				break;

			default:
				// add other characters to password
				dbSettings.password.push_back(input);

				std::cout << '*' << std::flush;
			}

		}
		while(inputLoop && this->running.load());

		std::cout << std::endl;

		if(inputCancel)
			return false;

		return true;
	}

	// static helper function: show version (and library versions if necessary)
	void App::outputHeader(bool showLibraryVersions) {
		std::cout
				<<	"crawlserv++ Command-and-Control Server\n"
					"Version "
				<<	Version::getString()
				<<	"\n\n"
					"Copyright (C) "
				<<	(__DATE__ + 7)
				<<	" Anselm Schmidt (ans[ät]ohai.su)\n\n"
					"This program is free software: you can redistribute it and/or modify\n"
					"it under the terms of the GNU General Public License as published by\n"
					"the Free Software Foundation, either version 3 of the License, or\n"
					"(at your option) any later version.\n\n"
					"This program is distributed in the hope that it will be useful,\n"
			    	"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
			    	"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the\n"
			    	"GNU General Public License for more details.\n\n"
					"You should have received a copy of the GNU General Public License\n"
			    	"along with this program. If not, see <https://www.gnu.org/licenses/>.\n";

		if(showLibraryVersions)
			std::cout
					<< "\nusing:"
					<< Helper::Versions::getLibraryVersionsStr("\t");

		std::cout << std::endl;
	}

	// static helper function: check number of command line arguments, throws Main::Exception
	void App::checkArgumentNumber(int argc) {
		if(argc != 2)
			throw Exception("USAGE: crawlserv <config_file> or crawlserv -v");
	}

	// static helper function: load database and server settings from configuration file, throws Main::Exception
	void App::loadConfig(
			const std::string& fileName,
			ServerSettings& serverSettings,
			DatabaseSettings& dbSettings,
			NetworkSettings& networkSettings
	) {
		// read file
		const ConfigFile configFile(fileName);

		// set database settings
		dbSettings.host = configFile.getValue("db_host");

		try {
			dbSettings.port = boost::lexical_cast<unsigned short>(configFile.getValue("db_port"));
		}
		catch(const boost::bad_lexical_cast& e) {
			throw Exception(
							fileName + ":"
							" Could not convert config file entry \"db_port\""
							" (=\"" + configFile.getValue("db_port") + "\") to numeric value"
			);
		}

		dbSettings.user = configFile.getValue("db_user");
		dbSettings.name = configFile.getValue("db_name");

		if(!configFile.getValue("db_debug_logging").empty()) {
			try {
				dbSettings.debugLogging = boost::lexical_cast<bool>(configFile.getValue("db_debug_logging"));
			}
			catch(const boost::bad_lexical_cast& e) {
				throw Exception(
						fileName + ":"
						" Could not convert config file entry \"db_debug_logging\""
						" (=\"" + configFile.getValue("db_debug_logging") + "\")"
						" to boolean value"
				);
			}
		}
		else
			dbSettings.debugLogging = false;

		// set server settings
		if(!configFile.getValue("server_client_compress").empty()) {
			try {
				dbSettings.compression = boost::lexical_cast<bool>(configFile.getValue("server_client_compress"));
			}
			catch(const boost::bad_lexical_cast& e) {
				throw Exception(
						fileName + ":"
						" Could not convert config file entry"
						" \"server_client_compress\" (=\"" + configFile.getValue("server_client_compress") + "\")"
						" to boolean value"
				);
			}
		}

		serverSettings.port = configFile.getValue("server_port");
		serverSettings.allowedClients = configFile.getValue("server_allow");

		if(!configFile.getValue("server_cors_origins").empty())
			serverSettings.corsOrigins = configFile.getValue("server_cors_origins");

		if(!configFile.getValue("server_logs_deletable").empty()) {
			try {
				serverSettings.logsDeletable = boost::lexical_cast<bool>(configFile.getValue("server_logs_deletable"));
			}
			catch(const boost::bad_lexical_cast& e) {
				throw Exception(
						fileName + ":"
						" Could not convert config file entry \"server_logs_deletable\""
						" (=\""	+ configFile.getValue("server_logs_deletable") + "\")"
						" to boolean value"
				);
			}
		}
		else
			serverSettings.logsDeletable = false;

		if(!configFile.getValue("server_data_deletable").empty()) {
			try {
				serverSettings.dataDeletable = boost::lexical_cast<bool>(configFile.getValue("server_data_deletable"));
			}
			catch(const boost::bad_lexical_cast& e) {
				throw Exception(
						fileName + ":"
						" Could not convert config file entry \"server_data_deletable\""
						" (=\"" + configFile.getValue("server_data_deletable") + "\")"
						" to boolean value"
				);
			}
		}
		else
			serverSettings.dataDeletable = false;

		networkSettings.defaultProxy = configFile.getValue("network_default_proxy");
		networkSettings.torControlServer = configFile.getValue("network_tor_control_server");

		if(!networkSettings.torControlServer.empty()) {
			try {
				networkSettings.torControlPort = boost::lexical_cast<unsigned short>(
						configFile.getValue("network_tor_control_port")
				);
			}
			catch(const boost::bad_lexical_cast& e) {
				throw Exception(
								fileName + ":"
								" Could not convert config file entry \"network_tor_control_port\""
								" (=\"" + configFile.getValue("network_tor_control_port") + "\") to numeric value"
				);
			}

			networkSettings.torControlPassword = configFile.getValue("network_tor_control_password");
		}
	}

} /* crawlservpp::App */
