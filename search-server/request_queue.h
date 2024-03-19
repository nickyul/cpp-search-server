#pragma once
#include <deque>
#include "search_server.h"

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);

    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate);

    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status = DocumentStatus::ACTUAL) {
        return AddFindRequest(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {return document_status == status; });
    }

    int GetNoResultRequests() const;

private:
    struct QueryResult {
        std::vector<Document> result_of_request;
    };
    const SearchServer& search_server_;
    std::deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    int count_no_result_request = 0;
};

template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {

    auto result_of_request = search_server_.FindTopDocuments(raw_query, document_predicate);
    if (requests_.size() >= min_in_day_) {
        if (requests_[0].result_of_request.size() != 0) {
            --count_no_result_request;
            requests_.pop_back();
        }
    }
    if (result_of_request.empty()) {
        ++count_no_result_request;
    }
    requests_.push_front({ result_of_request });
    return result_of_request;
}
