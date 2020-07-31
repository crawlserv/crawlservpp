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
 * NetworkSettings.hpp
 *
 * Network settings transferred from the server to threads using networking, i.e. crawler and extractor
 *  (default proxy, TOR control server, port and password).
 *
 *  Created on: Oct 13, 2018
 *      Author: ans
 */

#ifndef STRUCT_NETWORKSETTINGS_HPP_
#define STRUCT_NETWORKSETTINGS_HPP_

#include <cstdint>	// std::uint16_t
#include <string>	// std::string

namespace crawlservpp::Struct {

	//! %Network settings containing the default proxy as well as host, port, and password of the TOR control server.
	/*!
	 * These settings are read from the
	 *  configuration file.
	 *
	 * They will be transferred from the
	 *  server to threads that use networking,
	 *  i.e. crawler and extractor threads.
	 */
	struct NetworkSettings {
		///@name Properties
		///@{

		//! The host name and the port of the default proxy to be used.
		std::string defaultProxy;

		//! The host name of the TOR control server.
		std::string torControlServer;

		//! The port used by the TOR control server.
		std::uint16_t torControlPort{0};

		//! The password used by the TOR control server.
		std::string torControlPassword;
	};

} /* namespace crawlservpp::Struct */

#endif /* STRUCT_NETWORKSETTINGS_HPP_ */
