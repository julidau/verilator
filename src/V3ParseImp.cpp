// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
// DESCRIPTION: Verilator: Netlist (top level) functions
//
// Code available from: https://verilator.org
//
//*************************************************************************
//
// Copyright 2003-2023 by Wilson Snyder. This program is free software; you
// can redistribute it and/or modify it under the terms of either the GNU
// Lesser General Public License Version 3 or the Perl Artistic License
// Version 2.0.
// SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0
//
//*************************************************************************
// Overview of files involved in parsing
//       V3Parse.h              External consumer interface to V3ParseImp
//       V3ParseImp             Internals to parser, common to across flex & bison
//         V3ParseGrammar       Wrapper that includes V3ParseBison
//           V3ParseBison       Bison output
//         V3ParseLex           Wrapper that includes lex output
//           V3Lexer.yy.cpp     Flex output
//*************************************************************************

#include "config_build.h"
#include "verilatedos.h"

#include "V3ParseImp.h"

#include "V3Ast.h"
#include "V3Error.h"
#include "V3File.h"
#include "V3Global.h"
#include "V3LanguageWords.h"
#include "V3Os.h"
#include "V3ParseBison.h"  // Generated by bison
#include "V3PreShell.h"

#include <sstream>

VL_DEFINE_DEBUG_FUNCTIONS;

//======================================================================
// Globals

V3ParseImp* V3ParseImp::s_parsep = nullptr;

int V3ParseSym::s_anonNum = 0;

//######################################################################
// Parser constructor

V3ParseImp::~V3ParseImp() {
    for (auto& itr : m_stringps) VL_DO_DANGLING(delete itr, itr);
    m_stringps.clear();
    for (auto& itr : m_numberps) VL_DO_DANGLING(delete itr, itr);
    m_numberps.clear();
    lexDestroy();
    parserClear();

    if (debug() >= 9) {
        UINFO(0, "~V3ParseImp\n");
        symp()->dumpSelf(cout, "-vpi: ");
    }
}

//######################################################################
// Parser utility methods

void V3ParseImp::lexPpline(const char* textp) {
    // Handle lexer `line directive
    FileLine* const prevFl = copyOrSameFileLine();
    int enterExit;
    lexFileline()->lineDirective(textp, enterExit /*ref*/);
    if (enterExit == 1) {  // Enter
        lexFileline()->parent(prevFl);
    } else if (enterExit == 2) {  // Exit
        FileLine* upFl = lexFileline()->parent();
        if (upFl) upFl = upFl->parent();
        if (upFl) lexFileline()->parent(upFl);
    }
}

void V3ParseImp::lexTimescaleParse(FileLine* fl, const char* textp) {
    // Parse `timescale of <number><units> / <number><units>
    VTimescale unit;
    VTimescale prec;
    VTimescale::parseSlashed(fl, textp, unit /*ref*/, prec /*ref*/);
    m_timeLastUnit = v3Global.opt.timeComputeUnit(unit);
    v3Global.rootp()->timeprecisionMerge(fl, prec);
}
void V3ParseImp::timescaleMod(FileLine* fl, AstNodeModule* modp, bool unitSet, double unitVal,
                              bool precSet, double precVal) {
    VTimescale unit{VTimescale::NONE};
    if (unitSet) {
        bool bad;
        unit = VTimescale{unitVal, bad /*ref*/};
        if (bad) {
            UINFO(1, "Value = " << unitVal << endl);
            fl->v3error("timeunit illegal value");
        }
    }
    VTimescale prec{VTimescale::NONE};
    if (precSet) {
        bool bad;
        prec = VTimescale{precVal, bad /*ref*/};
        if (bad) {
            UINFO(1, "Value = " << precVal << endl);
            fl->v3error("timeprecision illegal value");
        }
    }
    if (!unit.isNone()) {
        unit = v3Global.opt.timeComputeUnit(unit);
        if (modp) {
            modp->timeunit(unit);
        } else {
            v3Global.rootp()->timeunit(unit);
            unitPackage(fl)->timeunit(unit);
        }
    }
    v3Global.rootp()->timeprecisionMerge(fl, prec);
}

