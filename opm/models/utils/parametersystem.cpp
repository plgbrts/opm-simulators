// -*- mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// vi: set et ts=4 sw=4 sts=4:
/*
  This file is part of the Open Porous Media project (OPM).

  OPM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  OPM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPM.  If not, see <http://www.gnu.org/licenses/>.

  Consult the COPYING file in the top-level source directory of this
  module for the precise wording of the license and the list of
  copyright holders.
*/
/*!
 * \file
 *
 * \brief This file provides the infrastructure to retrieve run-time parameters
 *
 * Internally, runtime parameters are implemented using
 * Dune::ParameterTree with the default value taken from the parameter
 * definition.
 */
#include <config.h>
#include <opm/models/utils/parametersystem.hpp>

namespace {

std::string parseQuotedValue(std::string& s, const std::string& errorPrefix)
{
    if (s.empty() || s[0] != '"')
        throw std::runtime_error(errorPrefix+"Expected quoted string");

    std::string result;
    unsigned i = 1;
    for (; i < s.size(); ++i) {
        // handle escape characters
        if (s[i] == '\\') {
            ++ i;
            if (s.size() <= i)
                throw std::runtime_error(errorPrefix+"Unexpected end of quoted string");

            if (s[i] == 'n')
                result += '\n';
            else if (s[i] == 'r')
                result += '\r';
            else if (s[i] == 't')
                result += '\t';
            else if (s[i] == '"')
                result += '"';
            else if (s[i] == '\\')
                result += '\\';
            else
                throw std::runtime_error(errorPrefix+"Unknown escape character '\\" + s[i] + "'");
        }
        else if (s[i] == '"')
            break;
        else
            result += s[i];
    }

    s = s.substr(i+1);
    return result;
}

std::string parseUnquotedValue(std::string& s, const std::string&)
{
    unsigned i;
    for (i = 0; i < s.size(); ++ i)
        if (std::isspace(s[i]))
            break;

    std::string ret = s.substr(0, i);
    s = s.substr(i);
    return ret;
}

void removeLeadingSpace(std::string& s)
{
    unsigned i;
    for (i = 0; i < s.size(); ++ i)
        if (!std::isspace(s[i]))
            break;
    s = s.substr(i);
}

} // anonymous namespace

