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
 * curl.h
 *
 * Define libcurl flags that are not defined yet.
 *  Used for compiling with older versions of libcurl.
 *
 *  Created on: Jul 30, 2020
 *      Author: ans
 */

#ifndef HELPER_PORTABILITY_CURL_H_
#define HELPER_PORTABILITY_CURL_H_

#include <curl/curl.h>

/*
 * CHECK LIBRARY (HEADER) VERSION
 */

#if LIBCURL_VERSION_NUM < 0x071100 /* 7.17.0 */
#error Your libcurl library is hopelessly outdated (< 7.17.0).
#endif

#if LIBCURL_VERSION_NUM < 0x074400 /* 7.68.0 */
#warning Your libcurl library is outdated (< 7.68.0), consider updating it.
#endif

/*
 * MACROS FOR UNDEFINED OPTIONS
 */

#if LIBCURL_VERSION_NUM < 0x072100 /* 7.33.0 */
#define CURL_HTTP_VERSION_2_0 3
#endif

#if LIBCURL_VERSION_NUM < 0x072F00 /* 7.47.0 */
#define CURL_HTTP_VERSION_2TLS 4
#endif

#if LIBCURL_VERSION_NUM < 0x073100 /* 7.49.0 */
#define CURLOPT_TCP_FASTOPEN static_cast<CURLoption>(CURLOPTTYPE_LONG + 244)
#define CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE 5
#endif

#if LIBCURL_VERSION_NUM < 0x073400 /* 7.52.0 */
#define CURLOPT_PROXY_SSL_VERIFYPEER static_cast<CURLoption>(CURLOPTTYPE_LONG + 248)
#define CURLOPT_PROXY_SSL_VERIFYHOST static_cast<CURLoption>(CURLOPTTYPE_LONG + 249)
#define CURLOPT_PROXY_TLSAUTH_USERNAME static_cast<CURLoption>(CURLOPTTYPE_OBJECTPOINT + 251)
#define CURLOPT_PROXY_TLSAUTH_PASSWORD static_cast<CURLoption>(CURLOPTTYPE_OBJECTPOINT + 252)
#define CURLOPT_PROXY_TLSAUTH_TYPE static_cast<CURLoption>(CURLOPTTYPE_OBJECTPOINT + 253)
#define CURLOPT_PRE_PROXY static_cast<CURLoption>(CURLOPTTYPE_OBJECTPOINT + 262)
#endif

#if LIBCURL_VERSION_NUM < 0x073B00 /* 7.59.0 */
#define CURLOPT_HAPPY_EYEBALLS_TIMEOUT_MS static_cast<CURLoption>(CURLOPTTYPE_LONG + 271)
#define CURL_HET_DEFAULT 200L
#endif

#if LIBCURL_VERSION_NUM < 0x073C00 /* 7.60.0 */
#define CURLOPT_DNS_SHUFFLE_ADDRESSES static_cast<CURLoption>(CURLOPTTYPE_LONG + 275)
#endif

#if LIBCURL_VERSION_NUM < 0x073E00 /* 7.62.0 */
#define CURLOPT_DOH_URL static_cast<CURLoption>(CURLOPTTYPE_OBJECTPOINT + 279)
#endif

#if LIBCURL_VERSION_NUM < 0x074200 /* 7.66.0 */
#define CURL_HTTP_VERSION_3 30
#endif

/*
 * MACROS FOR UNDEFINED FEATURES
 */

#ifndef CURL_VERSION_SSL
#define CURL_VERSION_SSL (1<<2)
#endif

#ifndef CURL_VERSION_LIBZ
#define CURL_VERSION_LIBZ (1<<3)
#endif

#ifndef CURL_VERSION_TLSAUTH_SRP
#define CURL_VERSION_TLSAUTH_SRP (1<<14)
#endif

#ifndef CURL_VERSION_HTTP2
#define CURL_VERSION_HTTP2 (1<<16)
#endif

#ifndef CURL_VERSION_BROTLI
#define CURL_VERSION_BROTLI (1<<23)
#endif

#ifndef CURL_VERSION_HTTP3
#define CURL_VERSION_HTTP3 (1<<25)
#endif

#ifndef CURL_VERSION_ZSTD
#define CURL_VERSION_ZSTD (1<<26)
#endif

#endif /* HELPER_PORTABILITY_CURL_H_ */
