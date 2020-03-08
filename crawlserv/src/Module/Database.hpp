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
 * Database.hpp
 *
 * Database functionality for a single thread.
 *
 * Only implements module-independent functionality. For module-specific functionality use the
 *  child classes of the Wrapper::Database interface instead.
 *
 *  Created on: Oct 22, 2018
 *      Author: ans
 */

#ifndef MODULE_DATABASE_HPP_
#define MODULE_DATABASE_HPP_

#include "../Helper/DateTime.hpp"
#include "../Helper/FileSystem.hpp"
#include "../Helper/Utf8.hpp"
#include "../Main/Database.hpp"
#include "../Main/Exception.hpp"
#include "../Struct/DatabaseSettings.hpp"
#include "../Struct/ModuleOptions.hpp"

#include <climits>	// USHRT_MAX
#include <fstream>	// std::ofstream
#include <memory>	// std::unique_ptr
#include <queue>	// std::queue
#include <string>	// std::string, std::to_string

namespace crawlservpp::Wrapper {

	class Database;

} /* crawlservpp::Wrapper */

namespace crawlservpp::Module {

	class Database : public Main::Database {
		friend class Wrapper::Database;

		// for convenience
		using DatabaseSettings = Struct::DatabaseSettings;
		using ModuleOptions = Struct::ModuleOptions;

		using SqlResultSetPtr = std::unique_ptr<sql::ResultSet>;

	public:
		// constructor
		Database(const DatabaseSettings& dbSettings, const std::string& dbModule);

		// destructor
		virtual ~Database();

		// setters
		void setOptions(const ModuleOptions& moduleOptions);
		void setThreadId(size_t id);
		void setLogging(unsigned short level, unsigned short min, unsigned short verbose);

		// command function
		void prepare();

		// logging functions
		void log(unsigned short level, const std::string& logEntry);
		void log(unsigned short level, std::queue<std::string>& logEntries);

		// thread functions
		void setThreadStatusMessage(size_t threadId, bool threadPaused, const std::string& threadStatusMessage);
		void setThreadProgress(size_t threadId, float threadProgress, size_t threadRunTime);
		void setThreadLast(size_t threadId, size_t threadLast);

		// class for Module::Database exceptions
		MAIN_EXCEPTION_CLASS();

	private:
		// general thread options
		ModuleOptions options;
		std::string threadIdString;
		std::string websiteIdString;
		std::string urlListIdString;
		unsigned short loggingLevel;
		unsigned short loggingMin;
		unsigned short loggingVerbose;
		std::ofstream loggingFile;
		bool debugLogging;
		std::string debugDir;

		// private helper function
		void initDebugLogging();

		// IDs of prepared SQL statements
		struct _ps {
			unsigned short setThreadStatusMessage;
			unsigned short setThreadProgress;
			unsigned short setThreadLast;
		} ps;
	};

} /* crawlservpp::Module */

#endif /* MODULE_DATABASE_HPP_ */
