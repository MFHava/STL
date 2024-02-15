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
    }

    template <class U>
    void destroy(U* ptr) {
        std::allocator<T> alloc;
        std::allocator_traits<std::allocator<T>>::destroy(alloc, ptr);
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

template<typename List>
bool test_equal(const List & l, std::initializer_list<int> ilist) {
    //check bidirectional to ensure all internal pointers are correctly set up
    return std::equal(l.begin(), l.end(), ilist.begin(), ilist.end())
        && std::equal(l.rbegin(), l.rend(), std::make_reverse_iterator(ilist.end()), std::make_reverse_iterator(ilist.begin()));
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
}

void test_forward_list() {
    //TODO: init std::forward_list with elements
    //TODO: try extract at front
    //TODO: try extract at back
    //TODO: try extract in the middle
    //TODO: try insert at front
    //TODO: try insert at back
    //TODO: try insert at middle
}

int main() {
    test_list();
    test_forward_list();
}
