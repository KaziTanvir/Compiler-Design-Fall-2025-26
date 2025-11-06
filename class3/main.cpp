#include <bits/stdc++.h>
using namespace std;

// ===== Tokens =====
enum class TokType {
    KW_INTEGER, KW_DEKHAO, KW_TE,
    IDENT, NUMBER,
    PLUS, MINUS, STAR, SLASH,
    LPAREN, RPAREN,
    END
};

struct Token { TokType type; string lexeme; int line; };

struct LexResult { vector<Token> tokens; vector<string> warnings; };

class Lexer {
public:
    static LexResult lexLine(const string& s, int lineNo) {
        size_t i = 0; vector<Token> toks; vector<string> warns;
        auto push = [&](TokType t, string lx){ toks.push_back({t,lx,lineNo}); };
        auto isIdStart = [](char c){ return isalpha((unsigned char)c) || c=='_'; };
        auto isId = [](char c){ return isalnum((unsigned char)c) || c=='_'; };

        while (i < s.size()) {
            char c = s[i];
            if (isspace((unsigned char)c)) { ++i; continue; }
            if (isdigit((unsigned char)c)) {
                size_t j=i; while (j<s.size() && isdigit((unsigned char)s[j])) ++j;
                push(TokType::NUMBER, s.substr(i,j-i)); i=j; continue;
            }
            if (isIdStart(c)) {
                size_t j=i; while (j<s.size() && isId(s[j])) ++j;
                string w = s.substr(i,j-i), lw=w; for (auto& ch:lw) ch=tolower(ch);
                if (lw=="integer"||lw=="interger"){ if(lw=="interger") warns.push_back("Line "+to_string(lineNo)+": 'interger' treated as 'integer'."); push(TokType::KW_INTEGER,w);}
                else if (lw=="dekhao") push(TokType::KW_DEKHAO,w);
                else if (lw=="te") push(TokType::KW_TE,w);
                else push(TokType::IDENT,w);
                i=j; continue;
            }
            switch(c){
                case '+': push(TokType::PLUS,"+"); break;
                case '-': push(TokType::MINUS,"-"); break;
                case '*': push(TokType::STAR,"*"); break;
                case '/': push(TokType::SLASH,"/"); break;
                case '(': push(TokType::LPAREN,"("); break;
                case ')': push(TokType::RPAREN,")"); break;
                default: warns.push_back("Line "+to_string(lineNo)+": skipping unknown character '"+string(1,c)+"'."); break;
            }
            ++i;
        }
        toks.push_back({TokType::END,"",lineNo});
        return {toks,warns};
    }
};

// ===== AST =====
struct Node { virtual ~Node()=default; int line=0; };

struct Expr : Node {};
struct Stmt : Node {};

struct Number : Expr { int value; explicit Number(int v){value=v;} };
struct Ident  : Expr { string name; explicit Ident(string n){name=std::move(n);} };

struct Binary : Expr {
    string op; unique_ptr<Expr> left, right;
    Binary(string o, unique_ptr<Expr> l, unique_ptr<Expr> r)
        : op(std::move(o)), left(std::move(l)), right(std::move(r)) {}
};

struct Decl : Stmt {
    string name; int value;
    Decl(string n,int v){name=std::move(n); value=v;}
};

struct Print : Stmt {
    unique_ptr<Expr> expr; explicit Print(unique_ptr<Expr> e):expr(std::move(e)){}
};

// ===== Parser =====
class Parser {
public:
    explicit Parser(const vector<Token>& t) : toks(t) {}
    unique_ptr<Stmt> parseStatement(string& err) {
        if (match(TokType::KW_INTEGER)) return parseDecl(err);
        if (match(TokType::KW_DEKHAO))  return parsePrint(err);
        err = here()+"Expected 'integer' or 'dekhao'."; return nullptr;
    }
    bool atEnd() const { return peek().type==TokType::END; }
private:
    const vector<Token>& toks; size_t i=0;
    const Token& peek(size_t k=0) const { return toks[min(i+k, toks.size()-1)]; }
    bool check(TokType t,size_t k=0) const { return peek(k).type==t; }
    const Token& advance(){ if(!atEnd()) ++i; return toks[i-1]; }
    bool match(TokType t){ if(check(t)){ advance(); return true; } return false; }
    string here() const { return "Line "+to_string(peek().line)+": "; }

