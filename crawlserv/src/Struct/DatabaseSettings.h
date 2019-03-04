/*
 * DatabaseSettings.h
 *
 * Basic database settings (host, port, user, password, schema, compression).
 *
 *  Created on: Oct 11, 2018
 *      Author: ans
 */

#ifndef STRUCT_DATABASESETTINGS_H_
#define STRUCT_DATABASESETTINGS_H_

#include <string>

namespace crawlservpp::Struct {
	struct DatabaseSettings {
		std::string host; // host name of database server
		unsigned short port; // port of database server
		std::string user; // user name for database
		std::string password; // user password for database
		std::string name; // name/schema of database
		bool compression; // use compression protocol

		// constructors
		DatabaseSettings() { this->port = 0; this->compression = false; }
		DatabaseSettings(const std::string& setHost, unsigned short setPort, const std::string& setUser,
				const std::string& setPassword, const std::string& setName, bool setCompression)
				: host(setHost), port(setPort), user(setUser), password(setPassword), name(setName), compression(setCompression) {}
	};
}

#endif /* STRUCT_DATABASESETTINGS_H_ */
