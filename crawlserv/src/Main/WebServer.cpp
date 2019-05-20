/*
 *
 * ---
 *
 *  Copyright (C) 2019 Anselm Schmidt (ans[ät]ohai.su)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version in addition to the terms of any
 *  licences already herein identified.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
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
 *  Created on: Feb 1, 2019
 *      Author: ans
 */

#include "WebServer.hpp"

namespace crawlservpp::Main {

	// constructor: set file cache directory and initialize embedded web server
	WebServer::WebServer(const std::string& fileCacheDirectory)
			: fileLength(MAIN_WEBSERVER_FILE_LENGTH),
			  fileCache(fileCacheDirectory) {
		mg_mgr_init(&(this->eventManager), nullptr);
	}

	// destructor: free embedded web server
	WebServer::~WebServer() {
		mg_mgr_free(&(this->eventManager));
	}

	// bind embedded web server to port, set user data and protocol, throws WebServer::Exception
	void WebServer::initHTTP(const std::string& port) {
		ConnectionPtr connection = mg_bind(
				&(this->eventManager),
				port.c_str(),
				WebServer::eventHandler
		);

		if(!connection)
			throw Exception("Could not bind server to port " + port);

		// set user data (i.e. pointer to class)
		connection->user_data = this;

		// set protocol
		mg_set_protocol_http_websocket(connection);
	}

	// set cross-origin resource sharing (CORS) string
	void WebServer::setCORS(const std::string& allowed) {
		this->cors = allowed;
	}

	// set callback function for accepts
	void WebServer::setAcceptCallback(AcceptCallback callback) {
		this->onAccept = callback;
	}

	// set callback function for requests
	void WebServer::setRequestCallback(RequestCallback callback) {
		this->onRequest = callback;
	}

	// poll server
	void WebServer::poll(int timeOut) {
		mg_mgr_poll(&(this->eventManager), timeOut);
	}

	// send HTTP reply, throws WebServer::Exception
	void WebServer::send(
			ConnectionPtr connection,
			unsigned short code,
			const std::string& type,
			const std::string& content
	) {
		// check connection
		if(!connection)
			throw Exception("WebServer::send(): No connection specified");

		// send reply
		std::string head;

		if(!type.empty())
			head += "Content-Type: " + type + "\r\n";

		head += this->getCorsHeaders();

		mg_send_head(connection, code, content.size(), head.c_str());

		mg_printf(connection, "%s", content.c_str());
	}

