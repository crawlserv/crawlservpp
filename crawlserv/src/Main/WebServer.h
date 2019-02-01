/*
 * WebServer.h
 *
 * Embedded web server using mongoose.
 *
 *  Created on: Feb 1, 2019
 *      Author: ans
 */

#ifndef MAIN_WEBSERVER_H_
#define MAIN_WEBSERVER_H_

#include "../_extern/mongoose/mongoose.h"

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>

namespace crawlservpp::Main {

class WebServer final {
public:
	// for convenience
	typedef std::function<void(const std::string&)> AcceptCallback;
	typedef std::function<void(const std::string&, const std::string&, const std::string&)> RequestCallback;

	WebServer();
	virtual ~WebServer();

	// initializer
	void initHTTP(const std::string& port);

	// setters
	void setCORS(const std::string& allowed);
	void setAcceptCallback(AcceptCallback callback);
	void setRequestCallback(RequestCallback callback);

	// server functions
	void poll(int timeOut);
	void send(unsigned short code, const std::string& type, const std::string& content);
	void close();

private:
	mg_mgr eventManager;
	mg_connection * lastConnection;
	std::string cors;

	// callback functions
	AcceptCallback onAccept;
	RequestCallback onRequest;

	// event handler
	static void eventHandler(struct mg_connection * connection, int event, void * data);
	void eventHandlerInClass(struct mg_connection * connection, int event, void * data);

	// static helper function for the class
	static std::string getIP(mg_connection * connection);
};

}

#endif /* MAIN_WEBSERVER_H_ */
