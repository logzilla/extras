#include "pch.h"
#include "Bitmap.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <stdexcept>

// ---------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------
#define BITS_PER_WORD  (sizeof(std::size_t) * 8)

using namespace std;

// ---------------------------------------------------------------------------
//  Construction
// ---------------------------------------------------------------------------
Bitmap::Bitmap(std::size_t number_of_bits, unsigned char initial_value)
{
    if (number_of_bits == 0 || number_of_bits > MAX_BITS)
        throw invalid_argument("Bitmap: number_of_bits out of range");

    number_of_bits_ = number_of_bits;
    number_of_words_ = (number_of_bits + BITS_PER_WORD - 1) / BITS_PER_WORD;

    // set every used word to either 0 or all-ones, unused words remain 0
    std::size_t init_word = initial_value ? ~static_cast<std::size_t>(0) : 0;
    fill(bitmap_.begin(), bitmap_.begin() + number_of_words_, init_word);

    ones_count_.store(initial_value ? static_cast<int>(number_of_bits) : 0,
        memory_order_release);
}

// ---------------------------------------------------------------------------
//  Bit queries (no lock needed)
// ---------------------------------------------------------------------------
[[nodiscard]] unsigned char Bitmap::bitValue(std::size_t bit) const
{
    if (bit >= number_of_bits_)
        throw out_of_range("Bitmap::bitValue – index out of range");

    std::size_t word = bit / BITS_PER_WORD;
    std::size_t mask = static_cast<std::size_t>(1) << (bit % BITS_PER_WORD);
    return (bitmap_[word] & mask) ? 1 : 0;
}

bool Bitmap::isSet(std::size_t bit) const { return bitValue(bit) != 0; }

// ---------------------------------------------------------------------------
//  Mutating operations – all take the mutex_ once and never re-lock
// ---------------------------------------------------------------------------
void Bitmap::setBitTo(std::size_t bit, unsigned char new_value)
{
    if (bit >= number_of_bits_)
        throw out_of_range("Bitmap::setBitTo – index out of range");

    lock_guard<detail::BitmapMutex> lk(mutex_);

    std::size_t word = bit / BITS_PER_WORD;
    std::size_t mask = static_cast<std::size_t>(1) << (bit % BITS_PER_WORD);
    bool currently_set = (bitmap_[word] & mask) != 0;

    if (currently_set == static_cast<bool>(new_value))
        return; // no change

    if (new_value)
    {
        bitmap_[word] |= mask;
        ones_count_.fetch_add(1, memory_order_release);
    }
    else
    {
        bitmap_[word] &= ~mask;
        ones_count_.fetch_sub(1, memory_order_release);
    }
}

unsigned char Bitmap::testAndSet(std::size_t bit)
{
    if (bit >= number_of_bits_)
        throw out_of_range("Bitmap::testAndSet – index out of range");

    lock_guard<detail::BitmapMutex> lk(mutex_);

    std::size_t word = bit / BITS_PER_WORD;
    std::size_t mask = static_cast<std::size_t>(1) << (bit % BITS_PER_WORD);
    bool old_val = (bitmap_[word] & mask) != 0;

    if (!old_val)
    {
        bitmap_[word] |= mask;
        ones_count_.fetch_add(1, memory_order_release);
    }
    return static_cast<unsigned char>(old_val);
}

unsigned char Bitmap::testAndClear(std::size_t bit)
{
    if (bit >= number_of_bits_)
        throw out_of_range("Bitmap::testAndClear – index out of range");

    lock_guard<detail::BitmapMutex> lk(mutex_);

    std::size_t word = bit / BITS_PER_WORD;
    std::size_t mask = static_cast<std::size_t>(1) << (bit % BITS_PER_WORD);
    bool old_val = (bitmap_[word] & mask) != 0;

    if (old_val)
    {
        bitmap_[word] &= ~mask;
        ones_count_.fetch_sub(1, memory_order_release);
    }
    return static_cast<unsigned char>(old_val);
}

