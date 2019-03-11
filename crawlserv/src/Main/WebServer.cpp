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
	WebServer::WebServer() {
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
		ConnectionPtr connection = mg_bind(&(this->eventManager), port.c_str(), WebServer::eventHandler);
		if(!connection) throw std::runtime_error("Could not bind server to port " + port);

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
	void WebServer::send(ConnectionPtr connection, unsigned short code, const std::string& type, const std::string& content) {
		// check connection
		if(!connection) throw std::runtime_error("WebServer::send(): No connection specified");

		// send reply
		std::string head;
		if(!type.empty()) head += "Content-Type: " + type + "\r\n";
		head += "Access-Control-Allow-Origin: " + this->cors + "\r\n"
				"Access-Control-Allow-Methods: GET, POST\r\n"
				"Access-Control-Allow-Headers: Content-Type";
		mg_send_head(connection, 200, content.size(), head.c_str());
		mg_printf(connection, "%s", content.c_str());
	}

	// close connection immediately, throws std::runtime_error
	void WebServer::close(ConnectionPtr connection) {
		// check for connection
		if(!connection) throw std::runtime_error("WebServer::close(): No connection specified");

		// set closing flag
		connection->flags |= MG_F_CLOSE_IMMEDIATELY;
	}

	// static event handler
	void WebServer::eventHandler(ConnectionPtr connection, int event, void * data) {
		if(connection->user_data && data)
			static_cast<WebServer *>(connection->user_data)->eventHandlerInClass(connection, event, data);
	}

	// event handler
	void WebServer::eventHandlerInClass(ConnectionPtr connection, int event, void * data) {
		// get ip
		std::string ip = WebServer::getIP(connection);

		switch(event) {
		case MG_EV_ACCEPT:
			// handle accept event
			this->onAccept(connection);
			break;

		case MG_EV_HTTP_REQUEST:
			// handle request event
			http_message * httpMessage = (http_message *) data;
			std::string method(httpMessage->method.p, httpMessage->method.len);
			std::string body(httpMessage->body.p, httpMessage->body.len);
			this->onRequest(connection, method, body);
			break;
		}
	}

	// static helper function: get client IP from connection
	std::string WebServer::getIP(ConnectionPtr connection) {
		char ip[INET6_ADDRSTRLEN];
		mg_sock_to_str(connection->sock, ip, INET6_ADDRSTRLEN, MG_SOCK_STRINGIFY_REMOTE | MG_SOCK_STRINGIFY_IP);
		return std::string(ip);
	}

} /* crawlservpp::Main */
