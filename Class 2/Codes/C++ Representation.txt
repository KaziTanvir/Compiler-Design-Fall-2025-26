#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cctype>
#include <algorithm>

using namespace std;

// ---------- Tokenizer ----------
vector<string> tokenizeLine(const string& line) {
    vector<string> tokens;
    size_t i = 0;
    while (i < line.size()) {
        char ch = line[i];

        // Skip whitespace
        if (isspace(static_cast<unsigned char>(ch))) {
            ++i;
            continue;
        }

        // Strings in double quotes: keep as single token, including quotes
        if (ch == '"') {
            size_t j = i + 1;
            while (j < line.size() && line[j] != '"') j++;
            // Include closing quote if found
            if (j < line.size() && line[j] == '"') j++;
            tokens.push_back(line.substr(i, j - i));
            i = j;
            continue;
        }

        // Single-character tokens
        if (ch == '(' || ch == ')' || ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == ',' ) {
            tokens.push_back(string(1, ch));
            ++i;
            continue;
        }

        // Alnum/underscore sequences
        if (isalnum(static_cast<unsigned char>(ch)) || ch == '_') {
            size_t j = i + 1;
            while (j < line.size() && (isalnum(static_cast<unsigned char>(line[j])) || line[j] == '_')) j++;
            tokens.push_back(line.substr(i, j - i));
            i = j;
            continue;
        }

        // Fallback: treat any other single char as its own token
        tokens.push_back(string(1, ch));
        ++i;
    }
    return tokens;
}

vector<string> tokenizeFile(const string& filename) {
    ifstream fin(filename);
    vector<string> all;
    if (!fin.is_open()) {
        cerr << "Error: cannot open " << filename << "\n";
        return all;
    }
    string line;
    while (getline(fin, line)) {
        auto t = tokenizeLine(line);
        all.insert(all.end(), t.begin(), t.end());
        // Add a newline marker between lines (optional, not used in parsing)
        all.push_back("\n");
    }
    fin.close();
    return all;
}

// ---------- Helpers ----------
bool isNewlineToken(const string& t) { return t == "\n"; }
bool isTypeKeyword(const string& t) { return (t == "integer" || t == "float" || t == "string"); }
string cppType(const string& t) {
    if (t == "integer") return "int";
    if (t == "float")   return "float";
    if (t == "string")  return "std::string";
    return t;
}

// Consume until end of line or EOF; join tokens as expression
string readExprUntilEOL(const vector<string>& tok, size_t& i) {
    ostringstream oss;
    while (i < tok.size() && !isNewlineToken(tok[i])) {
        // skip nothing; we want raw expression, but ignore stray "te" if any
        if (tok[i] == "te") { ++i; continue; }
        oss << tok[i];
        // Add spacing between identifiers/numbers/strings to avoid accidental merging,
        // but no spaces around operators or parentheses
        if (i + 1 < tok.size() && !isNewlineToken(tok[i+1])) {
            const string& next = tok[i+1];
            bool needSpace = false;
            // Add space if both current and next are "word-like" or strings
            auto wordLike = [](const string& s){
                if (s.size() >= 2 && s.front()=='"' && s.back()=='"') return true; // string literal
                return !s.empty() && (isalnum(static_cast<unsigned char>(s[0])) || s[0]=='_');
            };
            if (wordLike(tok[i]) && wordLike(next)) needSpace = true;
            if (needSpace) oss << ' ';
        }
        ++i;
    }
    return oss.str();
}