void V3ParseImp::lexVerilatorCmtLintSave(const FileLine* fl) { m_lexLintState.push_back(*fl); }

void V3ParseImp::lexVerilatorCmtLintRestore(FileLine* fl) {
    if (m_lexLintState.empty()) {
        fl->v3error("/*verilator lint_restore*/ without matching save");
        return;
    }
    fl->warnStateFrom(m_lexLintState.back());
    m_lexLintState.pop_back();
}

void V3ParseImp::lexVerilatorCmtLint(FileLine* fl, const char* textp, bool warnOff) {
    const char* sp = textp;
    while (*sp && !isspace(*sp)) ++sp;
    while (*sp && isspace(*sp)) ++sp;
    while (*sp && !isspace(*sp)) ++sp;
    while (*sp && isspace(*sp)) ++sp;
    string msg = sp;
    string::size_type pos;
    if ((pos = msg.find('*')) != string::npos) msg.erase(pos);
    // Use parsep()->lexFileline() as want to affect later FileLine's warnings
    if (!(parsep()->lexFileline()->warnOff(msg, warnOff))) {
        if (!v3Global.opt.isFuture(msg)) {
            fl->v3error("Unknown verilator lint message code: '" << msg << "', in '" << textp
                                                                 << "'");
        }
    }
}

void V3ParseImp::lexVerilatorCmtBad(FileLine* fl, const char* textp) {
    string cmtparse = textp;
    if (cmtparse.substr(0, std::strlen("/*verilator")) == "/*verilator") {
        cmtparse.replace(0, std::strlen("/*verilator"), "");
    }
    while (isspace(cmtparse[0])) cmtparse.replace(0, 1, "");
    string cmtname;
    for (int i = 0; isalnum(cmtparse[i]); i++) { cmtname += cmtparse[i]; }
    if (!v3Global.opt.isFuture(cmtname)) {
        fl->v3error("Unknown verilator comment: '" << textp << "'");
    }
}

void V3ParseImp::lexErrorPreprocDirective(FileLine* fl, const char* textp) {
    // Find all `preprocessor spelling candidates
    // Can't make this static as might get more defines later when read cells
    VSpellCheck speller;
    for (V3LanguageWords::const_iterator it = V3LanguageWords::begin();
         it != V3LanguageWords::end(); ++it) {
        const string ppDirective = it->first;
        if (ppDirective[0] == '`') speller.pushCandidate(ppDirective);
    }
    V3PreShell::candidateDefines(&speller);
    const string suggest = speller.bestCandidateMsg(textp);
    fl->v3error("Define or directive not defined: '"
                << textp << "'\n"
                << (suggest.empty() ? "" : fl->warnMore() + suggest));
}

string V3ParseImp::lexParseTag(const char* textp) {
    string tmp = textp + std::strlen("/*verilator tag ");
    string::size_type pos;
    if ((pos = tmp.rfind("*/")) != string::npos) tmp.erase(pos);
    return tmp;
}

double V3ParseImp::lexParseTimenum(const char* textp) {
    const size_t length = std::strlen(textp);
    char* const strgp = new char[length + 1];
    char* dp = strgp;
    const char* sp = textp;
    for (; isdigit(*sp) || *sp == '_' || *sp == '.'; ++sp) {
        if (*sp != '_') *dp++ = *sp;
    }
    *dp++ = '\0';
    const double d = strtod(strgp, nullptr);
    const string suffix{sp};

    double divisor = 1;
    if (suffix == "s") {
        divisor = 1;
    } else if (suffix == "ms") {
        divisor = 1e3;
    } else if (suffix == "us") {
        divisor = 1e6;
    } else if (suffix == "ns") {
        divisor = 1e9;
    } else if (suffix == "ps") {
        divisor = 1e12;
    } else if (suffix == "fs") {
        divisor = 1e15;
    } else {
        // verilog.l checks the suffix for us, so this is an assert
        v3fatalSrc("Unknown time suffix " << suffix);
    }

    VL_DO_DANGLING(delete[] strgp, strgp);
    return d / divisor;
}

