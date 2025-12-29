#pragma once
#include <streambuf>
#include <cstring>
#include <algorithm>

template <typename char_type>
struct OStreamBuf : public std::basic_streambuf<char_type, std::char_traits<char_type>> {
public:
    // Reserve one slot for the null terminator.
    OStreamBuf(char_type* buffer, std::streamsize bufferLength) {
        if(bufferLength > 0) {
            // Only allow bufferLength - 1 characters to be written.
            this->setp(buffer, buffer + bufferLength - 1);
            // Make sure the buffer starts null terminated.
            *this->pptr() = char_type();
        }
    }

    // Return the number of characters written (not counting the null terminator).
    std::streamsize current_length() const {
        return this->pptr() - this->pbase();
    }

protected:
    // Override xsputn to copy multiple characters and maintain the null terminator.
    std::streamsize xsputn(const char_type* s, std::streamsize count) override {
        std::streamsize avail = this->epptr() - this->pptr();
        std::streamsize to_copy = (std::min)(count, avail);
        if(to_copy > 0) {
            std::memcpy(this->pptr(), s, to_copy * sizeof(char_type));
            this->pbump(static_cast<int>(to_copy));
            // Ensure the buffer remains null terminated.
            *this->pptr() = char_type();
        }
        return to_copy;
    }

    // Override overflow to handle single-character insertions.
    typename std::basic_streambuf<char_type, std::char_traits<char_type>>::int_type
    overflow(typename std::basic_streambuf<char_type, std::char_traits<char_type>>::int_type ch) override {
        if(ch == std::char_traits<char_type>::eof())
            return std::char_traits<char_type>::not_eof(ch);
        if(this->pptr() < this->epptr()) {
            *this->pptr() = std::char_traits<char_type>::to_char_type(ch);
            this->pbump(1);
            // Always re-null terminate.
            *this->pptr() = char_type();
            return ch;
        }
        return std::char_traits<char_type>::eof();
    }
};
