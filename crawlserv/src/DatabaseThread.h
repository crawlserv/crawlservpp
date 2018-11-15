/*
 * DatabaseThread.h
 *
 * Database functionality for a single thread. Only implements module-independent functionality, for module-specific functionality use the
 *  child classes of the DatabaseModule interface instead.
 *
 *  Created on: Oct 22, 2018
 *      Author: ans
 */

#ifndef DATABASETHREAD_H_
#define DATABASETHREAD_H_

#include "Database.h"

#include "structs/DatabaseSettings.h"

#include <exception>
#include <stdexcept>
#include <string>

class DatabaseModule;

class DatabaseThread : public Database {
	friend class DatabaseModule;
public:
	DatabaseThread(const DatabaseSettings& dbSettings);
	virtual ~DatabaseThread();

	bool prepare();

	// thread functions
	void setThreadStatusMessage(unsigned long threadId, bool threadPaused, const std::string& threadStatusMessage);
	void setThreadProgress(unsigned long threadId, float threadProgress);
	void setThreadLast(unsigned long threadId, unsigned long threadLast);

private:
	// ids of prepared SQL statements
	unsigned short psSetThreadStatusMessage;
	unsigned short psSetThreadProgress;
	unsigned short psSetThreadLast;
};

#endif /* DATABASETHREAD_H_ */