//######################################################################
// Parser tokenization

size_t V3ParseImp::ppInputToLex(char* buf, size_t max_size) {
    size_t got = 0;
    while (got < max_size  // Haven't got enough
           && !m_ppBuffers.empty()) {  // And something buffered
        string front = m_ppBuffers.front();
        m_ppBuffers.pop_front();
        size_t len = front.length();
        if (len > (max_size - got)) {  // Front string too big
            const string remainder = front.substr(max_size - got);
            front = front.substr(0, max_size - got);
            m_ppBuffers.push_front(remainder);  // Put back remainder for next time
            len = (max_size - got);
        }
        std::memcpy(buf + got, front.c_str(), len);
        got += len;
    }
    if (debug() >= 9) {
        const string out = std::string{buf, got};
        cout << "   inputToLex  got=" << got << " '" << out << "'" << endl;
    }
    // Note returns 0 at EOF
    return got;
}

void V3ParseImp::preprocDumps(std::ostream& os) {
    if (v3Global.opt.dumpDefines()) {
        V3PreShell::dumpDefines(os);
    } else {
        const bool noblanks = v3Global.opt.preprocOnly() && v3Global.opt.preprocNoLine();
        for (auto& buf : m_ppBuffers) {
            if (noblanks) {
                bool blank = true;
                for (string::iterator its = buf.begin(); its != buf.end(); ++its) {
                    if (!isspace(*its) && *its != '\n') {
                        blank = false;
                        break;
                    }
                }
                if (blank) continue;
            }
            os << buf;
        }
    }
}

void V3ParseImp::parseFile(FileLine* fileline, const string& modfilename, bool inLibrary,
                           const string& errmsg) {  // "" for no error, make fake node
    const string nondirname = V3Os::filenameNonDir(modfilename);
    const string modname = V3Os::filenameNonExt(modfilename);

    UINFO(2, __FUNCTION__ << ": " << modname << (inLibrary ? " [LIB]" : "") << endl);
    m_lexFileline = new FileLine{fileline};
    m_lexFileline->newContent();
    m_bisonLastFileline = m_lexFileline;
    m_inLibrary = inLibrary;

    // Preprocess into m_ppBuffer
    const bool ok = V3PreShell::preproc(fileline, modfilename, m_filterp, this, errmsg);
    if (!ok) {
        if (errmsg != "") return;  // Threw error already
        // Create fake node for later error reporting
        AstNodeModule* const nodep = new AstNotFoundModule{fileline, modname};
        v3Global.rootp()->addModulesp(nodep);
        return;
    }

    if (v3Global.opt.preprocOnly() || v3Global.opt.keepTempFiles()) {
        // Create output file with all the preprocessor output we buffered up
        const string vppfilename = v3Global.opt.hierTopDataDir() + "/" + v3Global.opt.prefix()
                                   + "_" + nondirname + ".vpp";
        std::ofstream* ofp = nullptr;
        std::ostream* osp;
        if (v3Global.opt.preprocOnly()) {
            osp = &cout;
        } else {
            osp = ofp = V3File::new_ofstream(vppfilename);
        }
        if (osp->fail()) {
            fileline->v3error("Cannot write preprocessor output: " + vppfilename);
            return;
        } else {
            preprocDumps(*osp);
            if (ofp) {
                ofp->close();
                VL_DO_DANGLING(delete ofp, ofp);
            }
        }
    }

    // Parse it
    if (!v3Global.opt.preprocOnly()) {
        lexFile(modfilename);
    } else {
        m_ppBuffers.clear();
    }
}

