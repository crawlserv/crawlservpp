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
 * DatabaseSettings.hpp
 *
 * Basic database settings (host, port, user, password, schema, compression).
 *
 *  Created on: Oct 11, 2018
 *      Author: ans
 */

#ifndef STRUCT_DATABASESETTINGS_HPP_
#define STRUCT_DATABASESETTINGS_HPP_

#include <string>	// std::string

namespace crawlservpp::Struct {

	struct DatabaseSettings {
		std::string host;		// host name of database server
		unsigned short port;	// port of database server
		std::string user;		// user name for database
		std::string password;	// user password for database
		std::string name;		// name/schema of database
		bool compression;		// use compression protocol

		// constructors
		DatabaseSettings() : port(0), compression(false) {}
		DatabaseSettings(
				const std::string& setHost,
				unsigned short setPort,
				const std::string& setUser,
				const std::string& setPassword,
				const std::string& setName,
				bool setCompression
		) : host(setHost),
			port(setPort),
			user(setUser),
			password(setPassword),
			name(setName),
			compression(setCompression) {}
	};

} /* crawlservpp::Struct */

#endif /* STRUCT_DATABASESETTINGS_HPP_ */
