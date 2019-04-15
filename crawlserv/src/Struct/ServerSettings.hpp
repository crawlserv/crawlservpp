/*
 * ServerSettings.hpp
 *
 * Basic server settings (port, allowed clients, deletion of logs allowed, deletion of data allowed).
 *
 *  Created on: Oct 11, 2018
 *      Author: ans
 */

#ifndef STRUCT_SERVERSETTINGS_HPP_
#define STRUCT_SERVERSETTINGS_HPP_

#include <string>

namespace crawlservpp::Struct {

	struct ServerSettings {
		std::string port; // server port
		std::string allowedClients; // list of allowed IP addresses
		std::string corsOrigins; // allowed origins for CORS requests
		bool logsDeletable; // are logs deletable by frontend?
		bool dataDeletable; // is data deletable by frontend?

		// constructors
		ServerSettings() : corsOrigins("*"), logsDeletable(false), dataDeletable(false) {}
		ServerSettings(
				const std::string& setPort,
				const std::string& setAllowedClients,
				const std::string& setCorsOrigins,
				bool setLogsDeletable,
				bool setDataDeletable
		) : port(setPort),
			allowedClients(setAllowedClients),
			corsOrigins(setCorsOrigins),
			logsDeletable(setLogsDeletable),
			dataDeletable(setDataDeletable) {}
	};

} /* crawlservpp::Struct */

#endif /* STRUCT_SERVERSETTINGS_HPP_ */