void V3ParseImp::lexFile(const string& modname) {
    // Prepare for lexing
    UINFO(3, "Lexing " << modname << endl);
    s_parsep = this;
    lexFileline()->warnResetDefault();  // Reenable warnings on each file
    lexDestroy();  // Restart from clean slate.
    lexNew();

    // Lex it
    if (bisonParse()) v3fatal("Cannot continue\n");
}

void V3ParseImp::tokenPull() {
    // Pull token from lex into the pipeline
    // This corrupts yylval, must save/restore if required
    // Fetch next token from prefetch or real lexer
    yylexReadTok();  // sets yylval
    m_tokensAhead.push_back(yylval);
}

const V3ParseBisonYYSType* V3ParseImp::tokenPeekp(size_t depth) {
    // Look ahead "depth" number of tokens in the input stream
    // Returns pointer to token, which is no longer valid after changing m_tokensAhead
    while (m_tokensAhead.size() <= depth) tokenPull();
    return &m_tokensAhead.at(depth);
}

size_t V3ParseImp::tokenPipeScanParam(size_t depth) {
    // Search around IEEE parameter_value_assignment to see if :: follows
    // Return location of following token, or input if not found
    // yaID [ '#(' ... ')' ]
    if (tokenPeekp(depth)->token != '#') return depth;
    if (tokenPeekp(depth + 1)->token != '(') return depth;
    depth += 2;  // Past the (
    int parens = 1;  // Count first (
    while (true) {
        const int tok = tokenPeekp(depth)->token;
        if (tok == 0) {
            UINFO(9, "tokenPipeScanParam hit EOF; probably syntax error to come");
            break;
        } else if (tok == '(') {
            ++parens;
        } else if (tok == ')') {
            --parens;
            if (parens == 0) {
                ++depth;
                break;
            }
        }
        ++depth;
    }
    return depth;
}

void V3ParseImp::tokenPipeline() {
    // called from bison's "yylex", has a "this"
    if (m_tokensAhead.empty()) tokenPull();  // corrupts yylval
    yylval = m_tokensAhead.front();
    m_tokensAhead.pop_front();
    int token = yylval.token;
    // If a paren, read another
    if (token == '('  //
        || token == ':'  //
        || token == yCONST__LEX  //
        || token == yGLOBAL__LEX  //
        || token == yLOCAL__LEX  //
        || token == yNEW__LEX  //
        || token == ySTATIC__LEX  //
        || token == yVIRTUAL__LEX  //
        || token == yWITH__LEX  //
        || token == yaID__LEX  //
    ) {
        if (debugFlex() >= 6) {
            cout << "   tokenPipeline: reading ahead to find possible strength" << endl;
        }
        const V3ParseBisonYYSType curValue = yylval;  // Remember value, as about to read ahead
        const V3ParseBisonYYSType* nexttokp = tokenPeekp(0);
        const int nexttok = nexttokp->token;
        yylval = curValue;
        // Now potentially munge the current token
        if (token == '(' && isStrengthToken(nexttok)) {
            token = yP_PAR__STRENGTH;
        } else if (token == ':') {
            if (nexttok == yBEGIN) {
                token = yP_COLON__BEGIN;
            } else if (nexttok == yFORK) {
                token = yP_COLON__FORK;
            }
        } else if (token == yCONST__LEX) {
            if (nexttok == yREF) {
                token = yCONST__REF;
            } else {
                token = yCONST__ETC;
            }
        } else if (token == yGLOBAL__LEX) {
            if (nexttok == yCLOCKING) {
                token = yGLOBAL__CLOCKING;
            } else if (v3Global.opt.pedantic()) {
                token = yGLOBAL__ETC;
            }
            // Avoid 2009 "global" conflicting with old code when we can
            else {
                token = yaID__LEX;
                yylval.strp = V3ParseImp::parsep()->newString("global");
            }
        } else if (token == yLOCAL__LEX) {
            if (nexttok == yP_COLONCOLON) {
                token = yLOCAL__COLONCOLON;
            } else {
                token = yLOCAL__ETC;
            }
        } else if (token == yNEW__LEX) {
            if (nexttok == '(') {
                token = yNEW__PAREN;
            } else {
                token = yNEW__ETC;
            }
        } else if (token == ySTATIC__LEX) {
            if (nexttok == yCONSTRAINT) {
                token = ySTATIC__CONSTRAINT;
            } else {
                token = ySTATIC__ETC;
            }
        } else if (token == yVIRTUAL__LEX) {
            if (nexttok == yCLASS) {
                token = yVIRTUAL__CLASS;
            } else if (nexttok == yINTERFACE) {
                token = yVIRTUAL__INTERFACE;
            } else if (nexttok == yaID__ETC  //
                       || nexttok == yaID__LEX) {
                // || nexttok == yaID__aINTERFACE  // but we may not know interfaces yet.
                token = yVIRTUAL__anyID;
            } else {
                token = yVIRTUAL__ETC;
            }
        } else if (token == yWITH__LEX) {
            if (nexttok == '(') {
                token = yWITH__PAREN;
            } else if (nexttok == '[') {
                token = yWITH__BRA;
            } else if (nexttok == '{') {
                token = yWITH__CUR;
            } else {
                token = yWITH__ETC;
            }
        } else if (token == yaID__LEX) {
            if (nexttok == yP_COLONCOLON) {
                token = yaID__CC;
            } else if (nexttok == '#') {
                VL_RESTORER(yylval);  // Remember value, as about to read ahead
                const size_t depth = tokenPipeScanParam(0);
                if (tokenPeekp(depth)->token == yP_COLONCOLON) token = yaID__CC;
            }
        }
        // If add to above "else if", also add to "if (token" further above
    }
    yylval.token = token;
    // effectively returns yylval
}

