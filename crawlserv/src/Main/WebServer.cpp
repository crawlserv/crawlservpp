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
 * WebServer.cpp
 *
 * Embedded web server using mongoose.
 *
 *  NOTE:	The web server does one final poll on destruction.
 *  		When used inside classes it should therefore declared last/destroyed first, in case it uses other member data.
 *
 *  Created on: Feb 1, 2019
 *      Author: ans
 */

#include "WebServer.hpp"

namespace crawlservpp::Main {

	/*
	 * CONSTRUCTION AND DESTRUCTION
	 */

	//! Constructor setting the file cache and initializing the web server.
	/*!
	 * \param fileCacheDirectory View of the
	 *   directory in which to temporarily
	 *   save files uploaded to the server.
	 */
	WebServer::WebServer(std::string_view fileCacheDirectory)
	: fileCache(fileCacheDirectory) {
		mg_mgr_init(&(this->eventManager));
	}

	//! Destructor freeing the web server.
	/*!
	 * \warning The web server does one final poll
	 *   on destruction. When used inside classes,
	 *   it should therefore declared last, i.e.
	 *   destroyed first, in case it uses other
	 *   member data when polled.
	 */
	WebServer::~WebServer() {
		this->isShutdown = true;

		mg_mgr_free(&(this->eventManager));
	}

	/*
	 * INITIALIZATION
	 */

	//! Initializes the web server for usage over HTTP.
	/*!
	 * Binds the web server to its port, sets user
	 *  data and the protocol.
	 *
	 * \param port Constant reference to a string
	 *   containing the port to be used by the web
	 *   server.
	 *
	 * \throws WebServer::Exception if the server could
	 *   not be bound to the given port.
	 */
	void WebServer::initHTTP(const std::string& port) {
		std::string listenTo{listenToAddress};

		listenTo += port;

		ConnectionPtr connection{
			mg_http_listen(
					&(this->eventManager),
					listenTo.c_str(),
					WebServer::eventHandler,
					static_cast<void *>(this)
			)
		};

		if(connection == nullptr) {
			throw Exception(
					"WebServer::initHTTP():"
					" Could not bind server to port "
					+ port
			);
		}
	}

	/*
	 * SETTERS
	 */

	//! Configures cross-origin resource sharing (CORS).
	/*!
	 * \param allowed Constant reference to a string
	 *   containing the origins from which access is
	 *   allowed via CORS.
	 */
	void WebServer::setCORS(const std::string& allowed) {
		this->cors = allowed;
	}

	//! Sets callback function for accepted connections.
	/*!
	 * \param callback A callback function that will
	 *   be called when a connection got accepted.
	 */
	void WebServer::setAcceptCallback(AcceptCallback callback) {
		std::swap(this->onAccept, callback);
	}

	//! Sets callback function for logging.
	/*!
	 * \param callback A callback function that will
	 *   be called when the web server wants to log
	 *   something.
	 */
	void WebServer::setLogCallback(LogCallback callback) {
		std::swap(this->onLog, callback);
	}

	//! Sets callback function for HTTP requests.
	/*!
	 * \param callback A callback function that will
	 *   be called when a HTTP request has been
	 *   received.
	 */
	void WebServer::setRequestCallback(RequestCallback callback) {
		std::swap(this->onRequest, callback);
	}

	/*
	 * NETWORKING
	 */

	//! Polls the web server.
	/*!
	 * \param timeOut The number of milliseconds
	 *   after which polling the web server times
	 *   out.
	 */
	void WebServer::poll(int timeOut) {
		mg_mgr_poll(&(this->eventManager), timeOut);
	}

	//! Sends a HTTP reply to a previously established connection.
	/*!
	 * \param connection Pointer to the connection
	 *   which will be used for sending the HTTP
	 *   reply.
	 * \param code HTTP status code to be send at
	 *   the beginning of the response.
	 * \param type Constant reference to a string
	 *   containing the content type to be sent.
	 * \param content Constant reference to a
	 *   string containing the content to be sent.
	 *
	 * \throws WebServer::Connection if no
	 *   connection has been specified, i.e.
	 *   the pointer to the connection is
	 *   @c nullptr.
	 */
	void WebServer::send(
			ConnectionPtr connection,
			std::uint16_t code,
			const std::string& type,
			const std::string& content
	) {
		// check connection
		if(connection == nullptr) {
			throw Exception(
					"WebServer::send():"
					" No connection has been specified"
			);
		}

		// send reply
		std::string headers;

		if(!type.empty()) {
			headers += "Content-Type: " + type + "\r\n";
		}

		headers += this->getCorsHeaders();

		//NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg, hicpp-vararg)
		mg_http_reply(connection, code, headers.c_str(), "%s", content.c_str());
	}

