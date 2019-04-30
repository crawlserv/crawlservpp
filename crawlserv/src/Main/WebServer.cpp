/*
 * WebServer.cpp
 *
 * Embedded web server using mongoose.
 *
 *  Created on: Feb 1, 2019
 *      Author: ans
 */

#include "WebServer.hpp"

namespace crawlservpp::Main {

	// constructor: set default value
	WebServer::WebServer(const std::string& fileCacheDirectory) : fileCache(fileCacheDirectory) {
		// initialize embedded web server
		mg_mgr_init(&(this->eventManager), nullptr);
	}

	// destructor: free embedded web server
	WebServer::~WebServer() {
		mg_mgr_free(&(this->eventManager));
	}

	// initialize embedded web server and bind it to port, throws std::runtime_error
	void WebServer::initHTTP(const std::string& port) {
		// bind mongoose server to port
		ConnectionPtr connection = mg_bind(
				&(this->eventManager),
				port.c_str(),
				WebServer::eventHandler
		);

		if(!connection)
			throw std::runtime_error("Could not bind server to port " + port);

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
		// poll server
		mg_mgr_poll(&(this->eventManager), timeOut);
	}

	// send reply, throws std::runtime_error
	void WebServer::send(
			ConnectionPtr connection,
			unsigned short code,
			const std::string& type,
			const std::string& content
	) {
		// check connection
		if(!connection)
			throw std::runtime_error("WebServer::send(): No connection specified");

		// send reply
		std::string head;

		if(!type.empty())
			head += "Content-Type: " + type + "\r\n";

		head += "Access-Control-Allow-Origin: " + this->cors + "\r\n"
				"Access-Control-Allow-Methods: GET, POST\r\n"
				"Access-Control-Allow-Headers: Content-Type";

		mg_send_head(connection, code, content.size(), head.c_str());

		mg_printf(connection, "%s", content.c_str());
	}

	// send error (HTTP code 500) with custom error message and close connection, throws std::runtime_error
	void WebServer::sendError(ConnectionPtr connection, const std::string& error) {
		// check for connection
		if(!connection)
			throw std::runtime_error("WebServer::sendError(): No connection specified");

		std::string responseStr =
				"HTTP/1.1 500 "
				+ error + "\r\n"
				"Content-Length: 0\r\n\r\n";

		mg_printf(
				connection,
				"%s",
				responseStr.c_str()
		);

		connection->flags |= MG_F_SEND_AND_CLOSE;
	}

	// close connection immediately, throws std::runtime_error
	void WebServer::close(ConnectionPtr connection) {
		// check for connection
		if(!connection)
			throw std::runtime_error("WebServer::close(): No connection specified");

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

	// static event handler
	void WebServer::eventHandler(ConnectionPtr connection, int event, void * data) {
		if(connection->user_data && data)
			static_cast<WebServer *>(connection->user_data)->eventHandlerInClass(
					connection,
					event,
					data
			);
	}

	// event handler
	void WebServer::eventHandlerInClass(ConnectionPtr connection, int event, void * data) {
		http_message * httpMessage = nullptr;

		std::string method;
		std::string body;
		std::string ip = WebServer::getIP(connection); // get IP
		std::string fileName;

		WebServer::FileUploadData * uploadData = nullptr;
		struct mg_http_multipart_part * multiPart = nullptr;

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

			this->onRequest(connection, method, body);

			break;

		case MG_EV_HTTP_PART_BEGIN:
			// handle begin of file upload: check whether writer already exists
			if(connection->user_data)
				break;

			// generate file name
			do {
				fileName = Helper::Strings::generateRandom(128);
			}
			while(
					Helper::FileSystem::exists(
							this->fileCache
							+ Helper::FileSystem::getPathSeparator()
							+ fileName
					)
			);

			// create upload data and open file for writing
			uploadData = new WebServer::FileUploadData(
					fileName,
					this->fileCache
					+ Helper::FileSystem::getPathSeparator()
					+ fileName
			);

			// check whether file has been opened
			if(!(uploadData->stream.is_open())) {
				WebServer::sendError(
						connection,
						"Failed to open \'"
						+ fileName
						+ "\' for reading"
				);

				break;
			}

			// save upload data
			connection->user_data = static_cast<void *>(uploadData);

			break;

		case MG_EV_HTTP_PART_DATA:
			// handle part of file upload
			if(connection->user_data) {
				uploadData = static_cast<WebServer::FileUploadData *>(connection->user_data);
				multiPart = static_cast<struct mg_http_multipart_part *>(data);

				if(multiPart && multiPart->data.p && multiPart->data.len) {
					uploadData->stream << std::string(multiPart->data.p, multiPart->data.len);
					uploadData->stream << std::flush;
				}
			}
			else
				WebServer::sendError(connection, "No file open to write to");

			break;

		case MG_EV_HTTP_PART_END:
			// handle end of file upload
			if(connection->user_data) {
				uploadData = static_cast<WebServer::FileUploadData *>(connection->user_data);

				if(uploadData->stream.is_open())
					uploadData->stream.close();

				this->send(connection, 200, "text/plain", uploadData->name);

				delete uploadData;

				connection->user_data = nullptr;
			}
			else
				WebServer::sendError(connection, "Missing information about file");

			break;

		case MG_EV_CLOSE:
			// handle closing of connection
			if(connection->user_data) {
				uploadData = static_cast<WebServer::FileUploadData *>(connection->user_data);

				if(uploadData->stream.is_open())
					uploadData->stream.close();

				delete uploadData;

				connection->user_data = nullptr;
			}

			break;
		}
	}
} /* crawlservpp::Main */