    unique_ptr<Stmt> parseDecl(string& err){
        if(!check(TokType::IDENT)){ err=here()+"Expected identifier after 'integer'."; return nullptr; }
        auto idTok=advance(); string name=idTok.lexeme;
        if(!match(TokType::KW_TE)){ err=here()+"Expected 'te' after identifier."; return nullptr; }
        if(!check(TokType::NUMBER)){ err=here()+"Expected integer literal after 'te'."; return nullptr; }
        int val=stoi(advance().lexeme);
        auto d=make_unique<Decl>(name,val); d->line=idTok.line;
        if(!atEnd()){ err=here()+"Unexpected tokens after declaration."; return nullptr; }
        return d;
    }
    unique_ptr<Stmt> parsePrint(string& err){
        int ln=peek().line;
        if(!match(TokType::LPAREN)){ err=here()+"Expected '(' after 'dekhao'."; return nullptr; }
        auto e=parseExpr(err); if(!e) return nullptr;
        if(!match(TokType::RPAREN)){ err=here()+"Expected ')' after expression."; return nullptr; }
        if(!atEnd()){ err=here()+"Unexpected tokens after print statement."; return nullptr; }
        auto p=make_unique<Print>(std::move(e)); p->line=ln; return p;
    }

    unique_ptr<Expr> parseExpr(string& err){
        auto left=parseTerm(err); if(!left) return nullptr;
        while(check(TokType::PLUS)||check(TokType::MINUS)){
            string op=advance().lexeme;
            auto right=parseTerm(err); if(!right) return nullptr;
            auto n=make_unique<Binary>(op,std::move(left),std::move(right)); n->line=peek().line; left=std::move(n);
        }
        return left;
    }
    unique_ptr<Expr> parseTerm(string& err){
        auto left=parseFactor(err); if(!left) return nullptr;
        while(check(TokType::STAR)||check(TokType::SLASH)){
            string op=advance().lexeme;
            auto right=parseFactor(err); if(!right) return nullptr;
            auto n=make_unique<Binary>(op,std::move(left),std::move(right)); n->line=peek().line; left=std::move(n);
        }
        return left;
    }
    unique_ptr<Expr> parseFactor(string& err){
        if(check(TokType::IDENT)){ auto t=advance(); auto e=make_unique<Ident>(t.lexeme); e->line=t.line; return e; }
        if(check(TokType::NUMBER)){ auto t=advance(); auto e=make_unique<Number>(stoi(t.lexeme)); e->line=t.line; return e; }
        if(match(TokType::LPAREN)){ auto e=parseExpr(err); if(!e) return nullptr; if(!match(TokType::RPAREN)){ err=here()+"Expected ')'."; return nullptr; } return e; }
        err=here()+"Expected identifier, number, or '('."; return nullptr;
    }
};

// ===== Semantic annotations =====
enum class Type { Int, Unknown };

struct Annotation {
    Type type = Type::Unknown;
    bool isConst = false;
    long long constVal = 0;
    const Decl* resolvedDecl = nullptr; // for identifiers
};

struct Semantic {
    unordered_map<const Node*, Annotation> ann;
    unordered_map<string, const Decl*> sym;  // single global scope
    vector<string> errors;
    vector<string> notes;

    void analyze(vector<unique_ptr<Stmt>>& prog) {
        // 1) collect decls
        for (auto& s: prog) if (auto d = dynamic_cast<Decl*>(s.get())) {
            if (sym.count(d->name)) {
                errors.push_back(loc(d)+"Redeclaration of '"+d->name+"'.");
            } else sym[d->name] = d;
            Annotation A; A.type=Type::Int; A.isConst=true; A.constVal=d->value;
            ann[d]=A;
        }

        // 2) analyze statements
        for (auto& s: prog) {
            if (auto p = dynamic_cast<Print*>(s.get())) {
                analyzeExpr(p->expr.get());
                // print node annotation: type must be Int
                Annotation A; A.type = get(p->expr.get()).type;
                A.isConst = get(p->expr.get()).isConst;
                if (A.isConst) A.constVal = get(p->expr.get()).constVal;
                ann[p]=A;
            }
        }
    }

