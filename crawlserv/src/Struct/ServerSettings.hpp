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
 * Basic server settings (port, allowed clients, deletion of logs allowed, deletion of data allowed).
 *
 *  Created on: Oct 11, 2018
 *      Author: ans
 */

#ifndef STRUCT_SERVERSETTINGS_HPP_
#define STRUCT_SERVERSETTINGS_HPP_

#include <string>	// std::string

namespace crawlservpp::Struct {

	struct ServerSettings {
		std::string port; 			// server port
		std::string allowedClients; // list of allowed IP addresses
		std::string corsOrigins; 	// allowed origins for CORS requests
		bool logsDeletable; 		// are logs deletable by frontend?
		bool dataDeletable; 		// is data deletable by frontend?

		// constructors
		ServerSettings() : corsOrigins("*"), logsDeletable(false), dataDeletable(false) {}
	};

} /* crawlservpp::Struct */

#endif /* STRUCT_SERVERSETTINGS_HPP_ */
