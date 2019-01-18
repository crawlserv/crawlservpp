/*
 * Config.cpp
 *
 * Network configuration. This class is used by both the crawler and the extractor.
 *
 * WARNING:	Changing the configuration requires updating 'json/crawler.json' and 'json/extractor.json'
 * 			in crawlserv_frontend! See there for details on the specific configuration entries.
 *
 *  Created on: Jan 8, 2019
 *      Author: ans
 */

#include "Config.h"

namespace crawlservpp::Network {

// constructor: set default values
Config::Config() {
	this->connectionsMax = 5;
	this->contentLengthIgnore = false;
	this->cookies = false;
	this->cookiesSession = true;
	this->dnsCacheTimeOut = 60;
	this->dnsShuffle = false;
	this->encodingBr = true;
	this->encodingDeflate = true;
	this->encodingGZip = true;
	this->encodingIdentity = true;
	this->encodingTransfer = false;
	this->httpVersion = Config::httpVersionV2tls;
	this->localPort = 0;
	this->localPortRange = 1;
	this->proxyTunnelling = false;
	this->redirect = true;
	this->redirectMax = 20;
	this->redirectPost301 = false;
	this->redirectPost302 = false;
	this->redirectPost303 = false;
	this->refererAutomatic = false;
	this->speedDownLimit = 0;
	this->speedLowLimit = 0;
	this->speedLowTime = 0;
	this->speedUpLimit = 0;
	this->sslVerifyHost = true;
	this->sslVerifyPeer = true;
	this->sslVerifyProxyHost = true;
	this->sslVerifyProxyPeer = true;
	this->sslVerifyStatus = false;
	this->tcpFastOpen = false;
	this->tcpKeepAlive = false;
	this->tcpKeepAliveIdle = 60;
	this->tcpKeepAliveInterval = 60;
	this->tcpNagle = false;
	this->timeOut = 300;
	this->timeOutHappyEyeballs = 0;
	this->timeOutRequest = 300;
	this->verbose = false;
}

// destructor stub
Config::~Config() {}

// set network configuration entry from parsed JSON member (set value by iterator)
void Config::setEntry(const std::string& name, const rapidjson::Value::ConstMemberIterator& iterator,
		std::vector<std::string>& warningsTo) {
	if(name == "connections.max") {
		if(iterator->value.IsUint()) this->connectionsMax = iterator->value.GetUint();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not unsigned int).");
	}
	else if(name == "contentlength.ignore") {
		if(iterator->value.IsBool()) this->contentLengthIgnore = iterator->value.GetBool();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not bool).");
	}
	else if(name == "cookies") {
		if(iterator->value.IsBool()) this->cookies = iterator->value.GetBool();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not bool).");
	}
	else if(name == "cookies.load") {
		if(iterator->value.IsString()) this->cookiesLoad = iterator->value.GetString();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not string).");
	}
	else if(name == "cookies.overwrite") {
		if(iterator->value.IsArray()) {
			this->cookiesOverwrite.clear();
			this->cookiesOverwrite.reserve(iterator->value.Size());
			for(auto k = iterator->value.Begin(); k != iterator->value.End(); ++k) {
				if(k->IsString()) this->cookiesOverwrite.push_back(k->GetString());
				else warningsTo.push_back("Value in \'networking." + name
						+ "\' ignored because of wrong type (not string).");
			}
		}
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not array).");
	}
	else if(name == "cookies.save") {
		if(iterator->value.IsString()) this->cookiesSave = iterator->value.GetString();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not string).");
	}
	else if(name == "cookies.session") {
		if(iterator->value.IsBool()) this->cookiesSession = iterator->value.GetBool();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not bool).");
	}
	else if(name == "cookies.set") {
		if(iterator->value.IsString()) this->cookiesSet = iterator->value.GetString();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not string).");
	}
	else if(name == "dns.cachetimeout") {
		if(iterator->value.IsInt64()) this->dnsCacheTimeOut = iterator->value.GetInt64();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not long).");
	}
	else if(name == "dns.doh") {
		if(iterator->value.IsString()) this->dnsDoH = iterator->value.GetString();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not string).");
	}
	else if(name == "dns.interface") {
		if(iterator->value.IsString()) this->dnsInterface = iterator->value.GetString();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not string).");
	}
	else if(name == "dns.resolves") {
		if(iterator->value.IsArray()) {
			this->dnsResolves.clear();
			this->dnsResolves.reserve(iterator->value.Size());
			for(auto k = iterator->value.Begin(); k != iterator->value.End(); ++k) {
				if(k->IsString()) this->dnsResolves.push_back(k->GetString());
				else warningsTo.push_back("Value in \'networking." + name
						+ "\' ignored because of wrong type (not string).");
			}
		}
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not array).");
	}
	else if(name == "dns.servers") {
		if(iterator->value.IsArray()) {
			this->dnsServers.clear();
			this->dnsServers.reserve(iterator->value.Size());
			for(auto k = iterator->value.Begin(); k != iterator->value.End(); ++k) {
				if(k->IsString()) this->dnsServers.push_back(k->GetString());
				else warningsTo.push_back("Value in \'networking." + name
						+ "\' ignored because of wrong type (not string).");
			}
		}
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not array).");
	}
	else if(name == "dns.shuffle") {
		if(iterator->value.IsBool()) this->dnsShuffle = iterator->value.GetBool();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not bool).");
	}
	else if(name == "encoding.br") {
		if(iterator->value.IsBool()) this->encodingBr = iterator->value.GetBool();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not bool).");
	}
	else if(name == "encoding.deflate") {
		if(iterator->value.IsBool()) this->encodingDeflate = iterator->value.GetBool();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not bool).");
	}
	else if(name == "encoding.gzip") {
		if(iterator->value.IsBool()) this->encodingGZip = iterator->value.GetBool();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not bool).");
	}
	else if(name == "encoding.identity") {
		if(iterator->value.IsBool()) this->encodingIdentity = iterator->value.GetBool();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not bool).");
	}
	else if(name == "encoding.transfer") {
		if(iterator->value.IsBool()) this->encodingTransfer = iterator->value.GetBool();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not bool).");
	}
	else if(name == "headers") {
		if(iterator->value.IsArray()) {
			this->headers.clear();
			this->headers.reserve(iterator->value.Size());
			for(auto k = iterator->value.Begin(); k != iterator->value.End(); ++k) {
				if(k->IsString()) this->headers.push_back(k->GetString());
				else warningsTo.push_back("Value in \'networking." + name
						+ "\' ignored because of wrong type (not string).");
			}
		}
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not array).");
	}
	else if(name == "http.200aliases") {
		if(iterator->value.IsArray()) {
			this->http200Aliases.clear();
			this->http200Aliases.reserve(iterator->value.Size());
			for(auto k = iterator->value.Begin(); k != iterator->value.End(); ++k) {
				if(k->IsString()) this->http200Aliases.push_back(k->GetString());
				else warningsTo.push_back("Value in \'networking." + name
						+ "\' ignored because of wrong type (not string).");
			}
		}
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not array).");
	}
	else if(name == "http.version") {
		if(iterator->value.IsInt()) this->httpVersion = iterator->value.GetInt();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not int).");
	}
	else if(name == "local.interface") {
		if(iterator->value.IsString()) this->localInterface = iterator->value.GetString();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not string).");
	}
	else if(name == "local.port") {
		if(iterator->value.IsUint()) this->localPort = iterator->value.GetUint();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not unsigned int).");
	}
	else if(name == "local.portrange") {
		if(iterator->value.IsUint()) this->localPortRange = iterator->value.GetUint();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not unsigned int).");
	}
	else if(name == "proxy") {
		if(iterator->value.IsString()) this->proxy = iterator->value.GetString();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not string).");
	}
	else if(name == "proxy.auth") {
		if(iterator->value.IsString()) this->proxyAuth = iterator->value.GetString();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not string).");
	}
	else if(name == "proxy.headers") {
		if(iterator->value.IsArray()) {
			this->proxyHeaders.clear();
			this->proxyHeaders.reserve(iterator->value.Size());
			for(auto k = iterator->value.Begin(); k != iterator->value.End(); ++k) {
				if(k->IsString()) this->proxyHeaders.push_back(k->GetString());
				else warningsTo.push_back("Value in \'networking." + name
						+ "\' ignored because of wrong type (not string).");
			}
		}
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not array).");
	}
	else if(name == "proxy.pre") {
		if(iterator->value.IsString()) this->proxyPre = iterator->value.GetString();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not string).");
	}
	else if(name == "proxy.tlssrp.password") {
		if(iterator->value.IsString()) this->proxyTlsSrpPassword = iterator->value.GetString();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not string).");
	}
	else if(name == "proxy.tlssrp.user") {
		if(iterator->value.IsString()) this->proxyTlsSrpUser = iterator->value.GetString();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not string).");
	}
	else if(name == "proxyy.tunnelling") {
		if(iterator->value.IsBool()) this->proxyTunnelling = iterator->value.GetBool();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not bool).");
	}
	else if(name == "redirect") {
		if(iterator->value.IsBool()) this->redirect = iterator->value.GetBool();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not bool).");
	}
	else if(name == "redirect.max") {
		if(iterator->value.IsInt64()) this->redirectMax = iterator->value.GetInt64();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not long).");
	}
	else if(name == "redirect.post301") {
		if(iterator->value.IsBool()) this->redirectPost301 = iterator->value.GetBool();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not bool).");
	}
	else if(name == "redirect.post302") {
		if(iterator->value.IsBool()) this->redirectPost302 = iterator->value.GetBool();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not bool).");
	}
	else if(name == "redirect.post303") {
		if(iterator->value.IsBool()) this->redirectPost303 = iterator->value.GetBool();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not bool).");
	}
	else if(name == "referer") {
		if(iterator->value.IsString()) this->referer = iterator->value.GetString();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not string).");
	}
	else if(name == "referer.automatic") {
		if(iterator->value.IsBool()) this->refererAutomatic = iterator->value.GetBool();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not bool).");
	}
	else if(name == "speed.downlimit") {
		if(iterator->value.IsUint64()) this->speedDownLimit = iterator->value.GetUint64();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not unsigned long).");
	}
	else if(name == "speed.lowlimit") {
		if(iterator->value.IsUint64()) this->speedLowLimit = iterator->value.GetUint64();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not unsigned long).");
	}
	else if(name == "speed.lowtime") {
		if(iterator->value.IsUint64()) this->speedLowTime = iterator->value.GetUint64();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not unsigned long).");
	}
	else if(name == "speed.uplimit") {
		if(iterator->value.IsUint64()) this->speedUpLimit = iterator->value.GetUint64();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not unsigned long).");
	}
	else if(name == "ssl.verify.host") {
		if(iterator->value.IsBool()) this->sslVerifyHost = iterator->value.GetBool();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not bool).");
	}
	else if(name == "ssl.verify.peer") {
		if(iterator->value.IsBool()) this->sslVerifyPeer= iterator->value.GetBool();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not bool).");
	}
	else if(name == "ssl.verify.proxy.host") {
		if(iterator->value.IsBool()) this->sslVerifyProxyHost = iterator->value.GetBool();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not bool).");
	}
	else if(name == "ssl.verify.proxy.peer") {
		if(iterator->value.IsBool()) this->sslVerifyProxyPeer = iterator->value.GetBool();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not bool).");
	}
	else if(name == "ssl.verify.status") {
		if(iterator->value.IsBool()) this->sslVerifyStatus = iterator->value.GetBool();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not bool).");
	}
	else if(name == "tcp.fastopen") {
		if(iterator->value.IsBool()) this->tcpFastOpen = iterator->value.GetBool();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not bool).");
	}
	else if(name == "tcp.keepalive") {
		if(iterator->value.IsBool()) this->tcpKeepAlive = iterator->value.GetBool();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not bool).");
	}
	else if(name == "tcp.keepalive.idle") {
		if(iterator->value.IsUint64()) this->tcpKeepAliveIdle = iterator->value.GetUint64();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not unsigned long).");
	}
	else if(name == "tcp.keepalive.interval") {
		if(iterator->value.IsUint64()) this->tcpKeepAliveInterval = iterator->value.GetUint64();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not unsigned long).");
	}
	else if(name == "tcp.nagle") {
		if(iterator->value.IsBool()) this->tcpNagle = iterator->value.GetBool();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not bool).");
	}
	else if(name == "timeout") {
		if(iterator->value.IsUint64()) this->timeOut = iterator->value.GetUint64();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not unsigned long).");
	}
	else if(name == "timeout.happyeyeballs") {
		if(iterator->value.IsUint()) this->timeOutHappyEyeballs = iterator->value.GetUint();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not unsigned int).");
	}
	else if(name == "timeout.request") {
		if(iterator->value.IsUint64()) this->timeOutRequest = iterator->value.GetUint64();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not unsigned long).");
	}
	else if(name == "tlssrp.password") {
		if(iterator->value.IsString()) this->tlsSrpPassword = iterator->value.GetString();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not string).");
	}
	else if(name == "tlssrp.user") {
		if(iterator->value.IsString()) this->tlsSrpUser = iterator->value.GetString();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not string).");
	}
	else if(name == "useragent") {
		if(iterator->value.IsString()) this->userAgent = iterator->value.GetString();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not string).");
	}
	else if(name == "verbose") {
		if(iterator->value.IsBool()) this->verbose = iterator->value.GetBool();
		else warningsTo.push_back("\'networking." + name + "\' ignored because of wrong type (not bool).");
	}
	else warningsTo.push_back("Unknown configuration entry \'networking." + name + "\' ignored.");
}

}

