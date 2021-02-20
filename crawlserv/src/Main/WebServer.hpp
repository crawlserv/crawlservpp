/*
 *
 * ---
 *
 *  Copyright (C) 2021 Anselm Schmidt (ans[ät]ohai.su)
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
#include "../Helper/Memory.hpp"
#include "../Helper/Strings.hpp"
#include "../Main/Exception.hpp"

extern "C" {
	#include "../_extern/mongoose/mongoose.h"
}

#include <algorithm>	// std::copy, std::swap, std::transform
#include <array>		// std::array
#include <cctype>		// std::tolower
#include <cstddef>		// std::size_t
#include <cstdint>		// std::uint16_t, std::uint64_t
#include <fstream>		// std::ofstream
#include <functional>	// std::function
#include <ios>			// std::ios_base
#include <stdexcept>	// std::logic_error
#include <string>		// std::stoull, std::string
#include <string_view>	// std::string_view, std::string_view_literals
#include <utility>		// std::pair
#include <vector>		// std::vector

namespace crawlservpp::Main {

	using std::string_view_literals::operator""sv;

	/*
	 * CONSTANTS
	 */

	///@name Constants
	///@{

	//! The address at which to listen for incoming connections.
	/*!
	 * The specified port will be appended.
	 */
	inline constexpr auto listenToAddress{"tcp://127.0.0.1:"sv};

	//! The name of a (lower-case) content type header.
	inline constexpr auto headerContentType{"content-type"sv};

	//! The name of a (lower-case) content size header.
	inline constexpr auto headerContentSize{"content-length"sv};

	//! The expected content type for HTTP multipart requests.
	inline constexpr auto headerContentTypeValue{"multipart/form-data"sv};

	//! The beginning of the header part that contains the boundary.
	inline constexpr auto headerBoundaryBegin{"boundary="sv};

	//! HTTP OK response code.
	inline constexpr auto httpOk{200};

	//! Required beginning of (lower-case) file part header.
	inline constexpr auto filePartHeaderBegin{"content-"sv};

	//! Required beginning of a HTTP multipart boundary.
	inline constexpr auto filePartBoundaryBegin{"--"sv};

	//! The end of the final HTTP multipart boundary.
	inline constexpr auto filePartBoundaryFinalEnd{"--"sv};

	//! The name of the upload header containing content information.
	inline constexpr auto filePartUploadHeader{"content-disposition"sv};

	//! The beginning of the field in the content information containing the name of the content.
	inline constexpr auto filePartUploadName{"name="sv};

	//! The name of the content containing the original name of the file to upload.
	inline constexpr auto filePartUploadFileName{"filename="sv};

	//! The (lower-case) name of the content containing file content to upload.
	inline constexpr auto filePartUploadField{"filetoupload"sv};

	//! The length of randomly generated file names.
	inline constexpr auto randFileNameLength{64};

	//! The length of two encapsulating quotes, in bytes.
	inline constexpr auto quotesLength{2};

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
		using ConstConnectionPtr = const mg_connection *;
		using StringString = std::pair<std::string, std::string>;

		using AcceptCallback =
				std::function<void(
						ConnectionPtr
				)>;
		using LogCallback = std::function<void(const std::string&)>;
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
		void setLogCallback(LogCallback callback);
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
		///@name Static Helper Function
		///@{

		static std::string getIP(ConstConnectionPtr connection);

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
		const std::string fileCache;
		mg_mgr eventManager{};
		std::string cors;
		bool isShutdown{false};

		// callback functions
		AcceptCallback onAccept;
		LogCallback onLog;
		RequestCallback onRequest;

		// event handlers
		static void eventHandler(
				ConnectionPtr connection,
				int event,
				void * data,
				void * arg
		);
		void eventHandlerInClass(
				ConnectionPtr connection,
				int event,
				void * data
		);
		void uploadHandler(ConnectionPtr connection, mg_http_message * msg);

		// internal helper functions
		std::string getCorsHeaders();
		void fileReceived(ConnectionPtr from, const std::string& name, const std::string& content);

		// static internal helper functions
		static bool parseHttpHeaders(
				const std::array<mg_http_header, MG_MAX_HTTP_HEADERS>& headers,
				std::string& boundaryTo,
				std::uint64_t& sizeTo
		);
		static bool getLine(struct mg_str& str, std::size_t& pos, std::string& to);
		static bool isBoundary(const std::string& line, const std::string& boundary);
		static bool isFinalBoundary(const std::string& line, const std::string& boundary);
		static bool getUploadHeaders(struct mg_str& str, std::size_t& pos, std::vector<StringString>& to);
		static bool getUploadHeader(const std::string& from, StringString& to);

		static bool parseContentType(
				const std::string& headerName,
				const struct mg_str& headerValue,
				std::string& boundaryTo,
				bool& isBoundaryFoundTo
		);
		static bool parseContentSize(
				const std::string& headerName,
				const struct mg_str& headerValue,
				std::uint64_t& sizeTo,
				bool& isFoundSizeTo
		);

		static bool parseContentTypeHeader(const std::string& value, std::string& boundaryTo);
		static bool parseUploadHeaders(const std::vector<StringString>& uploadHeaders, std::string& fileNameTo);
		static bool parseNextHeaderPart(const std::string& value, std::size_t& pos, std::string& to);

		static bool checkFileName(bool inFile, const std::string& currentFile, std::string& fileName);

		[[nodiscard]] static std::string toString(const struct mg_str& str);
		static void removeQuotes(std::string& str);
	};

} /* namespace crawlservpp::Main */

#endif /* MAIN_WEBSERVER_HPP_ */
