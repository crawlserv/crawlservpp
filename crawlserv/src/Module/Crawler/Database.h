/*
 * Database.h
 *
 * This class provides database functionality for a crawler thread by implementing the DatabaseModule interface.
 *
 *  Created on: Oct 22, 2018
 *      Author: ans
 */

#ifndef MODULE_CRAWLER_DATABASE_H_
#define MODULE_CRAWLER_DATABASE_H_

#include "../DBWrapper.h"
#include "../DBThread.h"

#include "../../Struct/IdString.h"

#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>

#include <mysql_connection.h>

#include <chrono>
#include <fstream>
#include <sstream>
#include <string>

namespace crawlservpp::Module::Crawler {
	class Database : public crawlservpp::Module::DBWrapper {
	public:
		Database(crawlservpp::Module::DBThread& dbRef);
		virtual ~Database();

		// prepare SQL statements for crawler
		bool prepare(unsigned long crawlerId, const std::string& websiteNameSpace, const std::string& urlListNameSpace, bool recrawl,
				bool verbose);

		// table function
		void lockUrlList();

		// URL functions
		bool isUrlExists(const std::string& urlString);
		unsigned long getUrlId(const std::string& urlString);
		bool isUrlCrawled(unsigned long urlId);
		crawlservpp::Struct::IdString getNextUrl(unsigned long currentUrlId);
		unsigned long addUrl(const std::string& urlString, bool manual);
		unsigned long getUrlPosition(unsigned long urlId);
		unsigned long getNumberOfUrls();
		void addLinkIfNotExists(unsigned long from, unsigned long to, bool archived);

		// URL locking functions
		bool isUrlLockable(unsigned long urlId);
		bool checkUrlLock(unsigned long urlId, const std::string& lockTime);
		std::string getUrlLock(unsigned long urlId);
		std::string lockUrl(unsigned long lockTimeout, unsigned long urlId);
		void unLockUrl(unsigned long urlId);

		// crawling functions
		void saveContent(unsigned long urlId, unsigned int response, const std::string& type, const std::string& content);
		void saveArchivedContent(unsigned long urlId, const std::string& timeStamp, unsigned int response, const std::string& type,
				const std::string& content);
		void setUrlFinished(unsigned long urlId);
		bool isArchivedContentExists(unsigned long urlId, const std::string& timeStamp);

		// helper functions (using multiple database commands)
		bool renewUrlLock(unsigned long lockTimeout, unsigned long urlId, std::string& lockTime);

	private:
		// URL list name
		std::string urlListTable;
		std::string linkTable;

		// prepared SQL statements
		unsigned short psIsUrlExists;
		unsigned short psIsUrlHashExists;
		unsigned short psGetUrlId;
		unsigned short psIsUrlCrawled;
		unsigned short psGetNextUrl;
		unsigned short psAddUrl;
		unsigned short psGetUrlPosition;
		unsigned short psGetNumberOfUrls;
		unsigned short psIsUrlLockable;
		unsigned short psCheckUrlLock;
		unsigned short psGetUrlLock;
		unsigned short psLockUrl;
		unsigned short psUnLockUrl;
		unsigned short psSaveContent;
		unsigned short psSaveArchivedContent;
		unsigned short psSetUrlFinished;
		unsigned short psIsArchivedContentExists;
		unsigned short psIsLinkExists;
		unsigned short psAddLink;
		unsigned short psAddLinkArchived;
	};
}

#endif /* MODULE_CRAWLER_DATABASE_H_ */