// Split dekhao(...) args by top-level commas
vector<string> splitArgs(const vector<string>& tok, size_t& i) {
    vector<string> args;
    int depth = 0;
    ostringstream cur;

    auto flushArg = [&](){
        string s = cur.str();
        // trim
        s.erase(s.begin(), find_if(s.begin(), s.end(), [](unsigned char c){return !isspace(c);} ));
        s.erase(find_if(s.rbegin(), s.rend(), [](unsigned char c){return !isspace(c);} ).base(), s.end());
        if (!s.empty()) args.push_back(s);
        cur.str(""); cur.clear();
    };

    // Expect '(' already consumed, now gather until matching ')'
    while (i < tok.size()) {
        const string& t = tok[i];
        if (t == "(") { depth++; cur << t; }
        else if (t == ")") {
            if (depth == 0) { // end of arg list
                flushArg();
                ++i; // consume ')'
                break;
            } else {
                depth--;
                cur << t;
            }
        } else if (t == "," && depth == 0) {
            flushArg();
        } else if (!isNewlineToken(t)) {
            cur << t;
            // spacing heuristic for readability
            if (i+1 < tok.size()) {
                const string& nxt = tok[i+1];
                bool curWord = (!t.empty() && (isalnum(static_cast<unsigned char>(t[0])) || t[0]=='_' || t[0]=='"'));
                bool nxtWord = (!nxt.empty() && (isalnum(static_cast<unsigned char>(nxt[0])) || nxt[0]=='_' || nxt[0]=='"'));
                if (curWord && nxtWord) cur << ' ';
            }
        }
        ++i;
    }

    return args;
}

// ---------- Parser / Transpiler ----------
string transpile(const vector<string>& tok) {
    ostringstream out;
    bool usesString = false;
    bool usesFloat  = false;

    // We'll emit a minimal C++ program
    vector<string> body;

    // Parse line-by-line using newline tokens as separators
    size_t i = 0;
    while (i < tok.size()) {
        // Skip leading newlines
        while (i < tok.size() && isNewlineToken(tok[i])) ++i;
        if (i >= tok.size()) break;

        // Peek token to detect kind of statement
        if (i < tok.size() && tok[i] == "dekhao") {
            // dekhao ( arg1, arg2, ... )
            ++i;
            // next should be "("
            if (i < tok.size() && tok[i] == "(") {
                ++i;
                auto args = splitArgs(tok, i); // consumes up to ')'
                // Consume until end of line
                while (i < tok.size() && !isNewlineToken(tok[i])) ++i;

                ostringstream stmt;
                stmt << "std::cout";
                for (auto &a : args) {
                    stmt << " << " << a;
                }
                stmt << " << std::endl;";
                body.push_back(stmt.str());
            } else {
                // malformed; skip to EOL
                while (i < tok.size() && !isNewlineToken(tok[i])) ++i;
            }
        }
        else if (i+3 < tok.size() && isTypeKeyword(tok[i])) {
            // <type> <id> te <expr...>
            string tkw = tok[i++]; // type keyword
            string id  = (i < tok.size() ? tok[i++] : "");
            string maybeTe = (i < tok.size() ? tok[i] : "");
            if (maybeTe == "te") ++i; // consume 'te'

            string expr = readExprUntilEOL(tok, i);

            if (tkw == "string") usesString = true;
            if (tkw == "float")  usesFloat  = true;

            ostringstream stmt;
            stmt << cppType(tkw) << " " << id << " = " << expr << ";";
            body.push_back(stmt.str());
        }
        else {
            // Unrecognized line: just skip to EOL
            while (i < tok.size() && !isNewlineToken(tok[i])) ++i;
        }

        // consume trailing newlines between statements
        while (i < tok.size() && isNewlineToken(tok[i])) ++i;
    }

    // Headers
    out << "#include <iostream>\n";
    if (usesString) out << "#include <string>\n";
    out << "using namespace std;\n\n";
    out << "int main() {\n";
    for (auto& s : body) out << "    " << s << "\n";
    out << "    return 0;\n}\n";
    return out.str();
}

int main() {
    const string inputFile = "input.txt";

    // 1) Tokenize
    auto tokens = tokenizeFile(inputFile);
    if (tokens.empty()) {
        cerr << "No tokens (or failed to read input).\n";
        return 1;
    }

    // 2) Print tokens
    cout << "Tokens:\n";
    for (const auto& t : tokens) {
        if (t == "\n") cout << "\n";
        else cout << "[" << t << "] ";
    }
    cout << "\n\n";

    // 3) Transpile to C++
    string cppCode = transpile(tokens);

    // 4) Show C++ equivalent in console
    cout << "===== Generated C++ =====\n";
    cout << cppCode << "\n";

    // 5) (Optional) Write to file for convenience
    ofstream gout("generated.cpp");
    if (gout.is_open()) {
        gout << cppCode;
        gout.close();
        cout << "Written to generated.cpp\n";
    } else {
        cerr << "Warning: could not write generated.cpp\n";
    }

    return 0;
}