	//! Sends a file located in the file cache.
	/*!
	 * The file might be a relative path, but must
	 *  be located in the file cache. If the path
	 *  to the file is invalid, an internal server
	 *  error (HTTP code 500) will be sent instead
	 *  of the file.
	 *
	 * Files can only be sent in response to a
	 *  HTTP message received by the callback
	 *  function set via setRequestCallback.
	 *
	 * \param connection Pointer to the connection
	 *   which will be used for sending the file.
	 * \param fileName Constant reference to a
	 *   string containing name of the file to be
	 *   sent.
	 * \param data Pointer to the HTTP message
	 *   requesting the file, as retrieved by the
	 *   callback function set via
	 *   setRequestCallback.
	 *
	 * \throws WebServer::Connection if no connection
	 *   has been specified, i.e. the pointer to the
	 *   connection is @c nullptr.
	 *
	 * \sa setRequestCallback
	 */
	void WebServer::sendFile(
			ConnectionPtr connection,
			const std::string& fileName,
			void * data
	) {
		// check arguments
		if(connection == nullptr) {
			throw Exception(
					"WebServer::sendFile():"
					" No connection has been specified"
			);
		}

		std::string fullFileName;

		fullFileName.reserve(fullFileName.length() + fileName.length() + 1);

		fullFileName = this->fileCache;

		fullFileName += Helper::FileSystem::getPathSeparator();
		fullFileName += fileName;

		try {
			if(!Helper::FileSystem::contains(this->fileCache, fullFileName)) {
				WebServer::sendError(connection, "Invalid file name");

				return;
			}
		}
		catch(const Helper::FileSystem::Exception& e) {
			throw Exception(
					"WebServer::sendFile(): "
					+ std::string(e.view())
			);
		}

		// add CORS headers
		std::string headers{this->getCorsHeaders()};

		// serve file from file cache
		mg_http_serve_file(
				connection,
				static_cast<mg_http_message *>(data),
				fullFileName.c_str(),
				"application/octet-stream",
				headers.c_str()
		);

		this->onLog(
				"sent '"
				+ fullFileName
				+ "' to "
				+ WebServer::getIP(connection)
				+ "."
		);
	}

	//! Sends an internal server error (HTTP code 500) with a custom message and closes the connection.
	/*!
	 * \param connection Pointer to the connection
	 *   which will be used for sending the error
	 *   and which will closed afterwards.
	 * \param error Constant reference to a string
	 *   containing the custom error message sent
	 *   via the given connection before closing it.
	 *
	 * \throws WebServer::Connection if no connection
	 *   has been specified, i.e. the pointer to the
	 *   connection is @c nullptr.
	 */
	void WebServer::sendError(ConnectionPtr connection, const std::string& error) {
		// check for connection
		if(connection == nullptr) {
			throw Exception(
					"WebServer::sendError():"
					" No connection has been specified"
			);
		}

		const std::string responseStr(
				"HTTP/1.1 500 "
				+ error + "\r\n"
				"Content-Length: 0\r\n\r\n"
		);

		//NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg, hicpp-vararg)
		mg_printf(
				connection,
				"%s",
				responseStr.c_str()
		);

		// close connection
		WebServer::close(connection, true);
	}

	//! Closes a connection immediately.
	/*!
	 * \param connection Pointer to the connection
	 *   that will closed immediately.
	 * \param immediately Set to true to close the
	 *   connection immediately, without sending
	 *   any remaining data.
	 *
	 * \throws WebServer::Connection if no connection
	 *  has been specified, i.e. the pointer to the
	 *  connection is @c nullptr.
	 */
	void WebServer::close(
			ConnectionPtr connection,
			bool immediately
	) {
		// check for connection
		if(connection == nullptr) {
			throw Exception(
					"WebServer::close():"
					" No connection has been specified"
			);
		}

		// set closing flag
		connection->is_closing = immediately;
		connection->is_draining = !immediately;
	}

	/*
	 * STATIC HELPER FUNCTION
	 */

