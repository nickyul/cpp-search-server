#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <optional>
#include <numeric>


using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

const double MIN_RELEVANCE_DIFF = 1e-6;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

bool IsQueryWordWithoutUnwantedMinus(const string& word) {
    if (word.size() == 1) {
        if (word[0] == '-') {
            return false;
        }
    }
    else {
        if ((word[0] == '-' && word[1] == '-') || word[word.size() - 1] == '-') {
            return false;
        }
    }
    return true;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        }
        else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    Document() = default;

    Document(int id, double relevance, int rating)
        : id(id)
        , relevance(relevance)
        , rating(rating) {
    }

    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

template <typename StringContainer>
set<string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    set<string> non_empty_strings;
    for (const string& str : strings) {
        if (!str.empty()) {
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;
}

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words)
        :stop_words_(MakeUniqueNonEmptyStrings(stop_words))
    {
        for (const string& stop_word : stop_words_) {
            if (!IsValidWord(stop_word)) {
                throw invalid_argument("Стоп-слова содержат недопустимые символы");
            }
        }
    }

    explicit SearchServer(const string& stop_words_text)
        : SearchServer(SplitIntoWords(stop_words_text))
    {
    }

    inline static constexpr int INVALID_DOCUMENT_ID = -1;

    int GetDocumentId(int index) const {
        if (index > documents_id_.size()) {
            throw out_of_range("Запрошенного id не существует");
        }
        return documents_id_[index];
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {

        if (documents_.count(document_id) || document_id < 0) {
            throw invalid_argument("Попытка добавить документ с отрицательным или уже присутствующим id");
        }

        vector<string> words;
        if (SplitIntoWordsNoStop(document, words)) {
            documents_id_.push_back(document_id);
            const double inv_word_count = 1.0 / words.size();
            for (const string& word : words) {
                word_to_document_freqs_[word][document_id] += inv_word_count;
            }
            documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
        }
        else {
            throw invalid_argument("В тексте присутствуют недопустимые символы");
        }
    }

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const {
        Query query = ParseQuery(raw_query);
        vector<Document> result = FindAllDocuments(query, document_predicate);
        sort(result.begin(), result.end(),
            [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < MIN_RELEVANCE_DIFF) {
                    return lhs.rating > rhs.rating;
                }
                else {
                    return lhs.relevance > rhs.relevance;
                }
            });
        if (result.size() > MAX_RESULT_DOCUMENT_COUNT) {
            result.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return result;
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status = DocumentStatus::ACTUAL) const {
        return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {return document_status == status; });
    }

    int GetDocumentCount() const {
        return static_cast<int>(documents_.size());
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        vector<string> matched_words;
        Query query = ParseQuery(raw_query);
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return tuple{ matched_words, documents_.at(document_id).status };
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    const set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;
    vector<int> documents_id_;

    static bool IsValidWord(const string& word) {
        return none_of(word.begin(), word.end(), [](char c) {
            return c >= '\0' && c < ' ';
            });
    }

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    bool SplitIntoWordsNoStop(const string& text, vector<string>& words) const {
        bool result = true;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsValidWord(word)) {
                result = false;
            }
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return result;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        return accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        QueryWord query_word;
        bool is_minus = false;
        if (!IsQueryWordWithoutUnwantedMinus(text) || !IsValidWord(text)) {
            throw invalid_argument("Текст запроса содержит недопустимые конструкции");
        }
        else if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return QueryWord{ text, is_minus, IsStopWord(text) };
    }


    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                }
                else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto& [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto& [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back(
                { document_id, relevance, documents_.at(document_id).rating });
        }
        return matched_documents;
    }
};

void PrintDocument(const Document& document) {
    cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating << " }"s << endl;
}

int main() {
    SearchServer search_server("и в на"s);
    try {
        cout << "begin"s << endl;
        search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
        cout << "1st doc done"s << endl;;
        //search_server.AddDocument(-1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, { 1, 2 });
        //cout << "2nd doc done"s << endl;
        //search_server.AddDocument(3, "большой пёс скво\x12рец"s, DocumentStatus::ACTUAL, { 1, 3, 2 });
        //cout << "3rd doc done"s << endl;
        vector<Document> documents = search_server.FindTopDocuments("кот"s);
        for (const Document& document : documents) {
            PrintDocument(document);
        }
    }

    catch (const invalid_argument& e) {
        cout << "Ошибка: "s << e.what() << endl;
    }
    catch (...) {
        cout << "Неизвестная ошибка"s << endl;
    }
}