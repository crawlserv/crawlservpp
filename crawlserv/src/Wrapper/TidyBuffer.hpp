/*
 * TidyBuffer.hpp
 *
 * RAII wrapper for tidyhtml buffers.
 *
 *  Created on: Feb 7, 2019
 *      Author: ans
 */

#ifndef WRAPPER_TIDYBUFFER_HPP_
#define WRAPPER_TIDYBUFFER_HPP_

#include <tidybuffio.h>

#include <string>

namespace crawlservpp::Wrapper {

	class TidyBuffer {
	public:
		// constructor: fill buffer with zeros
		TidyBuffer() noexcept : buffer(::TidyBuffer()) {}

		// destructor: free buffer if necessary
		~TidyBuffer() {
			this->clear();
		}

		// get pointer to buffer
		::TidyBuffer * get() {
			return &(this->buffer);
		}

		// get const pointer to buffer
		const ::TidyBuffer * get() const {
			return &(this->buffer);
		}

		// create string from buffer
		std::string getString() const {
			return std::string(reinterpret_cast<const char *>(this->buffer.bp), this->buffer.size);
		}

		// free buffer
		void clear() {
			if(this->buffer.bp)
				tidyBufFree(&(this->buffer));
		}

		// bool operator
		operator bool() const {
			return this->buffer.bp != nullptr;
		}

		// not operator
		bool operator!() const {
			return this->buffer.bp == nullptr;
		}

		// not moveable, not copyable
		TidyBuffer(TidyBuffer&) = delete;
		TidyBuffer(TidyBuffer&&) = delete;
		TidyBuffer& operator=(TidyBuffer&) = delete;
		TidyBuffer& operator=(TidyBuffer&&) = delete;

	private:
		::TidyBuffer buffer;
	};

	} /* crawlservpp::Wrapper */

#endif /* WRAPPER_TIDYBUFFER_HPP_ */