	//! Static helper function retrieving the client IP from a connection.
	/*!
	 * \param connection Pointer to the connection
	 *   from which to retrieve the IP address of
	 *   the connected client.
	 *
	 * \returns The IP address of the connected
	 *   client as string.
	 *
	 * \throws WebServer::Connection if no connection
	 *  has been specified, i.e. the pointer to the
	 *  connection is @c nullptr.
	 */
	std::string WebServer::getIP(ConstConnectionPtr connection) {
		// check for connection
		if(connection == nullptr) {
			throw Exception(
					"WebServer::getIP():"
					" No connection has been specified"
			);
		}

		std::array<char, INET6_ADDRSTRLEN> ip{};

		mg_ntoa(&(connection->peer), ip.data(), INET6_ADDRSTRLEN);

		return std::string{ip.data()};
	}

	/*
	 * EVENT HANDLERS (private)
	 */

	// static event handler, throws WebServer::Exception
	void WebServer::eventHandler(ConnectionPtr connection, int event, void * data, void * arg) {
		// check argument
		if(connection == nullptr) {
			throw Exception(
					"WebServer::eventHandler():"
					" No connection has been specified"
			);
		}

		if(arg == nullptr) {
			throw Exception(
					"WebServer::eventHandler():"
					" No WebServer instance has been specified"
			);
		}

		static_cast<WebServer *>(arg)->eventHandlerInClass(
				connection,
				event,
				data
		);
	}

	// event handler (in-class)
	void WebServer::eventHandlerInClass(ConnectionPtr connection, int event, void * data) {
		// check for shutdown
		if(this->isShutdown) {
			return;
		}

		// handle event
		mg_http_message * httpMessage{nullptr};
		std::string method;
		std::string body;

		switch(event) {
		case MG_EV_ACCEPT:
			// handle accept event
			this->onAccept(connection);

			break;

		case MG_EV_HTTP_MSG:
			httpMessage = static_cast<mg_http_message *>(data);

			if(mg_http_match_uri(httpMessage, "/upload")) {
				// handle file upload
				this->uploadHandler(connection, httpMessage);
			}
			else {
				// handle request
				method = WebServer::toString(httpMessage->method);
				body = WebServer::toString(httpMessage->body);

				this->onRequest(connection, method, body, data);
			}

			break;

		default:
			// ignore unknown event
			break;
		}
	}

	// multi-part upload handler
	void WebServer::uploadHandler(ConnectionPtr connection, mg_http_message * msg) {
		std::size_t pos{};
		std::string boundary;
		std::uint64_t size{};
		std::string line;
		std::vector<StringString> uploadHeaders;
		std::array<mg_http_header, MG_MAX_HTTP_HEADERS> headers{};

		std::copy(std::begin(msg->headers), std::end(msg->headers), headers.begin());

		if(
				WebServer::parseHttpHeaders(headers, boundary, size)
				&& WebServer::getLine(msg->body, pos, line) /* first line of content */
		) {
			if(size > MG_MAX_RECV_BUF_SIZE) {
				WebServer::sendError(connection, "Data too large");

				this->onLog(
						"received too large data from "
						+ WebServer::getIP(connection)
				);
			}

			std::string originFile;
			std::string content;

			while(
					WebServer::isBoundary(line, boundary)
					&& WebServer::getUploadHeaders(msg->body, pos, uploadHeaders)
			) {
				std::string fileName;
				bool inFile{parseUploadHeaders(uploadHeaders, fileName)};

				if(!WebServer::checkFileName(inFile, fileName, originFile)) {
					WebServer::sendError(connection, "Cannot send unnamed or multiple files");

					this->onLog(
							"recevied unnamed or multiple file(s) from "
							+ WebServer::getIP(connection)
							+ " (not supported)."
					);

					return;
				}

				while(
						WebServer::getLine(msg->body, pos, line)
						&& !WebServer::isBoundary(line, boundary)
						&& !WebServer::isFinalBoundary(line , boundary)
				) {
					if(inFile) {
						content += line + "\n";
					}
				}
			}

			if(!content.empty()) {
				// remove last newline
				content.pop_back();
			}

			// get whether finished
			if(WebServer::isFinalBoundary(line , boundary)) {
				this->fileReceived(connection, originFile, content);
			}
			else {
				WebServer::sendError(connection, "Incomplete data");

				this->onLog(
						"received incomplete data from "
						+ WebServer::getIP(connection)
				);
			}
		}
		else {
			WebServer::sendError(connection, "Misformed upload data");

			this->onLog(
					"received misformed data from "
					+ WebServer::getIP(connection)
			);
		}
	}

	/*
	 * INTERNAL HELPER FUNCTIONS (private)
	 */

	// generate CORS headers
	std::string WebServer::getCorsHeaders() {
		return	"Access-Control-Allow-Origin: " + this->cors + "\r\n"
				"Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
				"Access-Control-Allow-Headers: Content-Type\r\n";
	}

