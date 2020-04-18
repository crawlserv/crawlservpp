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
 * Network settings transferred from the server to threads using neworking, i.e. crawler and extractor
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

	struct NetworkSettings {
		std::string defaultProxy;		// default proxy (including port)

		std::string torControlServer;	// TOR control server
		std::uint16_t torControlPort;	// TOR control port
		std::string torControlPassword;	// TOR control password

		// constructor: create empty settings
		NetworkSettings() : torControlPort(0) {}
	};

} /* crawlservpp::Struct */

#endif /* STRUCT_NETWORKSETTINGS_HPP_ */
