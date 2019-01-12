/*
 * ServerSettings.h
 *
 * Basic server settings (port, allowed clients, deletion of logs allowed, deletion of data allowed).
 *
 *  Created on: Oct 11, 2018
 *      Author: ans
 */

#ifndef STRUCT_SERVERSETTINGS_H_
#define STRUCT_SERVERSETTINGS_H_

#include <string>

namespace crawlservpp::Struct {
	struct ServerSettings {
		std::string port; // server port
		std::string allowedClients; // list of allowed IP addresses
		bool logsDeletable; // are logs deletable by frontend?
		bool dataDeletable; // is data deletable by frontend?
	};
}

#endif /* STRUCT_SERVERSETTINGS_H_ */
