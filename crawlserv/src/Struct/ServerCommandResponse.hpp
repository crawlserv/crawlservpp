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
 * ServerCommandResponse.hpp
 *
 * Response from the command-and-control server.
 *
 *  Created on: May 4, 2019
 *      Author: ans
 */

#ifndef STRUCT_SERVERCOMMANDRESPONSE_HPP_
#define STRUCT_SERVERCOMMANDRESPONSE_HPP_

#include <cstdint>	// std::uint64_t
#include <string>	// std::string

namespace crawlservpp::Struct {

	struct ServerCommandResponse {
		// constructor initializing a successful empty response
		ServerCommandResponse()
				: fail(false), confirm(false), id(0) {}

		// constructor initializing a successful response with text
		ServerCommandResponse(const std::string& response)
				: fail(false), confirm(false), text(response), id(0) {}

		// constructor initializing a successful response with text and ID
		ServerCommandResponse(const std::string& response, std::uint64_t newId)
				: fail(false), confirm(false), text(response), id(newId) {}

		// constructor initializing a possibly failed or possibly to be confirmed response with text
		ServerCommandResponse(bool failed, bool toBeConfirmed, const std::string& response)
				: fail(failed), confirm(toBeConfirmed), text(response), id(0) {}

		bool fail;			// command failed
		bool confirm;		// command needs to be confirmed
		std::string text;	// text of response
		std::uint64_t id;	// [can be used by the server to return an ID]

		// helper to initialize a failed response with text
		static ServerCommandResponse failed(const std::string& text) {
			return ServerCommandResponse(true, false, text);
		}

		// helper to initialize a to be confirmed response with text
		static ServerCommandResponse toBeConfirmed(const std::string& text) {
			return ServerCommandResponse(false, true, text);
		}
	};


} /* crawlservpp::Struct */

#endif /* STRUCT_SERVERCOMMANDRESPONSE_HPP_ */
