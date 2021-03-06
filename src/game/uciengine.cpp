/*
 This file is part of Banksia.
 
 Copyright (c) 2019 Nguyen Hong Pham
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
 */


#include <regex>
#include <map>

#include "uciengine.h"

using namespace banksia;

const std::unordered_map<std::string, int> UciEngine::uciEngineCmd {
    { "uciok",          static_cast<int>(UciEngine::UciEngineCmd::uciok) },
    { "readyok",          static_cast<int>(UciEngine::UciEngineCmd::readyok) },
    { "option",         static_cast<int>(UciEngine::UciEngineCmd::option) },
    { "info",           static_cast<int>(UciEngine::UciEngineCmd::info) },
    { "bestmove",       static_cast<int>(UciEngine::UciEngineCmd::bestmove) },
    { "id",             static_cast<int>(UciEngine::UciEngineCmd::theId) },
    { "copyprotection", static_cast<int>(UciEngine::UciEngineCmd::copyprotection) },
    { "registration",   static_cast<int>(UciEngine::UciEngineCmd::registration) }
};

const std::unordered_map<std::string, int>& UciEngine::getEngineCmdMap() const
{
    return uciEngineCmd;
}

std::string UciEngine::protocolString() const
{
    return "uci";
}

bool UciEngine::sendOptions()
{
    for(auto && option : config.optionList) {
        if (!isWritable()) {
            return false;
        }

        auto o = ConfigMng::instance->checkOverrideOption(option);
        if (o.isDefaultValue()) {
            continue;
        }
        
        std::string str = "setoption name " + o.name + " value " + o.getValueAsString();
        write(str);
    }
    return true;
}

void UciEngine::newGame()
{
    assert(getState() == PlayerState::ready);
    ponderingMove = MoveFull::illegalMove;
    expectingBestmove = false;
    computingState = EngineComputingState::idle;
    if (write("ucinewgame")) {
        setState(PlayerState::playing);
    }
}

void UciEngine::prepareToDeattach()
{
    if (tick_deattach >= 0) return;
    stop();
    tick_deattach = tick_period_deattach;
}

bool UciEngine::stop()
{
    if (expectingBestmove) {
        std::string str = "stop";
        return write(str);
    }
    return false;
}

bool UciEngine::goPonder(const Move& pondermove)
{
    assert(!expectingBestmove && computingState == EngineComputingState::idle);
    
    Engine::go(); // just for setting flags
    ponderingMove = MoveFull::illegalMove;
    
    if (config.ponderable && pondermove.isValid()) {
        ponderingMove = pondermove;
        
        expectingBestmove = true;
        computingState = EngineComputingState::pondering;

        auto goString = getGoString(pondermove);
        assert(goString.find("ponder") != std::string::npos);
        return write(goString);
    }
    return false;
}

bool UciEngine::go()
{
    Engine::go();
    ponderingMove = MoveFull::illegalMove;
    
    // Check for ponderhit
    if (computingState == EngineComputingState::pondering) {
        assert(expectingBestmove);
        if (!board->histList.empty() && board->histList.back().move == ponderingMove) {
            computingState = EngineComputingState::thinking;
            write("ponderhit");
            return true;
        }
        return stop();
    }
    
    assert(!expectingBestmove && computingState == EngineComputingState::idle);
    expectingBestmove = true;
    computingState = EngineComputingState::thinking;
    auto goString = getGoString(MoveFull::illegalMove);
    return write(goString);
}

std::string UciEngine::getPositionString(const Move& pondermove) const
{
    assert(board);
    
    std::string str = "position " + (board->fromOriginPosition() ? "startpos" : ("fen " + board->getStartingFen()));
    
    if (!board->histList.empty()) {
        str += " moves";
        for(auto && hist : board->histList) {
            str += " " + hist.move.toCoordinateString();
        }
    }
    
    if (pondermove.isValid()) {
        if (board->histList.empty()) {
            str += " moves";
        }
        str += " " + pondermove.toCoordinateString();
    }
    return str;
}

