#include <cassert>
#include <cstddef>
#include <cstdio>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <type_traits>
#include <utility>
#include <list>
#include <forward_list>

bool allocation_allowed{true};
std::size_t allocation_count{0};
std::size_t construct_count{0};

template <class T>
struct tracked_allocator {
    using value_type = T;

    constexpr tracked_allocator() noexcept = default;
    template <class U>
    constexpr tracked_allocator(tracked_allocator<U> const&) noexcept {}

    T* allocate(size_t n) {
        assert(allocation_allowed);
        T* ptr = std::allocator<T>{}.allocate(n);
        ++allocation_count;
        return ptr;
    }

    void deallocate(T* ptr, size_t n) noexcept {
        assert(allocation_allowed);
        std::allocator<T>{}.deallocate(ptr, n);
        --allocation_count;
    }

    template <class U, class... Args>
    void construct(U* ptr, Args&&... args) {
        std::allocator<T> alloc;
        std::allocator_traits<std::allocator<T>>::construct(alloc, ptr, std::forward<Args>(args)...);
        ++construct_count;
    }

    template <class U>
    void destroy(U* ptr) {
        std::allocator<T> alloc;
        std::allocator_traits<std::allocator<T>>::destroy(alloc, ptr);
        --construct_count;
    }

    template <class U>
    bool operator==(tracked_allocator<U> const&) const noexcept {
        return true;
    }
    template <class U>
    bool operator!=(tracked_allocator<U> const&) const noexcept {
        return false;
    }
};

template<typename Func>
void no_allocation_scope(Func func) {
    allocation_allowed = false;
    func();
    allocation_allowed = true;
}

bool test_equal(const std::list<int, tracked_allocator<int>> & l, std::initializer_list<int> ilist) {
    //check bidirectional to ensure all internal pointers are correctly set up
    return std::equal(l.begin(), l.end(), ilist.begin(), ilist.end())
        && std::equal(l.rbegin(), l.rend(), std::make_reverse_iterator(ilist.end()), std::make_reverse_iterator(ilist.begin()));
}

bool test_equal(const std::forward_list<int, tracked_allocator<int>> & l, std::initializer_list<int> ilist) {
    //check bidirectional to ensure all internal pointers are correctly set up
    return std::equal(l.begin(), l.end(), ilist.begin(), ilist.end());
}

void test_list() {
    {
        std::list<int, tracked_allocator<int>> l{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        assert(l.size() == 10);
        assert(allocation_count >= l.size());

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

        std::list<int, tracked_allocator<int>> empty;
        no_allocation_scope([&] {
            auto nh{l.extract(l.begin())};
            assert(nh.value() == 9);
            empty.insert(empty.end(), std::move(nh));
            assert(test_equal(empty, {9}));
        });

        const auto nh{l.extract(l.begin())}; //must be correctly deallocated
        assert(!nh.empty());
    }

    assert(allocation_count == 0);
    assert(construct_count == 0);
}

template<typename Allocator>
auto size(const std::forward_list<int, Allocator> & l) -> std::size_t {
    return static_cast<std::size_t>(std::distance(l.begin(), l.end()));
}

void test_forward_list() {
    {
        std::forward_list<int, tracked_allocator<int>> l{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        assert(size(l) == 10);
        assert(allocation_count >= size(l));

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

        std::forward_list<int, tracked_allocator<int>> empty;
        no_allocation_scope([&] {
            auto nh{l.extract_after(l.before_begin())};
            assert(nh.value() == 9);
            empty.insert_after(empty.before_begin(), std::move(nh));
            assert(test_equal(empty, {9}));
        });

        const auto nh{l.extract_after(l.before_begin())}; //must be correctly deallocated
        assert(!nh.empty());
    }

    assert(allocation_count == 0);
    assert(construct_count == 0);
}

int main() {
    test_list();
    test_forward_list();
}
