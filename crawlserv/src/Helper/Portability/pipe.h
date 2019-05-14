/*
 * pipe.h
 *
 *  Created on: May 14, 2019
 *      Author: ans
 */

#ifndef HELPER_PORTABILITY_PIPE_H_
#define HELPER_PORTABILITY_PIPE_H_

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

#endif /* HELPER_PORTABILITY_PIPE_H_ */