namespace Opm::Parameters {

bool ParamInfo::operator==(const ParamInfo& other) const
{
    return other.paramName == paramName
        && other.paramTypeName == paramTypeName
        && other.typeTagName == typeTagName
        && other.usageString == usageString;
}

std::string breakLines(const std::string& msg,
                       int indentWidth,
                       int maxWidth)
{
    std::string result;
    int startInPos = 0;
    int inPos = 0;
    int lastBreakPos = 0;
    int ttyPos = 0;
    for (; inPos < int(msg.size()); ++ inPos, ++ ttyPos) {
        if (msg[inPos] == '\n') {
            result += msg.substr(startInPos, inPos - startInPos + 1);
            startInPos = inPos + 1;
            lastBreakPos = startInPos + 1;

            // we need to use -1 here because ttyPos is incremented after the loop body
            ttyPos = -1;
            continue;
        }

        if (std::isspace(msg[inPos]))
            lastBreakPos = inPos;

        if (ttyPos >= maxWidth) {
            if (lastBreakPos > startInPos) {
                result += msg.substr(startInPos, lastBreakPos - startInPos);
                startInPos = lastBreakPos + 1;
                lastBreakPos = startInPos;
                inPos = startInPos;
            }
            else {
                result += msg.substr(startInPos, inPos - startInPos);
                startInPos = inPos;
                lastBreakPos = startInPos;
                inPos = startInPos;
            }

            result += "\n";
            for (int i = 0; i < indentWidth; ++i)
                result += " ";
            ttyPos = indentWidth;
        }
    }

    result += msg.substr(startInPos);

    return result;
}

void reset()
{
    MetaData::clear();
}

void endRegistration()
{
    if (!MetaData::registrationOpen()) {
        throw std::logic_error("Parameter registration was already closed. It is only possible "
                               "to close it once.");
    }

    MetaData::registrationOpen() = false;

    // loop over all parameters and retrieve their values to make sure
    // that there is no syntax error
    for (const auto& param : MetaData::registrationFinalizers()) {
        param->retrieve();
    }
    MetaData::registrationFinalizers().clear();
}

void printParamUsage(std::ostream& os, const ParamInfo& paramInfo)
{
    std::string paramMessage, paramType, paramDescription;

    int ttyWidth = getTtyWidth();

    // convert the CamelCase name to a command line --parameter-name.
    std::string cmdLineName = "-";
    const std::string camelCaseName = paramInfo.paramName;
    for (unsigned i = 0; i < camelCaseName.size(); ++i) {
        if (isupper(camelCaseName[i]))
            cmdLineName += "-";
        cmdLineName += static_cast<char>(std::tolower(camelCaseName[i]));
    }

    // assemble the printed output
    paramMessage = "    ";
    paramMessage += cmdLineName;

    // add the =VALUE_TYPE part
    bool isString = false;
    if (paramInfo.paramTypeName == Dune::className<std::string>()
        || paramInfo.paramTypeName == "const char *")
    {
        paramMessage += "=STRING";
        isString = true;
    }
    else if (paramInfo.paramTypeName == Dune::className<float>()
             || paramInfo.paramTypeName == Dune::className<double>()
             || paramInfo.paramTypeName == Dune::className<long double>()
#if HAVE_QUAD
             || paramInfo.paramTypeName == Dune::className<quad>()
#endif // HAVE_QUAD
        )
        paramMessage += "=SCALAR";
    else if (paramInfo.paramTypeName == Dune::className<int>()
             || paramInfo.paramTypeName == Dune::className<unsigned int>()
             || paramInfo.paramTypeName == Dune::className<short>()
             || paramInfo.paramTypeName == Dune::className<unsigned short>())
        paramMessage += "=INTEGER";
    else if (paramInfo.paramTypeName == Dune::className<bool>())
        paramMessage += "=BOOLEAN";
    else if (paramInfo.paramTypeName.empty()) {
        // the parameter is a flag. Do nothing!
    }
    else {
        // unknown type
        paramMessage += "=VALUE";
    }

    // fill up the up help string to the 50th character
    paramMessage += "  ";
    while (paramMessage.size() < 50)
        paramMessage += " ";


    // append the parameter usage string.
    paramMessage += paramInfo.usageString;

    // add the default value
    if (!paramInfo.paramTypeName.empty()) {
        if (paramMessage.back() != '.')
            paramMessage += '.';
        paramMessage += " Default: ";
        if (paramInfo.paramTypeName == "bool") {
            if (paramInfo.defaultValue == "0")
                paramMessage += "false";
            else
                paramMessage += "true";
        }
        else if (isString) {
            paramMessage += "\"";
            paramMessage += paramInfo.defaultValue;
            paramMessage += "\"";
        }
        else
            paramMessage += paramInfo.defaultValue;
    }

    paramMessage = breakLines(paramMessage, /*indent=*/52, ttyWidth);
    paramMessage += "\n";

    // print everything
    os << paramMessage;
}

void getFlattenedKeyList(std::list<std::string>& dest,
                         const Dune::ParameterTree& tree,
                         const std::string& prefix)
{
    // add the keys of the current sub-structure
    for (const auto& valueKey : tree.getValueKeys()) {
        std::string newKey(prefix + valueKey);
        dest.push_back(newKey);
    }

    // recursively add all substructure keys
    for (const auto& subKey : tree.getSubKeys()) {
        std::string newPrefix(prefix + subKey + '.');
        getFlattenedKeyList(dest, tree.sub(subKey), newPrefix);
    }
}

// print the values of a list of parameters
void printParamList(std::ostream& os,
                    const std::list<std::string>& keyList,
                    bool printDefaults)
{
    const Dune::ParameterTree& tree = MetaData::tree();

    for (const auto& key : keyList) {
        const auto& paramInfo = MetaData::registry().at(key);
        const std::string& defaultValue = paramInfo.defaultValue;
        std::string value = defaultValue;
        if (tree.hasKey(key))
            value = tree.get(key, "");
        os << key << "=\"" << value << "\"";
        if (printDefaults)
            os << " # default: \"" << defaultValue << "\"";
        os << "\n";
    }
}

void printUsage(const std::string& helpPreamble,
                const std::string& errorMsg,
                std::ostream& os,
                const bool showAll)
{
    if (!errorMsg.empty()) {
        os << errorMsg << "\n\n";
    }

    os << breakLines(helpPreamble, /*indent=*/2, /*maxWidth=*/getTtyWidth());
    os << "\n";

    os << "Recognized options:\n";

    if (!helpPreamble.empty()) {
        ParamInfo pInfo;
        pInfo.paramName = "h,--help";
        pInfo.usageString = "Print this help message and exit";
        printParamUsage(os, pInfo);
        pInfo.paramName = "-help-all";
        pInfo.usageString = "Print all parameters, including obsolete, hidden and deprecated ones.";
        printParamUsage(os, pInfo);
    }

    for (const auto& param : MetaData::registry()) {
        if (showAll || !param.second.isHidden)
            printParamUsage(os, param.second);
    }
}

int noPositionalParameters_(std::function<void(const std::string&, const std::string&)>,
                            std::set<std::string>&,
                            std::string& errorMsg,
                            int,
                            const char** argv,
                            int paramIdx,
                            int)
{
    errorMsg = std::string("Illegal parameter \"")+argv[paramIdx]+"\".";
    return 0;
}

void parseParameterFile(const std::string& fileName, bool overwrite)
{
    std::set<std::string> seenKeys;
    std::ifstream ifs(fileName);
    unsigned curLineNum = 0;
    while (ifs) {
        // string and file processing in c++ is quite blunt!
        std::string curLine;
        std::getline(ifs, curLine);
        curLineNum += 1;
        std::string errorPrefix = fileName+":"+std::to_string(curLineNum)+": ";

        // strip leading white space
        removeLeadingSpace(curLine);

        // ignore empty and comment lines
        if (curLine.empty() || curLine[0] == '#' || curLine[0] == ';')
            continue;

        // TODO (?): support for parameter groups.

        // find the "key" of the key=value pair
        std::string key = parseKey(curLine);
        std::string canonicalKey = transformKey(key, /*capitalizeFirst=*/true, errorPrefix);

        if (seenKeys.count(canonicalKey) > 0)
            throw std::runtime_error(errorPrefix+"Parameter '"+canonicalKey+"' seen multiple times in the same file");
        seenKeys.insert(canonicalKey);

        // deal with the equals sign
        removeLeadingSpace(curLine);
        if (curLine.empty() || curLine[0] != '=')
            std::runtime_error(errorPrefix+"Syntax error, expecting 'key=value'");

        curLine = curLine.substr(1);
        removeLeadingSpace(curLine);

        if (curLine.empty() || curLine[0] == '#' || curLine[0] == ';')
            std::runtime_error(errorPrefix+"Syntax error, expecting 'key=value'");

        // get the value
        std::string value;
        if (curLine[0] == '"')
            value = parseQuotedValue(curLine, errorPrefix);
        else
            value = parseUnquotedValue(curLine, errorPrefix);

        // ignore trailing comments
        removeLeadingSpace(curLine);
        if (!curLine.empty() && curLine[0] != '#' && curLine[0] != ';')
            std::runtime_error(errorPrefix+"Syntax error, expecting 'key=value'");

        // all went well, add the parameter to the database object
        if (overwrite || !MetaData::tree().hasKey(canonicalKey)) {
            MetaData::tree()[canonicalKey] = value;
        }
    }
}

void printValues(std::ostream& os)
{
    std::list<std::string> runTimeAllKeyList;
    std::list<std::string> runTimeKeyList;
    std::list<std::string> unknownKeyList;

    getFlattenedKeyList(runTimeAllKeyList, MetaData::tree());
    for (const auto& key : runTimeAllKeyList) {
        if (MetaData::registry().find(key) == MetaData::registry().end()) {
            // key was not registered by the program!
            unknownKeyList.push_back(key);
        }
        else {
            // the key was specified at run-time
            runTimeKeyList.push_back(key);
        }
    }

    // loop over all registered parameters
    std::list<std::string> compileTimeKeyList;
    for (const auto& reg : MetaData::registry()) {
        // check whether the key was specified at run-time
        if (MetaData::tree().hasKey(reg.first)) {
            continue;
        } else  {
            compileTimeKeyList.push_back(reg.first);
        }
    }

    // report the values of all registered (and unregistered)
    // parameters
    if (runTimeKeyList.size() > 0) {
        os << "# [known parameters which were specified at run-time]\n";
        printParamList(os, runTimeKeyList, /*printDefaults=*/true);
    }

    if (compileTimeKeyList.size() > 0) {
        os << "# [parameters which were specified at compile-time]\n";
        printParamList(os, compileTimeKeyList, /*printDefaults=*/false);
    }

    if (unknownKeyList.size() > 0) {
        os << "# [unused run-time specified parameters]\n";
        for (const auto& unused : unknownKeyList) {
            os << unused << "=\"" << MetaData::tree().get(unused, "") << "\"\n" << std::flush;
        }
    }
}

bool printUnused(std::ostream& os)
{
    std::list<std::string> runTimeAllKeyList;
    std::list<std::string> unknownKeyList;

    getFlattenedKeyList(runTimeAllKeyList, MetaData::tree());
    for (const auto& key : runTimeAllKeyList) {
        if (MetaData::registry().find(key) == MetaData::registry().end()) {
            // key was not registered by the program!
            unknownKeyList.push_back(key);
        }
    }

    if (unknownKeyList.size() > 0) {
        os << "# [unused run-time specified parameters]\n";
        for (const auto& unused : unknownKeyList) {
            os << unused << "=\""
               << MetaData::tree().get(unused, "") << "\"\n" << std::flush;
        }
        return true;
    }
    return false;
}

int getTtyWidth()
{
    int ttyWidth = 10*1000; // effectively do not break lines at all.
    if (isatty(STDOUT_FILENO)) {
#if defined TIOCGWINSZ
        // This is a bit too linux specific, IMO. let's do it anyway
        struct winsize ttySize;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &ttySize);
        ttyWidth = std::max<int>(80, ttySize.ws_col);
#else
        // default for systems that do not implement the TIOCGWINSZ ioctl
        ttyWidth = 100;
#endif
    }

