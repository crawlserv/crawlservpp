/*
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

#include "../Main/Database.hpp"
#include "../Main/Exception.hpp"
#include "../Struct/DatabaseSettings.hpp"
#include "../Struct/ModuleOptions.hpp"

#include <memory>
#include <sstream>
#include <string>

namespace crawlservpp::Wrapper {
	class Database;
} /* crawlservpp::Wrapper */

namespace crawlservpp::Module {
	class Database : public Main::Database {
		friend class Wrapper::Database;

		// for convenience
		typedef Struct::DatabaseSettings DatabaseSettings;
		typedef Struct::ModuleOptions ModuleOptions;

		typedef std::unique_ptr<sql::ResultSet> SqlResultSetPtr;
	public:
		// constructor
		Database(const DatabaseSettings& dbSettings, const std::string& dbModule);

		// destructor
		virtual ~Database();

		// setters
		void setOptions(const ModuleOptions& moduleOptions);
		void setThreadId(unsigned long id);
		void setLogging(bool isLogging, bool isVerbose);

		// command function
		void prepare();

		// logging function
		void log(const std::string& logEntry);

		// thread functions
		void setThreadStatusMessage(unsigned long threadId, bool threadPaused, const std::string& threadStatusMessage);
		void setThreadProgress(unsigned long threadId, float threadProgress, unsigned long threadRunTime);
		void setThreadLast(unsigned long threadId, unsigned long threadLast);

		// sub-class for Module::Database exceptions
		class Exception : public Main::Exception {
		public:
			Exception(const std::string& description) : Main::Exception(description) {}
			virtual ~Exception() {}
		};

	private:
		// general thread options
		ModuleOptions options;
		std::string threadIdString;
		std::string websiteIdString;
		std::string urlListIdString;
		bool logging;
		bool verbose;

		// IDs of prepared SQL statements
		struct _ps {
			unsigned short setThreadStatusMessage;
			unsigned short setThreadProgress;
			unsigned short setThreadLast;
		} ps;
	};

} /* crawlservpp::Module */

#endif /* MODULE_DATABASE_HPP_ */