	// send file from file cache, throws WebServer::Exception
	void WebServer::sendFile(ConnectionPtr connection, const std::string& fileName, void * data) {
		// check arguments
		if(!connection)
			throw Exception("WebServer::sendFile(): No connection specified");

		const std::string fullFileName(
				this->fileCache
				+ Helper::FileSystem::getPathSeparator()
				+ fileName
		);

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

	// send internal server error (HTTP code 500) with custom message and close connection, throws WebServer::Exception
	void WebServer::sendError(ConnectionPtr connection, const std::string& error) {
		// check for connection
		if(!connection)
			throw Exception("WebServer::sendError(): No connection specified");

		const std::string responseStr(
				"HTTP/1.1 500 "
				+ error + "\r\n"
				"Content-Length: 0\r\n\r\n"
		);

		mg_printf(
				connection,
				"%s",
				responseStr.c_str()
		);

		// set closing flag
		connection->flags |= MG_F_SEND_AND_CLOSE;
	}

	// close connection immediately, throws WebServer::Exception
	void WebServer::close(ConnectionPtr connection) {
		// check for connection
		if(!connection)
			throw Exception("WebServer::close(): No connection specified");

		// set closing flag
		connection->flags |= MG_F_CLOSE_IMMEDIATELY;
	}

	// static helper function: get client IP from connection
	std::string WebServer::getIP(ConnectionPtr connection) {
		char ip[INET6_ADDRSTRLEN];

		mg_sock_to_str(
				connection->sock,
				ip,
				INET6_ADDRSTRLEN,
				MG_SOCK_STRINGIFY_REMOTE | MG_SOCK_STRINGIFY_IP
		);

		return std::string(ip);
	}

	// static event handler, throws WebServer::Exception
	void WebServer::eventHandler(ConnectionPtr connection, int event, void * data) {
		// check argument
		if(!connection)
			throw Exception("WebServer::eventHandler(): No connection specified");

		if(!(connection->user_data))
			throw Exception("WebServer::eventHandler(): No WebServer specified");

		static_cast<WebServer *>(connection->user_data)->eventHandlerInClass(
				connection,
				event,
				data
		);
	}

	// event handler (in-class)
	void WebServer::eventHandlerInClass(ConnectionPtr connection, int event, void * data) {
		http_message * httpMessage = nullptr;
		std::string method;
		std::string body;

		// get IP
		const std::string ip(WebServer::getIP(connection));

		switch(event) {
		case MG_EV_ACCEPT:
			// handle accept event
			this->onAccept(connection);

			break;

		case MG_EV_HTTP_REQUEST:
			// handle request event
			httpMessage = (http_message *) data;
			method = std::string(httpMessage->method.p, httpMessage->method.len);
			body = std::string(httpMessage->body.p, httpMessage->body.len);

			this->onRequest(connection, method, body, data);

			break;

		case MG_EV_HTTP_PART_BEGIN:
		case MG_EV_HTTP_PART_DATA:
		case MG_EV_HTTP_PART_END:
			// reset file name before upload begins
			if(event == MG_EV_HTTP_PART_BEGIN)
				this->oldFileName = "";

			// handle file upload
			if(
					data
					&& static_cast<mg_http_multipart_part *>(data)->var_name
					&& !strncmp(
							static_cast<mg_http_multipart_part *>(data)->var_name,
							"fileToUpload",
							12
					)
			)
				mg_file_upload_handler(
						connection,
						event,
						data,
						WebServer::generateFileName
				);

			if(event == MG_EV_HTTP_PART_END) {
				// file upload finished: return name on server as plain text
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
				connection->flags |= MG_F_SEND_AND_CLOSE;
			}

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
		if(!connection)
			throw Exception(
					"WebServer::generateFileName(): No connection specified ["
					+ std::string(fileName.p, fileName.len)
					+ "]"
			);

		if(!(connection->user_data))
			throw Exception(
					"WebServer::generateFileName(): No WebServer specified ["
					+ std::string(fileName.p, fileName.len)
					+ "]"
			);

		return static_cast<WebServer *>(connection->user_data)->generateFileNameInClass();
	}

	// file name generator (in-class)
	mg_str WebServer::generateFileNameInClass() {
		mg_str result;

		// save length of file name in result
		result.len = this->fileCache.size() + MAIN_WEBSERVER_FILE_LENGTH + 1;

		// allocate memory for file name
		char * buffer = static_cast<char *>(std::malloc(result.len));

		// copy file cache directory to file name
		std::memcpy(
				static_cast<void *>(buffer),
				static_cast<const void *>(this->fileCache.c_str()),
				this->fileCache.size()
		);

		// add path separator to file name
		buffer[this->fileCache.size()] = Helper::FileSystem::getPathSeparator();

		// generate random file names until no file with the generated name already exists
		do {
			std::memcpy(
					static_cast<void *>(buffer + this->fileCache.size() + 1),
					static_cast<const void *>(Helper::Strings::generateRandom(MAIN_WEBSERVER_FILE_LENGTH).c_str()),
					MAIN_WEBSERVER_FILE_LENGTH
			);
		}
		while(Helper::FileSystem::exists(std::string(buffer, result.len)));

		// save new file name in result
		result.p = buffer;

		// save file name in class and return result
		this->oldFileName = std::string(
				result.p + this->fileCache.size() + 1,
				result.len - this->fileCache.size() - 1
		);
		
		return result;
	}
} /* crawlservpp::Main */