    void analyzeExpr(Expr* e){
        if (ann.count(e)) return;
        if (auto n = dynamic_cast<Number*>(e)) {
            Annotation A; A.type=Type::Int; A.isConst=true; A.constVal=n->value; ann[e]=A; return;
        }
        if (auto id = dynamic_cast<Ident*>(e)) {
            Annotation A;
            auto it = sym.find(id->name);
            if (it==sym.end()) {
                errors.push_back(loc(id)+"Use of undeclared identifier '"+id->name+"'.");
                A.type=Type::Unknown;
            } else {
                A.type=Type::Int; A.isConst=true; A.constVal=it->second->value; A.resolvedDecl=it->second;
            }
            ann[e]=A; return;
        }
        if (auto b = dynamic_cast<Binary*>(e)) {
            analyzeExpr(b->left.get());
            analyzeExpr(b->right.get());
            Annotation L=get(b->left.get()), R=get(b->right.get()), A;
            if (L.type==Type::Int && R.type==Type::Int) {
                A.type = Type::Int;
                // constant fold if both const
                if (L.isConst && R.isConst) {
                    A.isConst = true;
                    if (b->op=="+") A.constVal = L.constVal + R.constVal;
                    else if (b->op=="-") A.constVal = L.constVal - R.constVal;
                    else if (b->op=="*") A.constVal = L.constVal * R.constVal;
                    else if (b->op=="/") {
                        if (R.constVal==0) {
                            errors.push_back(loc(b)+"Division by zero in constant expression.");
                            A.isConst=false;
                        } else A.constVal = L.constVal / R.constVal;
                    }
                }
            } else {
                A.type = Type::Unknown;
                errors.push_back(loc(b)+"Type error: operands must be integers.");
            }
            ann[e]=A; return;
        }
        // fallback
        ann[e]=Annotation{};
    }

    const Annotation& get(const Node* n) const {
        static Annotation empty;
        auto it=ann.find(n); return it==ann.end()?empty:it->second;
    }

    static string tstr(Type t){ return t==Type::Int? "int" : "unknown"; }

    static string loc(const Node* n){
        int ln = n? n->line:0; if(!ln) return ""; return "Line "+to_string(ln)+": ";
    }
};

// ===== Pretty printers =====
struct ASTPrinter {
    static void print(const vector<unique_ptr<Stmt>>& program, const Semantic& S) {
        cout << "=== Annotated Semantic Tree ===\n";
        int i=1; for (auto& s: program) {
            cout << "Stmt " << i++ << ":\n";
            printStmt(*s, S, 2);
        }
    }
    static void printStmt(const Stmt& s, const Semantic& S, int indent){
        string pad(indent,' ');
        if (auto d = dynamic_cast<const Decl*>(&s)) {
            auto A=S.get(&s);
            cout << pad << "Decl(integer)  :: type=" << Semantic::tstr(A.type)
                 << ", const=" << (A.isConst? "true ("+to_string(A.constVal)+")":"false") << "\n";
            cout << pad << "  name: " << d->name << "\n";
            cout << pad << "  value: " << d->value << "\n";
        } else if (auto p = dynamic_cast<const Print*>(&s)) {
            auto A=S.get(&s);
            cout << pad << "Print(dekhao)  :: expr.type=" << Semantic::tstr(A.type);
            if (A.isConst) cout << ", expr.const=" << A.constVal;
            cout << "\n";
            cout << pad << "  expr:\n";
            printExpr(*p->expr, S, indent+4);
        }
    }
    static void printExpr(const Expr& e, const Semantic& S, int indent){
        string pad(indent,' ');
        auto A=S.get(&e);
        if (auto n = dynamic_cast<const Number*>(&e)) {
            cout << pad << "Number(" << n->value << ")  :: type=" << Semantic::tstr(A.type)
                 << ", const=" << (A.isConst? "true ("+to_string(A.constVal)+")":"false") << "\n";
        } else if (auto id = dynamic_cast<const Ident*>(&e)) {
            cout << pad << "Ident(" << id->name << ")  :: type=" << Semantic::tstr(A.type);
            if (A.resolvedDecl) cout << ", binds→" << A.resolvedDecl->name;
            if (A.isConst) cout << ", const=" << A.constVal;
            cout << "\n";
        } else if (auto b = dynamic_cast<const Binary*>(&e)) {
            cout << pad << "BinaryOp(" << b->op << ")  :: type=" << Semantic::tstr(A.type);
            if (A.isConst) cout << ", const=" << A.constVal;
            cout << "\n";
            cout << pad << "  left:\n";  printExpr(*b->left,  S, indent+4);
            cout << pad << "  right:\n"; printExpr(*b->right, S, indent+4);
        } else {
            cout << pad << "<expr?> :: type=" << Semantic::tstr(A.type) << "\n";
        }
    }
};

// ===== DOT with annotations =====
struct DOT {
    ofstream out; int nextId=0;
    explicit DOT(const string& path):out(path){ out<<"digraph AnnotatedAST {\n  node [shape=box];\n"; }
    ~DOT(){ out<<"}\n"; }
    static string esc(string s){ for(char& c:s) if(c=='"') c='\''; return s; }
    int node(const string& label){ int id=nextId++; out<<"  n"<<id<<" [label=\""<<esc(label)<<"\"];\n"; return id; }
    void edge(int a,int b,const string& el=""){ out<<"  n"<<a<<" -> n"<<b; if(!el.empty()) out<<" [label=\""<<esc(el)<<"\"]"; out<<";\n"; }