bool V3ParseImp::isStrengthToken(int tok) {
    return tok == ygenSTRENGTH || tok == ySUPPLY0 || tok == ySUPPLY1 || tok == ySTRONG0
           || tok == ySTRONG1 || tok == yPULL0 || tok == yPULL1 || tok == yWEAK0 || tok == yWEAK1
           || tok == yHIGHZ0 || tok == yHIGHZ1;
}

void V3ParseImp::tokenPipelineSym() {
    // If an id, change the type based on symbol table
    // Note above sometimes converts yGLOBAL to a yaID__LEX
    tokenPipeline();  // sets yylval
    int token = yylval.token;
    if (token == yaID__LEX || token == yaID__CC) {
        const VSymEnt* foundp;
        if (const VSymEnt* const look_underp = V3ParseImp::parsep()->symp()->nextId()) {
            UINFO(7, "   tokenPipelineSym: next id lookup forced under " << look_underp << endl);
            // if (debug() >= 7) V3ParseImp::parsep()->symp()->dumpSelf(cout, " -symtree: ");
            foundp = look_underp->findIdFallback(*(yylval.strp));
            // "consume" it.  Must set again if want another token under temp scope
            V3ParseImp::parsep()->symp()->nextId(nullptr);
        } else {
            UINFO(7, "   tokenPipelineSym: find upward "
                         << V3ParseImp::parsep()->symp()->symCurrentp() << " for '"
                         << *(yylval.strp) << "'" << endl);
            // if (debug()>=9) V3ParseImp::parsep()->symp()->symCurrentp()->dumpSelf(cout,
            // " -findtree: ", true);
            foundp = V3ParseImp::parsep()->symp()->symCurrentp()->findIdFallback(*(yylval.strp));
        }
        if (!foundp && !m_afterColonColon) {  // Check if the symbol can be found in std
            AstPackage* const stdpkgp = v3Global.rootp()->stdPackagep();
            if (stdpkgp) {
                VSymEnt* const stdsymp = stdpkgp->user4u().toSymEnt();
                foundp = stdsymp->findIdFallback(*(yylval.strp));
            }
            if (foundp && !v3Global.usesStdPackage()) {
                AstPackageImport* const impp
                    = new AstPackageImport(stdpkgp->fileline(), stdpkgp, "*");
                unitPackage(stdpkgp->fileline())->addStmtsp(impp);
                v3Global.setUsesStdPackage();
            }
        }
        if (foundp) {
            AstNode* const scp = foundp->nodep();
            yylval.scp = scp;
            UINFO(7, "   tokenPipelineSym: Found " << scp << endl);
            if (token == yaID__LEX) {  // i.e. not yaID__CC
                if (VN_IS(scp, Typedef)) {
                    token = yaID__aTYPE;
                } else if (VN_IS(scp, TypedefFwd)) {
                    token = yaID__aTYPE;
                } else if (VN_IS(scp, Class)) {
                    token = yaID__aTYPE;
                } else if (VN_IS(scp, Package)) {
                    token = yaID__ETC;
                } else {
                    token = yaID__ETC;
                }
            } else if (!m_afterColonColon && *(yylval.strp) == "std") {
                v3Global.setUsesStdPackage();
            }
        } else {  // Not found
            yylval.scp = nullptr;
            if (token == yaID__CC) {
                if (!v3Global.opt.bboxUnsup()) {
                    // IEEE does require this, but we may relax this as UVM breaks it, so allow
                    // bbox for today
                    // We'll get a parser error eventually but might not be obvious
                    // is missing package, and this confuses people
                    static int warned = false;
                    if (!warned++) {
                        yylval.fl->v3warn(PKGNODECL, "Package/class '" + *yylval.strp
                                                         + "' not found, and needs to be "
                                                           "predeclared (IEEE 1800-2017 26.3)");
                    }
                }
            } else if (token == yaID__LEX) {
                token = yaID__ETC;
            }
        }
    }
    m_afterColonColon = token == yP_COLONCOLON;
    yylval.token = token;
    // effectively returns yylval
}

