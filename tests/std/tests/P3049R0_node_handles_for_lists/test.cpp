#include <list>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <forward_list>
#include <initializer_list>

bool allocations_allowed{true};
std::size_t allocations{0}, constructions{0};

template<typename T>
struct allocator final {
    using value_type = T;

    constexpr
    allocator() noexcept =default;
    template<typename U>
    constexpr
    allocator(const allocator<U> &) noexcept {}


    auto allocate(std::size_t n) -> T * {
        assert(allocations_allowed);
        T* ptr = std::allocator<T>{}.allocate(n);
        ++allocations;
        return ptr;
    }

    void deallocate(T * ptr, std::size_t n) noexcept {
        assert(allocations_allowed);
        assert(allocations);
        std::allocator<T>{}.deallocate(ptr, n);
        --allocations;
    }


    template<typename U, typename... Args>
    void construct(U * ptr, Args &&... args) {
        std::allocator<T> alloc;
        std::allocator_traits<std::allocator<T>>::construct(alloc, ptr, std::forward<Args>(args)...);
        ++constructions;
    }

    template<typename U>
    void destroy(U * ptr) {
        assert(constructions);
        std::allocator<T> alloc;
        std::allocator_traits<std::allocator<T>>::destroy(alloc, ptr);
        --constructions;
    }

    template<typename U>
    friend
    auto operator==(const allocator &, const allocator<U> &) noexcept -> bool { return true; }
};

template<typename Func>
void no_allocation_scope(Func func) {
    allocations_allowed = false;
    func();
    allocations_allowed = true;
}

bool test_equal(const std::list<int, allocator<int>> & l, std::initializer_list<int> ilist) {
    //check bidirectional to ensure all internal pointers are correctly set up
    return std::equal(l.begin(), l.end(), ilist.begin(), ilist.end())
        && std::equal(l.rbegin(), l.rend(), std::make_reverse_iterator(ilist.end()), std::make_reverse_iterator(ilist.begin()));
}

bool test_equal(const std::forward_list<int, allocator<int>> & l, std::initializer_list<int> ilist) {
    //check bidirectional to ensure all internal pointers are correctly set up
    return std::equal(l.begin(), l.end(), ilist.begin(), ilist.end());
}

template<typename Allocator>
auto size(const std::forward_list<int, Allocator> & l) -> std::size_t {
    return static_cast<std::size_t>(std::distance(l.begin(), l.end()));
}

void test_list() {
    {
        std::list<int, allocator<int>> l{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        assert(l.size() == 10);
        assert(allocations >= l.size());

        no_allocation_scope([&] {
            auto nh_front{l.extract(l.begin())};
            assert(nh_front.value() == 0);
            assert(l.size() == 9);
            assert(test_equal(l, {1, 2, 3, 4, 5, 6, 7, 8, 9}));

            auto nh_back{l.extract(std::prev(l.end()))};
            assert(nh_back.value() == 9);
            assert(l.size() == 8);
            assert(test_equal(l, {1, 2, 3, 4, 5, 6, 7, 8}));

            auto nh_middle{l.extract(std::next(l.begin(), 4))};
            assert(nh_middle.value() == 5);
            assert(l.size() == 7);
            assert(test_equal(l, {1, 2, 3, 4, 6, 7, 8}));


            l.insert(l.begin(), std::move(nh_back));
            assert(nh_back.empty());
            assert(l.size() == 8);
            assert(test_equal(l, {9, 1, 2, 3, 4, 6, 7, 8}));

            l.insert(l.end(), std::move(nh_front));
            assert(nh_front.empty());
            assert(l.size() == 9);
            assert(test_equal(l, {9, 1, 2, 3, 4, 6, 7, 8, 0}));

            l.insert(std::next(l.begin(), 4), std::move(nh_middle));
            assert(nh_middle.empty());
            assert(l.size() == 10);
            assert(test_equal(l, {9, 1, 2, 3, 5, 4, 6, 7, 8, 0}));
        });

        std::list<int, allocator<int>> empty;
        no_allocation_scope([&] {
            auto nh{l.extract(l.begin())};
            assert(nh.value() == 9);
            empty.insert(empty.end(), std::move(nh));
            assert(test_equal(empty, {9}));
        });

        const auto nh{l.extract(l.begin())}; //must be correctly deallocated
        assert(!nh.empty());
    }

    assert(allocations == 0);
    assert(constructions == 0);
}

void test_forward_list() {
    {
        std::forward_list<int, allocator<int>> l{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        assert(size(l) == 10);
        assert(allocations >= size(l));

        no_allocation_scope([&] {
            auto nh_front{l.extract_after(l.before_begin())};
            assert(nh_front.value() == 0);
            assert(size(l) == 9);
            assert(test_equal(l, {1, 2, 3, 4, 5, 6, 7, 8, 9}));

            auto nh_back{l.extract_after(std::next(l.begin(), static_cast<std::ptrdiff_t>(size(l)) - 2))};
            assert(nh_back.value() == 9);
            assert(size(l) == 8);
            assert(test_equal(l, {1, 2, 3, 4, 5, 6, 7, 8}));

            auto nh_middle{l.extract_after(std::next(l.begin(), 3))};
            assert(nh_middle.value() == 5);
            assert(size(l) == 7);
            assert(test_equal(l, {1, 2, 3, 4, 6, 7, 8}));


            l.insert_after(l.before_begin(), std::move(nh_back));
            assert(nh_back.empty());
            assert(size(l) == 8);
            assert(test_equal(l, {9, 1, 2, 3, 4, 6, 7, 8}));

            l.insert_after(std::next(l.begin(), static_cast<std::ptrdiff_t>(size(l)) - 1), std::move(nh_front));
            assert(nh_front.empty());
            assert(size(l) == 9);
            assert(test_equal(l, {9, 1, 2, 3, 4, 6, 7, 8, 0}));

            l.insert_after(std::next(l.begin(), 3), std::move(nh_middle));
            assert(nh_middle.empty());
            assert(size(l) == 10);
            assert(test_equal(l, {9, 1, 2, 3, 5, 4, 6, 7, 8, 0}));
        });

        std::forward_list<int, allocator<int>> empty;
        no_allocation_scope([&] {
            auto nh{l.extract_after(l.before_begin())};
            assert(nh.value() == 9);
            empty.insert_after(empty.before_begin(), std::move(nh));
            assert(test_equal(empty, {9}));
        });

        const auto nh{l.extract_after(l.before_begin())}; //must be correctly deallocated
        assert(!nh.empty());
    }

    assert(allocations == 0);
    assert(constructions == 0);
}

int main() {
    test_list();
    test_forward_list();
}
