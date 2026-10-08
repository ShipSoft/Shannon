#include <SHiP/random/philox_rng.hpp>
#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

/*
This is a tool to generate times strictly in order. It does so by progressively splitting the
interval in half until your reach group size. Then it does uniform draws from the remaining and
sorts them. Slower than the approximate draw but is exactly in order and so gets the distribution of
times, and gaps between times right.
*/

class sorted_random {
   public:
    sorted_random(std::uint32_t seed, std::uint32_t key_hi = 0xBEEFCAFE, const double maxTime = 1.,
                  const std::uint64_t nPoT = 10000, std::uint64_t groupSize = 1000,
                  int cacheDepth = 10)
        : m_seed(seed),
          m_stream(key_hi),
          m_maxTime(maxTime),
          m_n(nPoT),
          m_groupSize(groupSize),
          m_cacheDepth(cacheDepth) {
        if (m_groupSize > m_maxGroupSize) {
            throw std::invalid_argument(
                "You are trying to set a group size that is too large! The maximum allowed is " +
                std::to_string(m_maxGroupSize));
        }
        make_cache(m_cacheDepth);
    }

    [[nodiscard]]
    double next(const std::uint64_t evtNumber) const {
        if (evtNumber >= m_n) {
            throw std::invalid_argument(
                "Trying to generate a time for an event number larger than generated PoT. Check "
                "your options!");
        }
        // Start with the whole spill: every event, on the interval (0, 1).
        std::uint64_t lo = 0;
        std::uint64_t hi = m_n;
        double a = 0.0;
        double b = 1.0;
        std::uint64_t node = 1;

        while (true) {
            const std::uint64_t c = hi - lo;
            // 1. Small group: finish here. Every event in this group generates the
            //    same uniforms (same key), sorts them, and takes its own position.
            if (c <= m_groupSize) {
                SHiP::random::PhiloxRng rng{m_seed, m_stream, node};
                std::array<double, m_maxGroupSize>
                    u{};  // We set this to some largish notional number. The true group size must
                          // be less.
                for (std::uint64_t i = 0; i < c; ++i) {
                    u[i] = rng.uniform53();
                }
                std::sort(u.begin(), u.begin() + c);
                return (a + ((b - a) * u[evtNumber - lo])) * m_maxTime;
            }

            // 2. Find this group's middle event time: from the cache if it's
            //    stored there, otherwise compute it (same function, same answer).
            const std::uint64_t mid = lo + (c / 2);
            const double v = node < m_cache.size() ? m_cache[node] : draw_node(node, lo, hi, a, b);

            // 3. If our event is the middle one, that's its time.
            if (evtNumber == mid) {
                return v * m_maxTime;
            }

            // 4. Otherwise move into the half that contains our event. The middle
            //    time becomes the new upper or lower edge of the interval.
            if (evtNumber < mid) {
                hi = mid;
                b = v;
                node = 2 * node;
            } else {
                lo = mid + 1;
                a = v;
                node = (2 * node) + 1;
            }
        }
    }

   private:
    // A group of events [lo, hi) whose times all lie in the interval (a, b).
    struct Group {
        std::uint64_t lo = 0;
        std::uint64_t hi = 0;  // lo == hi means "no group here"
        double a = 0.0;
        double b = 0.0;
    };

    // Draws the time of the middle event of group [lo, hi) on (a, b).
    // Used both when building the cache and when walking, so they always agree.
    double draw_node(std::uint64_t node, std::uint64_t lo, std::uint64_t hi, double a,
                     double b) const {
        const std::uint64_t c = hi - lo;      // events in this group
        const std::uint64_t r = (c / 2) + 1;  // rank of the middle event (1-based)
        SHiP::random::PhiloxRng rng{m_seed, m_stream, node};
        const double u =
            rng.beta_dist_approx(static_cast<double>(r), static_cast<double>(c - r + 1),
                                 SHiP::random::Precision::Bits53);
        return a + ((b - a) * u);  // scale into this group's interval
    }

    void make_cache(const int n_layers = 10) {
        const std::size_t size = std::size_t{1} << n_layers;
        m_cache.assign(size, 0.0);
        std::vector<Group> groups(size);  // temporary, freed when we return
        groups[1] = {0, m_n, 0.0, 1.0};   // node 1: every event, the whole spill

        // A node's parent is node / 2, which always comes earlier in this loop,
        // so each node's group has already been filled in when we reach it.
        for (std::size_t node = 1; node < size; ++node) {
            const Group g = groups[node];
            if (g.hi - g.lo <= m_groupSize) {
                continue;  // small enough that the walk stops here (or empty)
            }
            const std::uint64_t mid = g.lo + ((g.hi - g.lo) / 2);
            const double v = draw_node(node, g.lo, g.hi, g.a, g.b);
            m_cache[node] = v;

            // Hand the two halves to the children, if they fit in the array
            if (2 * node < size) {
                groups[2 * node] = {g.lo, mid, g.a, v};            // events below mid: (a, v)
                groups[(2 * node) + 1] = {mid + 1, g.hi, v, g.b};  // events above mid: (v, b)
            }
        }
    }

    std::vector<double> m_cache;
    const double m_maxTime;
    std::uint32_t m_seed;
    std::uint32_t m_stream;
    std::uint64_t m_n;
    std::uint64_t m_groupSize = 1000;
    const int m_cacheDepth;
    static constexpr std::uint64_t m_maxGroupSize = 1000;
};