int V3ParseImp::tokenToBison() {
    // Called as global since bison doesn't have our pointer
    tokenPipelineSym();  // sets yylval
    m_bisonLastFileline = yylval.fl;

    // yylval.scp = nullptr;   // Symbol table not yet needed - no packages
    if (debugFlex() >= 6 || debugBison() >= 6) {  // --debugi-flex and --debugi-bison
        cout << "tokenToBison  " << yylval << endl;
    }
    return yylval.token;
}

//======================================================================
// V3ParseBisonYYSType functions

std::ostream& operator<<(std::ostream& os, const V3ParseBisonYYSType& rhs) {
    os << "TOKEN {" << rhs.fl->filenameLetters() << rhs.fl->asciiLineCol() << "}";
    os << "=" << rhs.token << " " << V3ParseImp::tokenName(rhs.token);
    if (rhs.token == yaID__ETC  //
        || rhs.token == yaID__CC  //
        || rhs.token == yaID__LEX  //
        || rhs.token == yaID__aTYPE) {
        os << " strp='" << *(rhs.strp) << "'";
    }
    return os;
}

//======================================================================
// V3Parse functions

V3Parse::V3Parse(AstNetlist* rootp, VInFilter* filterp, V3ParseSym* symp) {
    m_impp = new V3ParseImp{rootp, filterp, symp};
}
V3Parse::~V3Parse() {  //
    VL_DO_CLEAR(delete m_impp, m_impp = nullptr);
}
void V3Parse::parseFile(FileLine* fileline, const string& modname, bool inLibrary,
                        const string& errmsg) {
    m_impp->parseFile(fileline, modname, inLibrary, errmsg);
}
void V3Parse::ppPushText(V3ParseImp* impp, const string& text) {
    if (text != "") impp->ppPushText(text);
}