	// save received file and send new file name
	void WebServer::fileReceived(ConnectionPtr from, const std::string& name, const std::string& content) {
		std::string newName;
		std::string path;

		// create new file name
		do {
			newName = Helper::Strings::generateRandom(randFileNameLength);
			path = this->fileCache + Helper::FileSystem::getPathSeparator() + newName;
		} while(Helper::FileSystem::exists(path));

		// save file
		std::ofstream out(path.c_str(), std::ios_base::binary);

		if(!out.is_open()) {
			WebServer::sendError(from, "Could not create file on server");

			this->onLog("failed to create '" + path + "'.");

			return;
		}

		out << content;

		out.close();

		// send new file name in reply
		//NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg, hicpp-vararg)
		mg_http_reply(from, httpOk, this->getCorsHeaders().c_str(), newName.c_str());

		this->onLog(
				"received '"
				+ name
				+ "' from "
				+ WebServer::getIP(from)
				+ ", saved as '"
				+ newName
				+ "'."
		);
	}

	/*
	 * STATIC INTERNAL HELPER FUNCTIONS (private)
	 */

	// get boundary from HTTP request headers
	bool WebServer::parseHttpHeaders(
			const std::array<mg_http_header, MG_MAX_HTTP_HEADERS>& headers,
			std::string& boundaryTo,
			std::uint64_t& sizeTo
	) {
		bool isFoundBoundary{false};
		bool isFoundSize{false};

		for(std::size_t index{}; index < MG_MAX_HTTP_HEADERS; ++index) {
			if(headers[index].name.ptr == nullptr) {
				break;
			}

			std::string headerName{WebServer::toString(headers[index].name)};

			std::transform(
					headerName.begin(),
					headerName.end(),
					headerName.begin(),
					[](const auto c)  {
						return std::tolower(c);
					}
			);

			if(
					!parseContentType(
							headerName,
							headers[index].value,
							boundaryTo,
							isFoundBoundary
					)
					|| !parseContentSize(
							headerName,
							headers[index].value,
							sizeTo,
							isFoundSize
					)
			) {
				return false;
			}
		}

		return isFoundBoundary && isFoundSize;
	}

	// get line from mongoose string, update position, return false if end has been reached
	bool WebServer::getLine(struct mg_str& str, std::size_t& pos, std::string& to) {
		if(pos >= str.len) {
			return false;
		}

		std::size_t end{pos};

		for(; end < str.len; ++end) {
			if(str.ptr[end] == '\n') { //NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
				break;
			}
		}

		std::string{}.swap(to);

		to.reserve(end - pos);

		for(std::size_t index{pos}; index < end; ++index) {
			to.push_back(str.ptr[index]); //NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
		}

		if(!to.empty() && to.back() == '\r') {
			to.pop_back();
		}

		pos = end + 1;

		return true;
	}

	// check whether the line indicates a boundary
	bool WebServer::isBoundary(const std::string& line, const std::string& boundary) {
		return line.substr(0, filePartBoundaryBegin.size()) == filePartBoundaryBegin
				&& line.substr(filePartBoundaryBegin.size()) == boundary;
	}

	// check whether the line indicates the final boundary
	bool WebServer::isFinalBoundary(const std::string& line, const std::string& boundary) {
		return line.substr(0, filePartBoundaryBegin.size())
				== filePartBoundaryBegin
				&& line.substr(filePartBoundaryBegin.size(), boundary.size())
				== boundary
				&& line.substr(filePartBoundaryBegin.size() + boundary.size())
				== filePartBoundaryFinalEnd;
	}

	// get headers of uploaded file part
	bool WebServer::getUploadHeaders(
			struct mg_str& str,
			std::size_t& pos,
			std::vector<StringString>& to
	) {
		std::string line;

		while(WebServer::getLine(str, pos, line)) {
			if(line.empty()) {
				return true;
			}

			StringString header;

			if(!WebServer::getUploadHeader(line, header)) {
				return false;
			}

			to.emplace_back(header);
		}

		return false;
	}

	// get single header of uploaded file part
	bool WebServer::getUploadHeader(const std::string& from, StringString& to) {
		std::string lower(from.size(), {});

		std::transform(from.begin(), from.end(), lower.begin(), [](const auto c) {
			return std::tolower(c);
		});

		if(lower.substr(0, filePartHeaderBegin.size()) != filePartHeaderBegin) {
			return false;
		}

		const auto colon{lower.find(':')};

		if(colon == std::string::npos) {
			return false;
		}

		std::string value{lower.substr(colon + 1)};

		Helper::Strings::trim(value);

		to = { lower.substr(0, colon), value };

		return true;
	}