// ---------------------------------------------------------------------------
//  Search helpers (private)
// ---------------------------------------------------------------------------
int Bitmap::getAndOptionallyClearFirstOne(bool clear)
{
    lock_guard<detail::BitmapMutex> lk(mutex_);

    for (std::size_t w = 0; w < number_of_words_; ++w)
    {
        std::size_t word_val = bitmap_[w];
        if (word_val == 0) continue;

        for (std::size_t b = 0; b < BITS_PER_WORD && (w * BITS_PER_WORD + b) < number_of_bits_; ++b)
        {
            if (word_val & (static_cast<std::size_t>(1) << b))
            {
                std::size_t idx = w * BITS_PER_WORD + b;
                if (clear)
                {
                    bitmap_[w] &= ~(static_cast<std::size_t>(1) << b);
                    ones_count_.fetch_sub(1, memory_order_release);
                }
                return static_cast<int>(idx);
            }
        }
    }
    return -1;
}

int Bitmap::getAndOptionallySetFirstZero(bool set)
{
    lock_guard<detail::BitmapMutex> lk(mutex_);

    for (std::size_t w = 0; w < number_of_words_; ++w)
    {
        std::size_t word_val = bitmap_[w];
        if (word_val == static_cast<std::size_t>(~0)) continue; // all ones

        for (std::size_t b = 0; b < BITS_PER_WORD && (w * BITS_PER_WORD + b) < number_of_bits_; ++b)
        {
            if (!(word_val & (static_cast<std::size_t>(1) << b)))
            {
                std::size_t idx = w * BITS_PER_WORD + b;
                if (set)
                {
                    bitmap_[w] |= (static_cast<std::size_t>(1) << b);
                    ones_count_.fetch_add(1, memory_order_release);
                }
                return static_cast<int>(idx);
            }
        }
    }
    return -1;
}

//  public wrappers
int Bitmap::getFirstOne()             const { return const_cast<Bitmap*>(this)->getAndOptionallyClearFirstOne(false); }
int Bitmap::getAndClearFirstOne() { return getAndOptionallyClearFirstOne(true); }
int Bitmap::getFirstZero()            const { return const_cast<Bitmap*>(this)->getAndOptionallySetFirstZero(false); }
int Bitmap::getAndSetFirstZero() { return getAndOptionallySetFirstZero(true); }

// ---------------------------------------------------------------------------
//  Counts – lock-free but consistent thanks to acquire
// ---------------------------------------------------------------------------
int Bitmap::countOnes() const { return ones_count_.load(memory_order_acquire); }
int Bitmap::countZeroes() const { return static_cast<int>(number_of_bits_) - countOnes(); }

// ---------------------------------------------------------------------------
//  String helpers – heavy, used for debugging only
// ---------------------------------------------------------------------------
std::string Bitmap::asHexString() const
{
    lock_guard<detail::BitmapMutex> lk(mutex_);

#if defined(_WIN64) || defined(__x86_64__) || defined(__ppc64__)
    constexpr const char* FMT = "%016zx";
#else
    constexpr const char* FMT = "%08zx";
#endif

    string out;
    for (std::size_t i = 0; i < number_of_words_; ++i)
    {
        char buf[20];
        sprintf_s(buf, sizeof(buf), FMT, bitmap_[i]);
        out.insert(0, buf); // MSB first
    }
    return out;
}

std::string Bitmap::asBinaryString() const
{
    lock_guard<detail::BitmapMutex> lk(mutex_);
    if (number_of_bits_ > 1000) return "(too many bits)";

    string out; out.reserve(number_of_bits_);

    for (std::size_t bit = 0; bit < number_of_bits_; ++bit)
    {
        out.push_back(isSet(number_of_bits_ - 1 - bit) ? '1' : '0');
    }
    return out;
}
