/*
 * Database.h
 *
 * This class provides database functionality for a parser thread by implementing the Wrapper::Database interface.
 *
 *  Created on: Oct 22, 2018
 *      Author: ans
 */

#ifndef MODULE_PARSER_DATABASE_H_
#define MODULE_PARSER_DATABASE_H_

#include "../Database.h"

#include "../../Struct/CustomTableProperties.h"
#include "../../Struct/TableColumn.h"
#include "../../Struct/UrlProperties.h"
#include "../../Wrapper/Database.h"

#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>

#include <mysql_connection.h>

#include <chrono>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace crawlservpp::Module::Parser {
	class Database : public Wrapper::Database {
		// for convenience
		typedef Main::Database::Exception DatabaseException;
		typedef Struct::CustomTableProperties CustomTableProperties;
		typedef Struct::TableColumn TableColumn;
		typedef Struct::UrlProperties UrlProperties;
		typedef std::pair<unsigned long, std::string> IdString;

	public:
		Database(Module::Database& dbRef);
		virtual ~Database();

		// setters
		void setId(unsigned long analyzerId);
		void setWebsite(unsigned long websiteId);
		void setWebsiteNamespace(const std::string& websiteNamespace);
		void setUrlList(unsigned long listId);
		void setUrlListNamespace(const std::string& urlListNamespace);
		void setReparse(bool isReparse);
		void setParseCustom(bool isParseCustom);
		void setLogging(bool isLogging);
		void setVerbose(bool isVerbose);
		void setTargetTable(const std::string& table);
		void setTargetFields(const std::vector<std::string>& fields);
		void setTimeoutTargetLock(unsigned long timeOut);

		// prepare target table and SQL statements for parser
		void initTargetTable();
		void prepare();

		// URL functions
		UrlProperties getNextUrl(unsigned long currentUrlId);
		unsigned long getUrlPosition(unsigned long urlId);
		unsigned long getNumberOfUrls();

		// URL locking functions
		bool isUrlLockable(unsigned long lockId);
		bool checkUrlLock(unsigned long lockId, const std::string& lockTime);
		std::string getUrlLock(unsigned long lockId);
		void getUrlLockId(UrlProperties& urlProperties);
		std::string lockUrl(UrlProperties& urlProperties, unsigned long lockTimeout);
		void unLockUrl(unsigned long lockId);

		// parsing functions
		bool getLatestContent(unsigned long urlId, unsigned long index, std::pair<unsigned long, std::string>& contentTo);
		std::queue<IdString> getAllContents(unsigned long urlId);
		unsigned long getContentIdFromParsedId(const std::string& parsedId);
		void updateOrAddEntry(unsigned long contentId, const std::string& parsedId, const std::string& parsedDateTime,
				const std::vector<std::string>& parsedFields);
		void setUrlFinished(unsigned long parsingId);

	protected:
		// options
		std::string idString;
		unsigned long website;
		std::string websiteIdString;
		std::string websiteName;
		unsigned long urlList;
		std::string listIdString;
		std::string urlListName;
		bool reparse;
		bool parseCustom;
		bool logging;
		bool verbose;
		std::string targetTableName;
		std::vector<std::string> targetFieldNames;
		unsigned long timeoutTargetLock;

		// table names and target table ID
		std::string urlListTable;
		std::string parsingTable;
		std::string analyzingTable;
		unsigned long targetTableId;
		std::string targetTableFull;

	private:
		// IDs of prepared SQL statements
		struct {
			unsigned short isUrlParsed;
			unsigned short getNextUrl;
			unsigned short getUrlPosition;
			unsigned short getNumberOfUrls;
			unsigned short isUrlLockable;
			unsigned short checkUrlLock;
			unsigned short getUrlLock;
			unsigned short getUrlLockId;
			unsigned short lockUrl;
			unsigned short addUrlLock;
			unsigned short unLockUrl;
			unsigned short getContentIdFromParsedId;
			unsigned short getLatestContent;
			unsigned short getAllContents;
			unsigned short setUrlFinished;
			unsigned short getEntryId;
			unsigned short updateEntry;
			unsigned short addEntry;
			unsigned short updateParsedTable;
		} ps;

		// internal helper functions
		unsigned long getTargetTableId();
		unsigned long getEntryId(unsigned long contentId);
		void updateEntry(unsigned long entryId, const std::string& parsedId, const std::string& parsedDateTime,
				const std::vector<std::string>& parsedFields);
		void addEntry(unsigned long contentId, const std::string& parsedId, const std::string& parsedDateTime,
				const std::vector<std::string>& parsedFields);
		void updateParsedTable();
	};
}

#endif /* MODULE_PARSER_DATABASE_H_ */