    int emitStmt(const Stmt& s, const Semantic& S){
        if (auto d=dynamic_cast<const Decl*>(&s)){
            auto A=S.get(&s);
            int r=node("Decl\\n(integer)\\n:type="+Semantic::tstr(A.type)+"\\nconst="+(A.isConst?("true("+to_string(A.constVal)+")"):"false"));
            int n1=node("name="+d->name), n2=node("value="+to_string(d->value));
            edge(r,n1); edge(r,n2); return r;
        } else if (auto p=dynamic_cast<const Print*>(&s)){
            auto A=S.get(&s);
            int r=node("Print\\n(dekhao)\\nexpr.type="+Semantic::tstr(A.type)+(A.isConst?("\\nexpr.const="+to_string(A.constVal)):""));
            int e=emitExpr(*p->expr,S); edge(r,e,"expr"); return r;
        }
        return node("<stmt?>");
    }

    int emitExpr(const Expr& e, const Semantic& S){
        auto A=S.get(&e);
        if (auto n=dynamic_cast<const Number*>(&e)) {
            return node("Number\\n"+to_string(n->value)+"\\n:type="+Semantic::tstr(A.type)+"\\nconst="+(A.isConst?("true("+to_string(A.constVal)+")"):"false"));
        } else if (auto id=dynamic_cast<const Ident*>(&e)) {
            string lbl = "Ident\\n"+id->name+"\\n:type="+Semantic::tstr(A.type);
            if (A.resolvedDecl) lbl += "\\nbinds→"+A.resolvedDecl->name;
            if (A.isConst) lbl += "\\nconst="+to_string(A.constVal);
            return node(lbl);
        } else if (auto b=dynamic_cast<const Binary*>(&e)) {
            string lbl="BinaryOp\\n"+b->op+"\\n:type="+Semantic::tstr(A.type); if(A.isConst) lbl+="\\nconst="+to_string(A.constVal);
            int r=node(lbl), L=emitExpr(*b->left,S), R=emitExpr(*b->right,S);
            edge(r,L,"left"); edge(r,R,"right"); return r;
        }
        return node("<expr?>");
    }
};

static string trim(const string& s){
    auto l=s.find_first_not_of(" \t\r\n"), r=s.find_last_not_of(" \t\r\n");
    if (l==string::npos) return ""; return s.substr(l,r-l+1);
}

int main(){
    ifstream fin("input.txt");
    if (!fin){ cerr<<"Error: could not open input.txt\n"; return 1; }

    vector<unique_ptr<Stmt>> program;
    vector<string> warnings;
    string line; int lineNo=1;

    cout<<"=== Lexical Tokens ===\n";
    while (getline(fin,line)) {
        string t = trim(line);
        if (t.empty()) { ++lineNo; continue; }
        auto L = Lexer::lexLine(t, lineNo);
        warnings.insert(warnings.end(), L.warnings.begin(), L.warnings.end());
        for (auto &tk : L.tokens) if (tk.type!=TokType::END) cout<<"Line "<<tk.line<<" -> "<<tk.lexeme<<"\n";
        Parser P(L.tokens); string err;
        auto stmt = P.parseStatement(err);
        if (!stmt){ cerr<<"Syntax error: "<<err<<"\n"; return 2; }
        program.push_back(std::move(stmt));
        ++lineNo;
    }

    // Semantic analysis
    Semantic sem; sem.analyze(program);

    // Report
    if (!warnings.empty()) {
        cout << "\n=== Warnings ===\n";
        for (auto& w : warnings) cout << w << "\n";
    }
    if (!sem.errors.empty()) {
        cout << "\n=== Semantic Errors ===\n";
        for (auto& e : sem.errors) cout << e << "\n";
        // continue to print what we have
    }

    cout << "\n";
    ASTPrinter::print(program, sem);

    // DOT
    DOT dot("annotated_ast.dot");
    int programNode = dot.node("Program");
    int idx=1;
    for (auto& s: program) {
        int r = dot.emitStmt(*s, sem);
        dot.edge(programNode, r, "stmt"+to_string(idx++));
    }

    // If last statement is Print and expr folded, show its computed value
    if (!program.empty()) {
        if (auto p = dynamic_cast<Print*>(program.back().get())) {
            auto A = sem.get(p->expr.get());
            if (A.type==Type::Int && A.isConst) {
                cout << "\n=== Evaluation (constant-folded) ===\n";
                cout << "dekhao(...) = " << A.constVal << "\n";
            }
        }
    }

    return 0;
}
