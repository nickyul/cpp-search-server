#include <algorithm>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <cmath>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {   // считать строку
	string s;
	getline(cin, s);
	return s;
}

int ReadLineWithNumber() { // считать строку
	int result;
	cin >> result;
	ReadLine();
	return result;
}

vector<string> SplitIntoWords(const string& text) {        // вернуть вектор слов из строки
	vector<string> words;
	string word;
	for (const char c : text) {
		if (c == ' ') {
			words.push_back(word);
			word = "";
		}
		else {
			word += c;
		}
	}
	words.push_back(word);

	return words;
}

struct Document {
	int id;
	double relevance;
};

class SearchServer {

public:

	void SetStopWords(const string& text) {                  // заполняет поле класса stor_words_ словами из строки стоп-слов
		for (const string& word : SplitIntoWords(text)) {
			stop_words_.insert(word);
		}
	}

	void AddDocument(int document_id, const string& document) {   // заполнение поля класса words_in_documents_ словами из строк документов,
		document_count_ = document_count_ + 1;                    // значением по слову является словарь (id - tf)
		vector <string> words = SplitIntoWordsNoStop(document);
		for (const string& word : words) {
			words_in_documents_[word][document_id] = words_in_documents_[word][document_id] + 1.0 / words.size();
		}
	}

	vector<Document> FindTopDocuments(const string& raw_query) const {    // возвращает сортированный результат поиска наиболее релевантных документов
		const Query query = ParseQuery(raw_query);
		vector <Document> matched_documents = FindAllDocuments(query);

		sort(matched_documents.begin(), matched_documents.end(),
			[](const Document& lhs, const Document& rhs) {
				return lhs.relevance > rhs.relevance;
			});
		if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
			matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
		}
		return matched_documents;
	}

private:
	map < string, map <int, double>> words_in_documents_;
	set<string> stop_words_;
	int document_count_ = 0;

	bool IsStopWord(const string& word) const {         // проверка слова на нахождение его в множестве стоп-слов
		return stop_words_.count(word) > 0;
	}

	struct QueryWord {
		string d;
		bool minus;
		bool stop;
	};

	vector<string> SplitIntoWordsNoStop(const string& text) {     // возвращает вектор слов из текста, которые не входят в множество стоп-слов
		vector<string> words;
		for (const string& word : SplitIntoWords(text)) {
			if (!IsStopWord(word)) {
				words.push_back(word);
			}
		}
		return words;
	}

	QueryWord ParseQueryWord(string text) const { // проверка слова на первый символ (минус-слово или плюс-слово)
		bool minus = false;
		if (text[0] == '-') {
			minus = true;
			text = text.substr(1);
		}
		return {
		text,
		minus,
		IsStopWord(text)
		};
	}

	struct Query {
		vector<string> plus_words;
		vector<string> minus_words;
	};

	Query ParseQuery(const string& text) const { // парсинг запроса
		Query query;
		for (const string& word : SplitIntoWords(text)) {
			const QueryWord query_word = ParseQueryWord(word);
			if (!query_word.stop) {
				if (query_word.minus) {
					query.minus_words.push_back(query_word.d);
				}
				else {
					query.plus_words.push_back(query_word.d);
				}
			}
		}
		return query;
	}

	vector<Document> FindAllDocuments(const Query& query) const {   // возвращает релевантность и id документов по запросу
		map<int, double> document_to_relevance;
		for (const string& word : query.plus_words) {
			if (!words_in_documents_.count(word) == 0) {
				double znamenatel = words_in_documents_.at(word).size();
				for (auto const& [id, tf] : words_in_documents_.at(word)) {
					document_to_relevance[id] = (log(document_count_ / znamenatel) * tf) + document_to_relevance[id];
				}
			}
		}

		for (const string& word : query.minus_words) {
			if (!words_in_documents_.count(word) == 0) {
				for (auto const& [id, tf] : words_in_documents_.at(word)) {
					document_to_relevance.erase(id);
				}
			}
		}
		vector<Document> matched_documents;
		for (auto const& [document_id, relevance] : document_to_relevance) {
			matched_documents.push_back({ document_id, relevance });
		}
		return matched_documents;
	}
};

SearchServer CreateSearchServer() {
	SearchServer search_server;
	search_server.SetStopWords(ReadLine());

	const int document_count = ReadLineWithNumber();
	for (int document_id = 0; document_id < document_count; ++document_id) {
		search_server.AddDocument(document_id, ReadLine());
	}

	return search_server;
}

int main() {
	const SearchServer search_server = CreateSearchServer();

	const string query = ReadLine();
	for (auto const& [document_id, relevance] : search_server.FindTopDocuments(query)) {
		cout << "{ document_id = "s << document_id << ", " << "relevance = "s << relevance << " }"s << endl;
	}

	return 0;
}