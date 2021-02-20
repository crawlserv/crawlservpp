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
 * ServerSettings.hpp
 *
 * Server settings (port, allowed clients, origins and actions).
 *
 *  Created on: Oct 11, 2018
 *      Author: ans
 */

#ifndef STRUCT_SERVERSETTINGS_HPP_
#define STRUCT_SERVERSETTINGS_HPP_

#include <cstdint>	// std::uint16_t
#include <string>	// std::string

namespace crawlservpp::Struct {

	///@name Constants
	///@{

	constexpr auto serverSleepOnSqlErrorDefault{60};

	///@}

	//! Server settings containing its port, as well as allowed clients, origins, and actions.
	/*!
	 * These settings are read from the
	 *  configuration file.
	 */
	struct ServerSettings {
		///@name Properties
		///@{

		//! The port of the server as string.
		std::string port;

		//! A list of allowed IP addresses, separated by commas.
		std::string allowedClients;

		//! A list of allowed origins for CORS requests, separated by commas.
		std::string corsOrigins{"*"};

		//! Indicates whether the deletion of logs is allowed using the frontend, i.e. server commands.
		bool logsDeletable{false};

		//! Indicates whether the deletion of data is allowed using the frontend, i.e. server commands.
		bool dataDeletable{false};

		//! Number of seconds to wait if a MySQL error occures before retrying and possibly terminating the thread.
		std::uint16_t sleepOnSqlErrorS{serverSleepOnSqlErrorDefault};

		///@}
	};

} /* namespace crawlservpp::Struct */

#endif /* STRUCT_SERVERSETTINGS_HPP_ */
