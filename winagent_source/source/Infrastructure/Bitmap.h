#pragma once
//  ---------------------------------------------------------------------------
//  Bitmap – fixed‑size thread‑safe bitset with O(1) population‑count tracking
//  ---------------------------------------------------------------------------
//  • Maximum number of bits is defined by MAX_BITS.
//  • All mutating operations are mutex‑protected (BitmapMutex).
//  • copy/move is *disabled* – a Bitmap is intended to be shared via
//    std::shared_ptr or std::unique_ptr, never copied.
//  ---------------------------------------------------------------------------

#ifdef INFRASTRUCTURE_STATIC
#define INFRA_API
#else
#ifdef INFRASTRUCTURE_EXPORTS
#define INFRA_API __declspec(dllexport)
#else
#define INFRA_API __declspec(dllimport)
#endif
#endif

#include <array>
#include <atomic>
#include <cstddef>
#include <mutex>
#include <string>

namespace detail
{
    //  --------------------------------------------------------------------
    //  BitmapMutex – thin RAII wrapper to avoid exporting std::mutex in DLL
    //  --------------------------------------------------------------------
    class BitmapMutex
    {
    private:
        std::mutex mutex_;
    public:
        BitmapMutex() = default;
        BitmapMutex(const BitmapMutex&) = delete;
        BitmapMutex& operator=(const BitmapMutex&) = delete;

        void lock()      noexcept { mutex_.lock(); }
        void unlock()    noexcept { mutex_.unlock(); }
        bool try_lock()  noexcept { return mutex_.try_lock(); }
    };
} // namespace detail

class INFRA_API Bitmap
{
public:
    // ------------- compile‑time constants ---------------------------------
    static constexpr std::size_t MAX_BITS = 10'240;                 // tune to taste
    static constexpr std::size_t BITS_PER_WORD = sizeof(std::size_t) * 8;
    static constexpr std::size_t MAX_WORDS = (MAX_BITS + BITS_PER_WORD - 1) / BITS_PER_WORD;
    static constexpr std::size_t INVALID_BIT_NUM = static_cast<std::size_t>(-1);

    // ---------------------- construction ----------------------------------
    Bitmap(std::size_t number_of_bits, unsigned char initial_value = 0);

    // non‑copyable / non‑movable – share via pointer containers instead
    Bitmap(const Bitmap&) = delete;
    Bitmap(Bitmap&&) = delete;
    Bitmap& operator=(const Bitmap&) = delete;
    Bitmap& operator=(Bitmap&&) = delete;

    // ------------------- single‑bit operations ----------------------------
    [[nodiscard]] bool          isSet(std::size_t bit) const;
    [[nodiscard]] unsigned char bitValue(std::size_t bit) const;

    void setBitTo(std::size_t bit, unsigned char new_value); // 0 or 1
    unsigned char testAndSet(std::size_t bit);               // returns old value
    unsigned char testAndClear(std::size_t bit);               // returns old value

    // --------------- search helpers (first/any) ---------------------------
    int getFirstZero() const;        // -1 if none
    int getAndSetFirstZero();        // -1 if none
    int getFirstOne()  const;        // -1 if none
    int getAndClearFirstOne();       // -1 if none

    // -------------------- population counts -------------------------------
    int countZeroes() const;         // O(1)
    int countOnes()   const;         // O(1)

    // ---------------- debug / visualisation ------------------------------
    std::string asHexString()    const;   // hex words, MSB on the left
    std::string asBinaryString() const;   // *limited* – prints up to 1000 bits

private:
    // helper that scans & optionally mutates – defined in .cpp
    int  getAndOptionallyClearFirstOne(bool clear);
    int  getAndOptionallySetFirstZero(bool set);

    // --------------------- data members ----------------------------------
    std::size_t                           number_of_bits_{ 0 };
    std::size_t                           number_of_words_{ 0 };
    std::array<std::size_t, MAX_WORDS>    bitmap_{};
    mutable detail::BitmapMutex           mutex_;
    std::atomic<int>                      ones_count_{ 0 };
};
