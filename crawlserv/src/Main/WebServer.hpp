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
 * WebServer.hpp
 *
 * Embedded web server using mongoose.
 *
 *  Created on: Feb 1, 2019
 *      Author: ans
 */

#ifndef MAIN_WEBSERVER_HPP_
#define MAIN_WEBSERVER_HPP_

#include "../Helper/FileSystem.hpp"
#include "../Helper/Strings.hpp"
#include "../Main/Exception.hpp"

#include "../_extern/mongoose/mongoose.h"

#include <algorithm>	// std::swap
#include <array>		// std::array
#include <cstdint>		// std::uint16_t
#include <cstdlib>		// std::malloc
#include <cstring>		// std::memcpy
#include <fstream>		// std::ios_base, std::ofstream
#include <functional>	// std::function
#include <string>		// std::string
#include <string_view>	// std::string_view

namespace crawlservpp::Main {

	/*
	 * CONSTANTS
	 */

	///@name Constants
	///@{

	//! The length of randomly generated file names.
	inline constexpr auto randFileNameLength{64};

	//! Name of the variable used for uploading the content of a file.
	inline constexpr std::string_view fileUploadVarName{"fileToUpload"};

	///@}

	/*
	 * DECLARATION
	 */

	//! Embedded web server class using the @c mongoose library.
	/*!
	 * For more information about the
	 *  @c mongoose library, see its
	 *  <a href="https://github.com/cesanta/mongoose">
	 *  GitHub repository</a>.
	 *
	 * \warning The web server does one final poll
	 *   on destruction. When used inside classes,
	 *   it should therefore declared last, i.e.
	 *   destroyed first, in case it uses other
	 *   member data when polled.
	 */
	class WebServer final {
		// for convenience
		using ConnectionPtr = mg_connection *;
		using AcceptCallback =
				std::function<void(
						ConnectionPtr
				)>;
		using RequestCallback =
				std::function<void(
						ConnectionPtr,
						const std::string&,
						const std::string&,
						void *
				)>;

	public:
		///@name Construction and Destruction
		///@{

		explicit WebServer(std::string_view fileCacheDirectory);
		virtual ~WebServer();

		///@}
		///@name @name Initialization
		///@{

		/*!
		 * \todo The HTTPS protocol is not supported yet.
		 */

		void initHTTP(const std::string& port);

		///@}
		///@name Setters
		///@{

		void setCORS(const std::string& allowed);
		void setAcceptCallback(AcceptCallback callback);
		void setRequestCallback(RequestCallback callback);

		///@}
		///@name Networking
		///@{

		void poll(int timeOut);
		void send(
				ConnectionPtr connection,
				uint16_t code,
				const std::string& type,
				const std::string& content
		);
		void sendFile(
				ConnectionPtr connection,
				const std::string& fileName,
				void * data
		);
		static void sendError(
				ConnectionPtr connection,
				const std::string& error
		);
		static void close(
				ConnectionPtr connection,
				bool immediately
		);

		///@}
		///@name Static Helper
		///@{

		static std::string getIP(ConnectionPtr connection);

		///@}

		//! Class for web server exceptions.
		MAIN_EXCEPTION_CLASS();

		/**@name Copy and Move
		 * The class is neither copyable, nor movable.
		 */
		///@{

		//! Deleted copy constructor.
		WebServer(WebServer&) = delete;

		//! Deleted copy assignment operator.
		WebServer& operator=(WebServer&) = delete;

		//! Deleted move constructor.
		WebServer(WebServer&&) = delete;

		//! Deleted move assignment operator.
		WebServer& operator=(WebServer&&) = delete;

		///@}

	private:
		const std::string_view fileCache;

		mg_mgr eventManager;
		std::string cors;
		std::string oldFileName;

		// callback functions
		AcceptCallback onAccept;
		RequestCallback onRequest;

		// event handlers
		static void eventHandler(
				ConnectionPtr connection,
				int event,
				void * data
		);
		void eventHandlerInClass(
				ConnectionPtr connection,
				int event,
				void * data
		);

		// private helper functions
		std::string getCorsHeaders();
		mg_str generateFileNameInClass();
		static mg_str generateFileName(
				ConnectionPtr connection,
				mg_str fileName
		);

		// private struct for user data while uploading files
		struct FileUploadData {
			std::string name;
			std::ofstream stream;

			FileUploadData(
					const std::string& fileName,
					const std::string& fullFileName
			) : name(fileName) {
				this->stream.open(
						fullFileName.c_str(),
						std::ios_base::binary
				);
			}
		};
	};

} /* namespace crawlservpp::Main */

#endif /* MAIN_WEBSERVER_HPP_ */
