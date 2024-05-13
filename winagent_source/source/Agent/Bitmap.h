/*
SyslogAgent: a syslog agent for Windows
Copyright © 2021 Logzilla Corp.
*/

#pragma once
#include <mutex>
#include <string>
#include <vector>

class Bitmap
{
public:
	static constexpr size_t INVALID_BIT_NUMBER = ~static_cast<size_t>(0);
	Bitmap(int number_of_bits, unsigned char initial_bit_value);
	bool isSet(int bit_number) const;
	unsigned char bitValue(int bit_number) const;
	void setBitTo(int bit_number, unsigned char new_bit_value);
	unsigned char testAndSet(int bit_number);
	unsigned char testAndClear(int bit_number);
	int getFirstZero() const; // returns -1 if none
	int getAndSetFirstZero(); // returns -1 if none
	int getFirstOne() const; // returns -1 if none
	int getAndClearFirstOne(); // returns -1 if none
	int countZeroes();
	int countOnes();
	std::string asHexString() const;
	std::string asBinaryString() const;

private:
	int number_of_bits_;
	int number_of_words_;
	std::vector<size_t> bitmap_;
	std::mutex in_use_;

	int getAndOptionallyClearFirstOne(bool do_clear);
	int getAndOptionallySetFirstZero(bool do_set);
};


