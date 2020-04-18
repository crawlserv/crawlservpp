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
 * App.hpp
 *
 * The main application class that processes command line arguments, shows the initial header and component versions, loads the
 *  configuration from the argument-specified configuration file and creates as well as starts the server.
 *
 *  Created on: Oct 26, 2018
 *      Author: ans
 */

#ifndef MAIN_APP_HPP_
#define MAIN_APP_HPP_

#include "ConfigFile.hpp"
#include "Exception.hpp"
#include "Server.hpp"
#include "Version.hpp"

#include "../Helper/DateTime.hpp"
#include "../Helper/Portability/compiler.h"
#include "../Helper/Portability/getch.h"
#include "../Helper/Versions.hpp"
#include "../Struct/DatabaseSettings.hpp"
#include "../Struct/NetworkSettings.hpp"
#include "../Struct/ServerSettings.hpp"

#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>

#include <atomic>		// std::atomic
#include <csignal>		// sigaction, sigemptyset [Linux] / signal [Windows], SIGINT, SIGTERM
#include <cstdint>		// std::uint16_t
#include <exception>	// std::exception
#include <iostream>		// std::cout, std::endl, std::flush
#include <memory>		// std::make_unique, std::unique_ptr
#include <string>		// std::string

namespace crawlservpp::Main {

	class App {
		// for convenience
		using DatabaseSettings = Struct::DatabaseSettings;
		using NetworkSettings = Struct::NetworkSettings;
		using ServerSettings = Struct::ServerSettings;

	public:
		App(int argc, char * argv[]) noexcept;
		virtual ~App();

		int run() noexcept;

		// signal handling
		static std::atomic<int> interruptionSignal;
		static void signal(int signalNumber);
		void shutdown();

		// not moveable, not copyable
		App(App&) = delete;
		App(App&&) = delete;
		App& operator=(App&) = delete;
		App& operator=(App&&) = delete;

	private:
		std::atomic<bool> running;
		std::unique_ptr<Server> server;
		bool showVersionsOnly;

		// helper function
		bool getPassword(DatabaseSettings& dbSettings);

		// static helper functions
		static void outputHeader(bool showLibraryVersions);
		static void checkArgumentNumber(int argc);
		static void loadConfig(
				const std::string& fileName,
				ServerSettings& serverSettings,
				DatabaseSettings& dbSettings,
				NetworkSettings& networkSettings
		);
	};

} /* crawlservpp::Main */

#endif /* MAIN_APP_HPP_ */
