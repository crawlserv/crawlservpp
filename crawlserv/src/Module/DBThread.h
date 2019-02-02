/*
 * DBThread.h
 *
 * Database functionality for a single thread. Only implements module-independent functionality, for module-specific functionality use the
 *  child classes of the DatabaseModule interface instead.
 *
 *  Created on: Oct 22, 2018
 *      Author: ans
 */

#ifndef MODULE_DBTHREAD_H_
#define MODULE_DBTHREAD_H_

#include "../Main/Database.h"
#include "../Struct/DatabaseSettings.h"

#include <exception>
#include <memory>
#include <stdexcept>
#include <string>

namespace crawlservpp::Module {
	class DBWrapper;

	class DBThread : public crawlservpp::Main::Database {
		friend class DBWrapper;
	public:
		DBThread(const crawlservpp::Struct::DatabaseSettings& dbSettings);
		virtual ~DBThread();

		bool prepare();

		// thread functions
		void setThreadStatusMessage(unsigned long threadId, bool threadPaused, const std::string& threadStatusMessage);
		void setThreadProgress(unsigned long threadId, float threadProgress);
		void setThreadLast(unsigned long threadId, unsigned long threadLast);

	private:
		// IDs of prepared SQL statements
		unsigned short psSetThreadStatusMessage;
		unsigned short psSetThreadProgress;
		unsigned short psSetThreadLast;
	};
}

#endif /* MODULE_DBTHREAD_H_ */
