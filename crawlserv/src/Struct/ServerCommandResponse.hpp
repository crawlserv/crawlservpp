/*
 * ServerCommandResponse.hpp
 *
 * Response from the command-and-control server.
 *
 *  Created on: May 4, 2019
 *      Author: ans
 */

#ifndef STRUCT_SERVERCOMMANDRESPONSE_HPP_
#define STRUCT_SERVERCOMMANDRESPONSE_HPP_

namespace crawlservpp::Struct {

	struct ServerCommandResponse {
		// constructor initializing a successful empty response
		ServerCommandResponse()
				: fail(false), confirm(false), id(0) {}

		// constructor initializing a successful response with text
		ServerCommandResponse(const std::string& response)
				: fail(false), confirm(false), text(response), id(0) {}

		// constructor initializing a successful response with text and ID
		ServerCommandResponse(const std::string& response, unsigned long newId)
				: fail(false), confirm(false), text(response), id(newId) {}

		// constructor initializing a possibly failed or possibly to be confirmed response with text
		ServerCommandResponse(bool failed, bool toBeConfirmed, const std::string& response)
				: fail(failed), confirm(toBeConfirmed), text(response), id(0) {}

		bool fail;			// command failed
		bool confirm;		// command needs to be confirmed
		std::string text;	// text of response
		unsigned long id;	// [can be used by the server to return an ID]

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
