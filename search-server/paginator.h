#pragma once
#include <iostream>
#include <vector>

template <typename Iterator>
class IteratorRange {
public:
    IteratorRange(Iterator page_begin, Iterator page_end)
        : page_begin_(page_begin), page_end_(page_end), size_(distance(page_begin, page_end))
    {
    }
    Iterator begin() const {
        return page_begin_;
    }

    Iterator end() const {
        return page_end_;
    }

    size_t size() const {
        return size_;
    }

private:
    Iterator page_begin_;
    Iterator page_end_;
    size_t size_;
};

template <typename Iterator>
std::ostream& operator<<(std::ostream& os, const IteratorRange<Iterator>& page) {
    for (Iterator it = page.begin(); it != page.end(); ++it) {
        os << *it;
    }
    return os;
}

template <typename Iterator>
class Paginator {
public:
    Paginator(Iterator result_begin, Iterator result_end, size_t page_size) {
        for (size_t i = distance(result_begin, result_end); i > 0;) {
            page_size = std::min(page_size, i);
            const Iterator page_end = next(result_begin, page_size);
            pages_.push_back({ result_begin, page_end });

            i -= page_size;
            result_begin = page_end;
        }
    }

    auto begin() const {
        return pages_.begin();
    }

    auto end() const {
        return pages_.end();
    }

    size_t size() const {
        return pages_.size();
    }

private:
    std::vector<IteratorRange<Iterator>> pages_;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}