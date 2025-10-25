// interpreter_step5.cpp
#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <regex>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <stdexcept>

enum class Type { INT, FLOAT };

struct Env {
    std::unordered_map<std::string, Type> types;
    std::unordered_map<std::string, double> values;
    bool hasVar(const std::string& n) const { return values.find(n) != values.end(); }
};

struct Parser {
    std::string s; size_t i=0; Env* env;
    Parser(const std::string& str, Env* e): s(str), env(e) {}
    void skip(){while(i<s.size()&&isspace((unsigned char)s[i]))++i;}
    bool match(char c){skip(); if(i<s.size()&&s[i]==c){++i;return true;}return false;}
    double parse_number(){
        skip(); size_t st=i; bool dot=false;
        if(i<s.size()&&(s[i]=='+'||s[i]=='-'))++i;
        while(i<s.size()&&(isdigit((unsigned char)s[i])||s[i]=='.')){
            if(s[i]=='.'){if(dot)break;dot=true;}++i;
        }
        return std::stod(s.substr(st,i-st));
    }
    std::string parse_identifier(){
        skip(); if(i>=s.size()||!(isalpha((unsigned char)s[i])||s[i]=='_'))
            throw std::runtime_error("Expected identifier");
        size_t st=i++;
        while(i<s.size()&&(isalnum((unsigned char)s[i])||s[i]=='_'))++i;
        return s.substr(st,i-st);
    }
    double factor(){
        skip();
        if(match('(')){double v=expr(); if(!match(')'))throw std::runtime_error("Missing )"); return v;}
        if(i<s.size()&&(isdigit((unsigned char)s[i])||s[i]=='+'||s[i]=='-'))return parse_number();
        std::string id=parse_identifier();
        if(!env->hasVar(id))throw std::runtime_error("Undefined variable: "+id);
        return env->values[id];
    }
    double term(){
        double v=factor();
        while(true){skip();
            if(match('*'))v*=factor();
            else if(match('/')){double r=factor(); if(fabs(r)<1e-15)throw std::runtime_error("Division by zero"); v/=r;}
            else break;
        }return v;
    }
    double expr(){
        double v=term();
        while(true){skip();
            if(match('+'))v+=term();
            else if(match('-'))v-=term();
            else break;
        }return v;
    }
};

static inline bool is_int_like(double x){return fabs(x-round(x))<1e-9;}

int main(){
    std::ifstream f("editor.txt");
    if(!f.is_open()){std::cerr<<"Cannot open editor.txt\n";return 1;}
    Env env; std::string line;

    std::regex decl(R"(^\s*(integer|float)\s+([A-Za-z_]\w*)\s+te\s+(-?\d+(?:\.\d+)?)\s*$)");
    std::regex print_re(R"(^\s*dekhao\(\s*(.+)\s*\)\s*$)");

    while(std::getline(f,line)){
        if(line.empty())continue;
        std::smatch m;
        // variable declaration
        if(std::regex_match(line,m,decl)){
            std::string type=m[1], var=m[2]; double val=std::stod(m[3]);
            if(type=="integer"){env.types[var]=Type::INT;env.values[var]=round(val);}
            else{env.types[var]=Type::FLOAT;env.values[var]=val;}
            continue;
        }
        // print
        if(std::regex_match(line,m,print_re)){
            std::string args=m[1];
            // split by commas not inside quotes
            bool in_str=false; std::string token;
            for(size_t i=0;i<=args.size();++i){
                if(i==args.size()||(!in_str&&args[i]==',')){
                    std::string part=token; token.clear();
                    // trim
                    part=std::regex_replace(part,std::regex(R"(^\s+|\s+$)"),"");
                    if(!part.empty()){
                        if(part.size()>=2&&part.front()=='"'&&part.back()=='"'){
                            std::string text=part.substr(1,part.size()-2);
                            std::cout<<text;
                        }else{
                            try{
                                Parser p(part,&env);
                                double val=p.expr();
                                if(is_int_like(val))std::cout<<(long long)llround(val);
                                else std::cout<<std::setprecision(12)<<val;
                            }catch(const std::exception&e){
                                std::cerr<<"\nError: "<<e.what()<<"\n";
                            }
                        }
                    }
                    if(i<args.size())std::cout<<" ";
                }else{
                    if(args[i]=='"')in_str=!in_str;
                    token.push_back(args[i]);
                }
            }
            std::cout<<"\n";
            continue;
        }
        std::cerr<<"Syntax Error: "<<line<<"\n";
    }
}