    return ttyWidth;
}

std::string parseKey(std::string& s)
{
    unsigned i;
    for (i = 0; i < s.size(); ++ i)
        if (std::isspace(s[i]) || s[i] == '=')
            break;

    std::string ret = s.substr(0, i);
    s = s.substr(i);
    return ret;
}

std::string transformKey(const std::string& s,
                         bool capitalizeFirstLetter,
                         const std::string& errorPrefix)
{
    std::string result;

    if (s.empty())
        throw std::runtime_error(errorPrefix+"Empty parameter names are invalid");

    if (!std::isalpha(s[0]))
        throw std::runtime_error(errorPrefix+"Parameter name '" + s + "' is invalid: First character must be a letter");

    if (capitalizeFirstLetter)
        result += static_cast<char>(std::toupper(s[0]));
    else
        result += s[0];

    for (unsigned i = 1; i < s.size(); ++i) {
        if (s[i] == '-') {
            ++ i;
            if (s.size() <= i || !std::isalpha(s[i]))
                throw std::runtime_error(errorPrefix+"Invalid parameter name '" + s + "'");
            result += static_cast<char>(std::toupper(s[i]));
        }
        else if (!std::isalnum(s[i]))
            throw std::runtime_error(errorPrefix+"Invalid parameter name '" + s + "'");
        else
            result += s[i];
    }

    return result;
}

} // namespace Opm::Parameters
