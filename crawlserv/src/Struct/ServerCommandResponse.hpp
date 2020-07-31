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

	//! Response from the command-and-control server.
	struct ServerCommandResponse {
		///@name Properties
		///@{

		//! Indicates whether the server command failed.
		bool fail{false};

		//! Indicates whether the server command needs to be confirmed.
		bool confirm{false};

		//! The text of the response by the server
		std::string text;

		//! Optional ID returned by the server.
		std::uint64_t id{0};

		///@}
		///@name Construction
		///@{

		//! Default constructor.
		ServerCommandResponse() = default;

		//! Constructor initializing a successful response with text.
		/*!
		 * \param response Constant reference to
		 *   a string containing the text of the
		 *   server response.
		 */
		explicit ServerCommandResponse(const std::string& response)
				: text(response) {}

		//! Constructor initializing a successful response with text and ID.
		/*!
		 * \param response Constant reference to
		 *   a string containing the text of the
		 *   response by the server.
		 * \param newId The ID to be sent alongside
		 *   the response by the server.
		 */
		ServerCommandResponse(const std::string& response, std::uint64_t newId)
				: text(response), id(newId) {}

		//! Constructor initializing a possibly failed or possibly to be confirmed response with text.
		/*!
		 * \param failed Set whether the server
		 *   command failed.
		 * \param toBeConfirmed Set whether the
		 *   server command needs to be confirmed.
		 * \param response Constant reference to
		 *   a string containing the text of the
		 *   response by the server.
		 */
		ServerCommandResponse(bool failed, bool toBeConfirmed, const std::string& response)
				: fail(failed), confirm(toBeConfirmed), text(response) {}

		///@}
		///@name Helpers
		///@{

		//! Helper to initialize a "failed" response with text.
		/*!
		 * \param response Constant reference to
		 *   a string containing the text of the
		 *   response by the server.
		 */
		static ServerCommandResponse failed(const std::string& response) {
			return ServerCommandResponse(true, false, response);
		}

		//! Helper to initialize a "to be confirmed" response with text.
		/*!
		 * \param response Constant reference to
		 *   a string containing the text of the
		 *   response by the server.
		 */
		static ServerCommandResponse toBeConfirmed(const std::string& response) {
			return ServerCommandResponse(false, true, response);
		}

		///@}
	};

} /* namespace crawlservpp::Struct */

#endif /* STRUCT_SERVERCOMMANDRESPONSE_HPP_ */