std::string UciEngine::getGoString(const Move& pondermove)
{
    std::string str = getPositionString(pondermove) + "\ngo ";
    if (pondermove.isValid()) {
        str += "ponder ";
    }
    
    str += timeControlString();
    return str;
}

std::string UciEngine::timeControlString() const
{
    switch (timeController->mode) {
        case TimeControlMode::infinite:
            return "infinite";
            
        case TimeControlMode::depth:
            return "depth " + std::to_string(timeController->depth);
            
        case TimeControlMode::movetime:
            return "movetime " + std::to_string(timeController->time);
            
        case TimeControlMode::standard:
        {
            // timeController unit: second, here it needs ms
            int wtime = int(timeController->getTimeLeft(W) * 1000);
            int btime = int(timeController->getTimeLeft(B) * 1000);
            
            int inc = int(timeController->increment * 1000);
            
            std::string str = "wtime " + std::to_string(wtime) + " btime " + std::to_string(btime)
            + " winc " + std::to_string(inc) + " binc " + std::to_string(inc);
            
            auto movestogo = 0;
            auto n = timeController->moves;
            if (n > 0) { // n == 0 => Fischer's clock
                auto halfCnt = int(board->histList.size());
                int fullCnt = halfCnt / 2;
                int curMoveCnt = fullCnt % n;
                movestogo = n - curMoveCnt;
            }
            if (movestogo > 0) {
                str += " movestogo " + std::to_string(movestogo);
            }
            return str;
        }
        default:
            break;
    }
    return "";
}

bool UciEngine::sendPing()
{
    return write("isready");
}

bool UciEngine::sendPong()
{
    return write("readyok");
}

void UciEngine::parseLine(int cmdInt, const std::string& cmdString, const std::string& line)
{
    if (cmdInt < 0) return;
    
    auto cmd = static_cast<UciEngineCmd>(cmdInt);
    switch (cmd) {
        case UciEngineCmd::option:
            if (!parseOption(line)) {
                write("Unknown option " + line);
            }
            break;
            
        case UciEngineCmd::info:
            if (computingState == EngineComputingState::thinking) {
                parseInfo(line);
            }
            break;
            
        case UciEngineCmd::bestmove:
        {
            auto timeCtrl = timeController;
            auto moveRecv = moveReceiver;
            if (timeCtrl == nullptr || moveRecv == nullptr) {
                return;
            }
            
            assert(expectingBestmove);
            assert(computingState != EngineComputingState::idle);
            
            expectingBestmove = false;
            auto oldComputingState = computingState;
            computingState = EngineComputingState::idle;
            
            auto period = timeCtrl->moveTimeConsumed(); // moveTimeConsumed();
            
            auto vec = splitString(line, ' ');
            if (vec.size() < 2) {
                return;
            }
            auto moveString = vec.at(1);
            
            std::string ponderMoveString = "";
            if (vec.size() >= 4 && vec.at(2) == "ponder") {
                ponderMoveString = vec.at(3);
            }
            
            if (!moveString.empty() && moveReceiver != nullptr) {
                auto move = board->moveFromCoordiateString(moveString);
                auto ponderMove = board->moveFromCoordiateString(ponderMoveString);
                (moveRecv)(move, moveString, ponderMove, period, oldComputingState);
            }
            
            break;
        }

        case UciEngineCmd::uciok:
        {
            setState(PlayerState::ready);
            expectingBestmove = false;
            sendOptions();
            sendPing();
            break;
        }

        case UciEngineCmd::theId:
        {
            auto vec = splitString(line, ' ');
            if (vec.size() <= 2) {
                return;
            }
            
            std::string str;
            for(size_t i = 2; i < vec.size(); i++) {
                if (!str.empty()) str += " ";
                str += vec.at(i);
            }
            
            if (vec.at(1) == "name") { // name or author
                config.idName = str;
            }
            break;
        }
            
        default:
            break;
    }
}

