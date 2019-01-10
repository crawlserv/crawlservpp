/*
 * ThreadParser.h
 *
 * Implementation of the Thread interface for parser threads.
 *
 *  Created on: Oct 11, 2018
 *      Author: ans
 */

#ifndef THREADPARSER_H_
#define THREADPARSER_H_

#include "ConfigParser.h"
#include "Database.h"
#include "DatabaseParser.h"
#include "QueryContainer.h"
#include "Thread.h"
#include "TimerStartStop.h"
#include "XMLDocument.h"

#include "namespaces/DateTime.h"
#include "namespaces/Json.h"
#include "namespaces/Strings.h"
#include "structs/IdString.h"
#include "structs/ThreadOptions.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

class ThreadParser: public Thread, public QueryContainer {
public:
	ThreadParser(Database& database, unsigned long crawlerId, const std::string& crawlerStatus, bool crawlerPaused,
			const ThreadOptions& threadOptions, unsigned long crawlerLast);
	ThreadParser(Database& database, const ThreadOptions& threadOptions);
	virtual ~ThreadParser();

protected:
	DatabaseParser database;

	bool onInit(bool resumed) override;
	bool onTick() override;
	void onPause() override;
	void onUnpause() override;
	void onClear(bool interrupted) override;

private:
	// hide functions not to be used by thread
	void start();
	void pause();
	void unpause();
	void stop();
	void interrupt();

	// configuration
	ConfigParser config;
	bool idFromUrl;

	std::vector<QueryContainer::Query> queriesId;
	std::vector<QueryContainer::Query> queriesDateTime;
	std::vector<QueryContainer::Query> queriesFields;

	// timing
	unsigned long long tickCounter;
	std::chrono::steady_clock::time_point startTime;
	std::chrono::steady_clock::time_point pauseTime;
	std::chrono::steady_clock::time_point idleTime;

	// parsing state
	IdString currentUrl;	// currently parsed URL
	std::string lockTime;	// last locking time for currently parsed URL

	// initializing function
	void initTargetTable();
	void initQueries() override;

	// parsing functions
	bool parsingUrlSelection();
	unsigned long parsing();
	bool parsingContent(const IdString& content, const std::string& parsedId);
};

#endif /* THREADPARSER_H_ */
