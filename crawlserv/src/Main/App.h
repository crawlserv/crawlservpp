/*
 * App.h
 *
 * The main application class that processes command line arguments, shows the initial header and component versions, loads the
 *  configuration from the argument-specified configuration file and creates as well as starts the server.
 *
 *  Created on: Oct 26, 2018
 *      Author: ans
 */

#ifndef MAIN_APP_H_
#define MAIN_APP_H_

#include "ConfigFile.h"
#include "Server.h"

#include "../Helper/DateTime.h"
#include "../Helper/Portability.h"
#include "../Helper/Versions.h"
#include "../Struct/DatabaseSettings.h"
#include "../Struct/ServerSettings.h"

#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace crawlservpp::Main {
	class App {
		// for convenience
		typedef Struct::DatabaseSettings DatabaseSettings;
		typedef Struct::ServerSettings ServerSettings;

	public:
		App(int argc, char * argv[]) noexcept;
		virtual ~App();

		int run() noexcept;

		// signal handling
		static App * instance;
		static void signal(int num);
		void shutdown(int num);

		// not moveable, not copyable
		App(App&) = delete;
		App(App&&) = delete;
		App& operator=(App&) = delete;
		App& operator=(App&&) = delete;

	private:
		std::atomic<bool> running;
		std::unique_ptr<Server> server;

		// helper function
		bool getPassword(DatabaseSettings& dbSettings);

		// static helper functions
		static void outputHeader();
		static void checkArgumentNumber(int argc);
		static void loadConfig(const std::string& fileName, DatabaseSettings& dbSettings, ServerSettings& serverSettings);
	};
}

#endif /* MAIN_APP_H_ */
