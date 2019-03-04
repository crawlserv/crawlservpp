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

		// constructors
		ServerSettings() { this->logsDeletable = false; this->dataDeletable = false; }
		ServerSettings(const std::string& setPort, const std::string& setAllowedClients, bool setLogsDeletable, bool setDataDeletable)
				: port(setPort), allowedClients(setAllowedClients), logsDeletable(setLogsDeletable), dataDeletable(setDataDeletable) {}
	};
}

#endif /* STRUCT_SERVERSETTINGS_H_ */