	// check for content type header of multipart HTTP requests, return false on misformed header
	bool WebServer::parseContentType(
			const std::string& headerName,
			const struct mg_str& headerValue,
			std::string& boundaryTo,
			bool& isBoundaryFoundTo
	) {
		if(headerName != headerContentType) {
			return true;
		}

		if(parseContentTypeHeader(WebServer::toString(headerValue), boundaryTo)) {
			isBoundaryFoundTo = true;

			return true;
		}

		return false;
	}

	// check for content size header of multipart HTTP requests, return false on misformed header
	bool WebServer::parseContentSize(
			const std::string& headerName,
			const struct mg_str& headerValue,
			std::uint64_t& sizeTo,
			bool& isFoundSizeTo
	) {
		if(headerName != headerContentSize) {
			return true;
		}

		try {
			sizeTo = std::stoull(WebServer::toString(headerValue));

			isFoundSizeTo = true;

			return true;
		}
		catch(const std::logic_error& e) {}

		return false;
	}

	// parse content type header for multipart HTTP requests
	bool WebServer::parseContentTypeHeader(const std::string& value, std::string& boundaryTo) {
		std::size_t pos{value.find(';')};

		if(pos == std::string::npos) {
			return false;
		}

		if(value.substr(0, pos) != headerContentTypeValue) {
			return false;
		}

		++pos;

		std::string headerPart;

		while(parseNextHeaderPart(value, pos, headerPart)) {
			if(headerPart.substr(0, headerBoundaryBegin.length()) == headerBoundaryBegin) {
				boundaryTo = headerPart.substr(headerBoundaryBegin.length());

				return true;
			}
		}

		return false;
	}

	// parse the upload headers, return whether their part contains content of the file to upload
	bool WebServer::parseUploadHeaders(
			const std::vector<StringString>& uploadHeaders,
			std::string& fileNameTo
	) {
		for(const auto& header : uploadHeaders) {
			if(header.first == filePartUploadHeader) {
				std::size_t pos{};
				std::string headerPart;
				std::string name;
				std::string fileName;

				while(
						WebServer::parseNextHeaderPart(
								header.second,
								pos,
								headerPart
						)
				) {
					if(
							headerPart.substr(0, filePartUploadName.length())
							== filePartUploadName
					) {
						name = headerPart.substr(filePartUploadName.length());
					}

					if(
							headerPart.substr(0, filePartUploadFileName.length())
							== filePartUploadFileName
					) {
						fileName = headerPart.substr(filePartUploadFileName.length());
					}
				}

				WebServer::removeQuotes(name);
				WebServer::removeQuotes(fileName);

				if(name == filePartUploadField) {
					fileNameTo = fileName;

					return true;
				}
			}
		}

		return false;
	}

	// parse next header part, return whether it exists
	bool WebServer::parseNextHeaderPart(
			const std::string& value,
			std::size_t& pos, std::string& to
	) {
		if(pos >= value.length()) {
			return false;
		}

		const std::size_t end{value.find(';', pos)};

		if(end == std::string::npos) {
			to = value.substr(pos);
			pos = value.length();
		}
		else {
			to = value.substr(pos, end - pos);
			pos = end + 1;
		}

		Helper::Strings::trim(to);

		return true;
	}

	// check file name, return false if multiple files are transferred
	bool WebServer::checkFileName(
			bool inFile,
			const std::string& currentFile,
			std::string& fileName
	) {
		if(inFile) {
			if(currentFile.empty()) {
				return false;
			}

			if(fileName.empty()) {
				fileName = currentFile;
			}
			else if(currentFile != fileName) {
				return false;
			}
		}

		return true;
	}

	// convert mongoose string to C++ string
	std::string WebServer::toString(const struct mg_str& str) {
		if(str.ptr == nullptr) {
			return std::string{};
		}

		return std::string(str.ptr, str.len);
	}

	// remove quotes around string, if applicable
	void WebServer::removeQuotes(std::string& str) {
		if(str.length() < quotesLength) {
			return;
		}

		switch(str[0]) {
		case '\'':
			if(str.back() != '\'') {
				return;
			}

			break;

		case '"':
			if(str.back() != '"') {
				return;
			}

			break;

		default:
			return;
		}

		// remove quotes
		str.pop_back();
		str.erase(0, 1);
	}

} /* namespace crawlservpp::Main */
