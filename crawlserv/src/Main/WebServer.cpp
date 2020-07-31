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

	//! Constructor setting the file cache and initializing the web server.
	/*!
	 * \param fileCacheDirectory View of the
	 *   directory in which to temporarily
	 *   save files uploaded to the server.
	 */
	WebServer::WebServer(std::string_view fileCacheDirectory)
			: fileCache(fileCacheDirectory),
			  eventManager{}
	{
		mg_mgr_init(&(this->eventManager), nullptr);
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
		mg_mgr_free(&(this->eventManager));
	}

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
		ConnectionPtr connection{
			mg_bind(
					&(this->eventManager),
					port.c_str(),
					WebServer::eventHandler
			)
		};

		if(connection == nullptr) {
			throw Exception(
					"WebServer::initHTTP():"
					" Could not bind server to port "
					+ port
			);
		}

		// set user data (i.e. pointer to class)
		connection->user_data = this;

		// set protocol
		mg_set_protocol_http_websocket(connection);
	}

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

	//! Sets callback function for HTTP requests.
	/*!
	 * \param callback A callback function that will
	 *   be called when a HTTP request has been
	 *   received.
	 */
	void WebServer::setRequestCallback(RequestCallback callback) {
		std::swap(this->onRequest, callback);
	}

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
		std::string head;

		if(!type.empty()) {
			head += "Content-Type: " + type + "\r\n";
		}

		head += this->getCorsHeaders();

		mg_send_head(connection, code, content.size(), head.c_str());

		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg, hicpp-vararg)
		mg_printf(connection, "%s", content.c_str());
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

		if(!Helper::FileSystem::contains(this->fileCache, fullFileName)) {
			WebServer::sendError(connection, "Invalid file name");

			return;
		}

		// add CORS headers
		std::string headers(this->getCorsHeaders());

		// add attachment header
		headers += "\r\nContent-Disposition: attachment; filename=\"" + fileName + "\"";

		// serve file from file cache
		mg_http_serve_file(
				connection,
				static_cast<http_message *>(data),
				fullFileName.c_str(),
				mg_mk_str("application/octet-stream"),
				mg_mk_str_n(headers.c_str(), headers.size())
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

		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg, hicpp-vararg)
		mg_printf(
				connection,
				"%s",
				responseStr.c_str()
		);

		// set closing flag
		WebServer::close(connection);
	}

	//! Closes a connection immediately.
	/*!
	 * \param connection Pointer to the connection
	 *   that will closed immediately.
	 *
	 * \throws WebServer::Connection if no connection
	 *  has been specified, i.e. the pointer to the
	 *  connection is @c nullptr.
	 */
	void WebServer::close(ConnectionPtr connection) {
		// check for connection
		if(connection == nullptr) {
			throw Exception(
					"WebServer::close():"
					" No connection has been specified"
			);
		}

		// set closing flag
		// NOLINTNEXTLINE(hicpp-signed-bitwise)
		connection->flags |= MG_F_CLOSE_IMMEDIATELY;
	}

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
	std::string WebServer::getIP(ConnectionPtr connection) {
		// check for connection
		if(connection == nullptr) {
			throw Exception(
					"WebServer::getIP():"
					" No connection has been specified"
			);
		}

		std::array<char, INET6_ADDRSTRLEN> ip{};

		mg_sock_to_str(
				connection->sock,
				ip.data(),
				INET6_ADDRSTRLEN,
				// NOLINTNEXTLINE(hicpp-signed-bitwise)
				MG_SOCK_STRINGIFY_REMOTE | MG_SOCK_STRINGIFY_IP
		);

		return std::string{ip.data()};
	}

	// static event handler, throws WebServer::Exception
	void WebServer::eventHandler(ConnectionPtr connection, int event, void * data) {
		// check argument
		if(connection == nullptr) {
			throw Exception(
					"WebServer::eventHandler():"
					" No connection has been specified"
			);
		}

		if(connection->user_data == nullptr) {
			throw Exception(
					"WebServer::eventHandler():"
					" No WebServer instance has been specified"
			);
		}

		static_cast<WebServer *>(connection->user_data)->eventHandlerInClass(
				connection,
				event,
				data
		);
	}

	// event handler (in-class)
	void WebServer::eventHandlerInClass(ConnectionPtr connection, int event, void * data) {
		http_message * httpMessage{nullptr};
		std::string method;
		std::string body;

		switch(event) {
		case MG_EV_ACCEPT:
			// handle accept event
			this->onAccept(connection);

			break;

		case MG_EV_HTTP_REQUEST:
			// handle request event
			httpMessage = static_cast<http_message *>(data);
			method = std::string(httpMessage->method.p, httpMessage->method.len);
			body = std::string(httpMessage->body.p, httpMessage->body.len);

			this->onRequest(connection, method, body, data);

			break;

		case MG_EV_HTTP_PART_BEGIN:
		case MG_EV_HTTP_PART_DATA:
		case MG_EV_HTTP_PART_END:
			// reset file name before upload begins
			if(event == MG_EV_HTTP_PART_BEGIN) {
				this->oldFileName = "";
			}

			// handle file upload
			if(
					data != nullptr
					&& static_cast<mg_http_multipart_part *>(data)->var_name != nullptr
					&& strncmp(
							static_cast<mg_http_multipart_part *>(data)->var_name,
							fileUploadVarName.data(),
							fileUploadVarName.length()
					) == 0
			) {
				mg_file_upload_handler(
						connection,
						event,
						data,
						WebServer::generateFileName
				);
			}

			if(event == MG_EV_HTTP_PART_END) {
				// file upload finished: return name on server as plain text
				// NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg, hicpp-vararg)
				mg_printf(
						connection,
						"HTTP/1.1 200 OK\r\n"
						"Content-Type: text/plain\r\n"
						"Connection: close\r\n"
						"%sr\n\r\n"
						"%s",
						this->getCorsHeaders().c_str(),
						this->oldFileName.c_str()
				);

				// set closing flag
				WebServer::close(connection);
			}

	    	break;

		default:
			// ignore unknown event
			break;
		}
	}

	// generate CORS headers
	std::string WebServer::getCorsHeaders() {
		return	"Access-Control-Allow-Origin: " + this->cors + "\r\n"
				"Access-Control-Allow-Methods: GET, POST\r\n"
				"Access-Control-Allow-Headers: Content-Type";
	}

	// static file name generator, throws WebServer::Exception
	mg_str WebServer::generateFileName(ConnectionPtr connection, mg_str fileName) {
		// check argument
		if(connection == nullptr) {
			throw Exception(
					"WebServer::generateFileName():"
					" No connection has been specified ["
					+ std::string(fileName.p, fileName.len)
					+ "]"
			);
		}

		if(connection->user_data == nullptr) {
			throw Exception(
					"WebServer::generateFileName():"
					" No WebServer has been specified ["
					+ std::string(fileName.p, fileName.len)
					+ "]"
			);
		}

		return static_cast<WebServer *>(connection->user_data)->generateFileNameInClass();
	}

	// file name generator (in-class)
	mg_str WebServer::generateFileNameInClass() {
		mg_str result{};

		// get length of file cache directory
		const auto cacheDirLength{this->fileCache.length()};

		// save length of file name in result
		result.len = cacheDirLength + randFileNameLength + 1;

		// allocate memory for file name
		// NOLINTNEXTLINE(cppcoreguidelines-no-malloc, cppcoreguidelines-owning-memory, hicpp-no-malloc)
		char * buffer{static_cast<char *>(std::malloc(result.len))};

		// copy file cache directory to file name
		std::memcpy(
				static_cast<void *>(buffer),
				static_cast<const void *>(this->fileCache.data()),
				cacheDirLength
		);

		// add path separator to file name
		// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
		buffer[cacheDirLength] = Helper::FileSystem::getPathSeparator();

		// generate random file names until no file with the generated name already exists
		do {
			std::memcpy(
					// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
					static_cast<void *>(buffer + cacheDirLength + 1),
					static_cast<const void *>(Helper::Strings::generateRandom(randFileNameLength).c_str()),
					randFileNameLength
			);
		}
		while(Helper::FileSystem::exists(std::string(buffer, result.len)));

		// save new file name in result
		result.p = buffer;

		// save file name in class and return result
		const std::string tmpCopy(result.p, result.len);

		this->oldFileName = tmpCopy.substr(cacheDirLength + 1);
		
		return result;
	}

} /* namespace crawlservpp::Main */
