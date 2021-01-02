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
 * DatabaseSettings.hpp
 *
 * Database settings (host, port, user, password, schema and compression).
 *
 *  Created on: Oct 11, 2018
 *      Author: ans
 */

#ifndef STRUCT_DATABASESETTINGS_HPP_
#define STRUCT_DATABASESETTINGS_HPP_

#include <cstdint>		// std::uint16_t
#include <string>		// std::string
#include <string_view>	// std::string_view

namespace crawlservpp::Struct {

	//! Database settings containing its host, port, user, password, schema, and compression.
	/*!
	 * These settings are read from the
	 *  configuration file.
	 *
	 * They contain additional debugging
	 *  information.
	 */
	struct DatabaseSettings {
		///@name Properties
		///@{

		//! The host name to be used to connect to the database server.
		std::string host;

		//! The port to be used to connect to the database server.
		std::uint16_t port{};

		//! The user name to be used to connect to the database.
		std::string user;

		//! The password to be used to connect to the database.
		/*!
		 * \warning The password will remain as
		 *   plain text in memory as long as the
		 *   command-and-control server is running.
		 */
		std::string password;

		//! The name/schema of the database to be connected to.
		std::string name;

		//! Indicates whether compression should be used for the database connection.
		bool compression{false};

		//! The debug directory to be used by the current thread.
		std::string_view debugDir;

		//! Indicates whether debug logging (to file) is enabled for the current thread.
		bool debugLogging{false};

		///@}
		///@name Construction
		///@{

		//! Default constructor.
		DatabaseSettings() = default;

		//! Constructor copying database settings, but setting a distinct debug directory.
		/*!
		 * \param other Constant reference to a
		 *   structure containing database settings
		 *   to be copied to the current instance.
		 * \param setDebugDir View of a string
		 *   containing the debug directory to be
		 *   set explicitly.
		 */
		DatabaseSettings(const DatabaseSettings& other, std::string_view setDebugDir) {
			this->host = other.host;
			this->port = other.port;
			this->user = other.user;
			this->password = other.password;
			this->name = other.name;
			this->compression = other.compression;
			this->debugLogging = other.debugLogging;
			this->debugDir = setDebugDir;
		}

		///@}
	};

} /* namespace crawlservpp::Struct */

#endif /* STRUCT_DATABASESETTINGS_HPP_ */