bool UciEngine::parseOption(const std::string& s)
{
    assert(!s.empty());
    
    static const std::regex re("option name (.*) type (combo|spin|button|check|string)(.*)");
    
    try {
        std::smatch match;
        if (std::regex_search(s, match, re) && match.size() > 1) {
            Option option;
            option.name = match.str(1);
            auto type = match.str(2);
            
            if (type == "button") {
                option.type = OptionType::button;
                config.updateOption(option);
                return true;
            }
            
            std::string rest = match.str(3);
            
            auto pDefault = rest.find("default");
            
            if (type == "string" || type == "check") {
                if (type == "check") {
                    option.type = OptionType::check;
                    bool b = rest.find("true") != std::string::npos;
                    option.setDefaultValue(b);
                } else {
                    option.type = OptionType::string;
                    std::string str;
                    if (pDefault != std::string::npos) {
                        str = pDefault + 8 >= rest.size() ? "" : rest.substr(pDefault + 8);
                        if (str == "<empty>") {
                            str = "";
                        }
                    }
                    option.setDefaultValue(str);
                }
                if (option.isValid()) {
                    config.updateOption(option);
                    return true;
                }
                return false;
            }
            
            if (rest.empty() || match.size() <= 3 || pDefault == std::string::npos) {
                return false;
            }
            
            if (type == "spin") {
                static const std::regex re("(.*)default (.+) min (.+) max (.+)");
                if (std::regex_search(rest, match, re) && match.size() > 4) {
                    auto dString = match.str(2);
                    auto minString = match.str(3);
                    auto maxString = match.str(4);
                    auto dInt = std::atoi(dString.c_str()), minInt = std::atoi(minString.c_str()), maxInt = std::atoi(maxString.c_str());
                    
                    option.type = OptionType::spin;
                    option.setDefaultValue(dInt, minInt, maxInt);
                    if (option.isValid()) {
                        config.updateOption(option);
                        return true;
                    }
                }
                return false;
            }
            
            if (type == "combo") {
                auto p = rest.find("default");
                std::string defaultString;
                std::vector<std::string> list;
                if (p != std::string::npos && rest.find("var") != std::string::npos) {
                    p += 8;
                    for(int i = 0; ; i++) {
                        auto q = rest.find("var", p);
                        auto len = (q != std::string::npos ? q : rest.length()) - p;
                        auto str = rest.substr(p, len);
                        trim(str);
                        if (str.empty()) break;
                        if (i == 0) {
                            defaultString = str;
                        } else {
                            list.push_back(str);
                        }
                        
                        if (q == std::string::npos) break;
                        p = q + 4;
                    }
                    
                    if (!list.empty()) {
                        option.type = OptionType::combo;
                        option.setDefaultValue(defaultString, list);
                        
                        if (option.isValid()) {
                            config.updateOption(option);
                            return true;
                        }
                        
                    }
                }
                return false;
            }
        } else {
        }
    } catch (std::regex_error& e) {
        // Syntax error in the regular expression
        std::cout << "Error: " << e.what() << std::endl;
    }
    
    return false;
}

bool UciEngine::parseInfo(const std::string& line)
{
    assert(!line.empty());

    std::string str;
    auto p = line.find(" pv ");
    if (p != std::string::npos) {
        str = line.substr(5, p);
    } else {
        str = line.substr(5);
    }

    auto vec = splitString(str, ' ');

    for(int i = 0; i < vec.size() - 1; i++) {
        auto name = vec.at(i);
        if (name == "depth") {
            ++i;
            auto s = vec.at(i);
            depth = std::atoi(s.c_str());
            continue;
        }
        
        if (name == "nodes") {
            ++i;
            auto s = vec.at(i);
            nodes = std::atoi(s.c_str());
            continue;
        }
        
        if (name == "score") {
            ++i;
            auto s = vec.at(i);
            
            auto iscpscore = false;
            if (s == "cp") {
                iscpscore = true;
                ++i;
                s = vec.at(i);
            }
            score = std::atoi(s.c_str());
            if (!iscpscore) score *= 100;
            continue;
        }

    }

    return false;
}
