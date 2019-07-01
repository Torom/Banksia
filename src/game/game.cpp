/*
 This file is part of Banksia, distributed under MIT license.
 
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


#include <iostream>
#include <iomanip>
#include <ctime>

#include "game.h"
#include "engine.h"

using namespace banksia;


Game::Game()
{
    state = GameState::begin;
    players[0] = players[1] = nullptr;
}

Game::Game(Player* player0, Player* player1, const TimeController& timeController, bool ponderMode)
{
    state = GameState::begin;
    players[0] = players[1] = nullptr;
    set(player0, player1, timeController, ponderMode);
}

Game::~Game()
{
}

bool Game::isValid() const
{
    return players[0] && players[0]->isValid() && players[1] && players[1]->isValid();
}

std::string Game::toString() const
{
    return "";
}

void Game::setStartup(int _idx, const std::string& _startFen, const std::vector<Move>& _startMoves)
{
    idx = _idx;
    startFen = _startFen;
    startMoves = _startMoves;
}

int Game::getIdx() const
{
    return idx;
}

void Game::set(Player* player0, Player* player1, const TimeController& _timeController, bool _ponderMode)
{
    attach(player0, Side::white);
    attach(player1, Side::black);
    timeController.cloneFrom(_timeController);
    ponderMode = _ponderMode;
}

void Game::attach(Player* player, Side side)
{
    if (player == nullptr || (side != Side::white && side != Side::black)) return;
    auto sd = static_cast<int>(side);
    
    players[sd] = player;
    
    player->setup(&board, &timeController);
    
    player->addMoveReceiver(this, [=](const std::string& moveString, const std::string& ponderMoveString, double timeConsumed, EngineComputingState state) {
        moveFromPlayer(moveString, ponderMoveString, timeConsumed, side, state);
    });
}

Player* Game::deattachPlayer(Side side)
{
    auto sd = static_cast<int>(side);
    auto player = players[sd];
    players[sd] = nullptr;
    if (player) {
        player->setup(nullptr, nullptr);
    }
    return player;
}

void Game::newGame()
{
    // Include opening
    board.newGame(startFen);
    for(auto && m : startMoves) {
        if (m.isValid()) {
            auto fullMove = board.createMove(m.from, m.dest, m.promotion);
            board.make(fullMove);
        }
    }
    
    for(int i = 0; i < 2; i++) {
        if (players[i]) {
            players[i]->newGame();
        }
    }
    
    setState(GameState::begin);
}

void Game::startPlaying()
{
    assert(state == GameState::ready);
    
    newGame();
    
    setState(GameState::playing);
    startThinking();
}

void Game::startThinking(Move pondermove)
{
    assert(board.isValid());
    
    timeController.setupClocksBeforeThinking(int(board.histList.size()));
    
    auto sd = static_cast<int>(board.side);
    
    players[1 - sd]->goPonder(pondermove);
    players[sd]->go();
}

void Game::start()
{
    for(int sd = 0; sd < 2; sd++) {
        players[sd]->kickStart();
    }
}

void Game::pause()
{
}

void Game::stop()
{
}

void Game::moveFromPlayer(const std::string& moveString, const std::string& ponderMoveString, double timeConsumed, Side side, EngineComputingState oldState)
{
    // avoid conflicting between this function and one called by tickWork
    std::lock_guard<std::mutex> dolock(criticalMutex);
    
    if (state != GameState::playing || checkTimeOver() || board.side != side) {
        return;
    }

    auto move = ChessBoard::moveFromCoordiateString(moveString);
    Move pondermove = ponderMode ? ChessBoard::moveFromCoordiateString(ponderMoveString) : Move::illegalMove;

    
    assert(board.side == side);
    if (oldState == EngineComputingState::thinking) {
        if (make(move)) {
            assert(board.side != side);
            
            auto lastHist = board.histList.back();
            lastHist.elapsed = timeConsumed;
            timeController.udateClockAfterMove(timeConsumed, lastHist.move.piece.side, int(board.histList.size()));
            
            startThinking(pondermove);
        }
    } else if (oldState == EngineComputingState::pondering) { // missed ponderhit, stop called
        auto sd = static_cast<int>(board.side);
        players[sd]->go();
    }
}

bool Game::make(const Move& move)
{
    if (board.checkMake(move.from, move.dest, move.promotion)) {
        auto result = board.rule();
        if (result.result != ResultType::noresult) {
            gameOver(result);
            return false;
        }
        
        assert(board.isValid());
        return true;
    } else {
        Result result(board.side == Side::white ? ResultType::loss : ResultType::win, ReasonType::illegalmove);
        gameOver(result);
        return false;
    }
    return false;
}

void Game::gameOver(const Result& result)
{
    for(int sd = 0; sd < 2; sd++) {
        if (players[sd]) {
            players[sd]->stopThinking();
        }
    }
    
    board.result = result;
    setState(GameState::stopped);
}

Player* Game::getPlayer(Side side)
{
    auto sd = static_cast<int>(side);
    return players[sd];
}

bool Game::checkTimeOver()
{
    if (timeController.isTimeOver(board.side)) {
        Result result;
        result.result = board.side == Side::white ? ResultType::loss : ResultType::win;
        result.reason = ReasonType::timeout;
        gameOver(result);
        return true;
    }
    return false;
}

void Game::tickWork()
{
    switch (state) {
        case GameState::begin:
        {
            auto readyCnt = 0, stoppedCnt = 0;
            for(int sd = 0; sd < 2; sd++) {
                if (!players[sd]) {
                    continue;
                }
                auto st = players[sd]->getState();
                if (st == PlayerState::ready) {
                    readyCnt++;
                } else if (st == PlayerState::stopped) {
                    stoppedCnt++;
                }
            }
            if (readyCnt + stoppedCnt < 2) {
                break;
            }
            
            if (readyCnt == 2) {
                setState(GameState::ready);
            } else {
                Result result;
                result.reason = ReasonType::crash;
                setState(GameState::stopped);
                if (stoppedCnt == 2) {
                    result.result = ResultType::draw;
                } else {
                    result.result =  players[W]->getState() == PlayerState::stopped ? ResultType::loss : ResultType::win;
                }
                
                gameOver(result);
            }
            break;
        }
            
        case GameState::ready:
            startPlaying();
            break;
            
        case GameState::playing:
        {
            auto sd = static_cast<int>(board.side);
            if (!players[sd] || players[sd]->isHuman()) {
                break;
            }
            auto engine = (Engine*)players[sd];
            if (engine->computingState != EngineComputingState::thinking) {
                break;
            }
            
            // avoid conflict with moveFromPlayer
            std::lock_guard<std::mutex> dolock(criticalMutex);
            if (engine->computingState == EngineComputingState::thinking && state == GameState::playing) {
                checkTimeOver();
            }
            break;
        }
        default:
            break;
    }
}

// https://stackoverflow.com/questions/38034033/c-localtime-this-function-or-variable-may-be-unsafe
inline std::tm localtime_xp(std::time_t timer)
{
	std::tm bt{};
#if defined(__unix__)
	localtime_r(&timer, &bt);
#elif defined(_MSC_VER)
	localtime_s(&bt, &timer);
#else
	static std::mutex mtx;
	std::lock_guard<std::mutex> lock(mtx);
	bt = *std::localtime(&timer);
#endif
	return bt;
}


std::string Game::toPgn(std::string event, std::string site, int round) const
{
    std::ostringstream stringStream;
    
    if (!event.empty()) {
        stringStream << "[Event \t\"" << event << "\"]" << std::endl;
    }
    if (!site.empty()) {
        stringStream << "[Site \t\"" << site << "\"]" << std::endl;
    }
    
	auto tm = localtime_xp(std::time(0));
    
    stringStream << "[Date \t\"" << std::put_time(&tm, "%Y.%m.%d") << "\"]" << std::endl;
    
    if (round > 0) {
        stringStream << "[Round \t\"" << round << "\"]" << std::endl;
    }
    
    for(int sd = 1; sd >= 0; sd--) {
        if (players[sd]) {
            stringStream << "[" << (sd == W ? "White" : "Black") << " \t\"" << players[sd]->name << "\"]" << std::endl;
        }
    }
    stringStream << "[Result \t\"" << board.result.toShortString() << "\"]" << std::endl;
    
    stringStream << "[TimeControl \t\"" << timeController.toString() << "\"]" << std::endl;
    stringStream << "[Time \t\"" << std::put_time(&tm, "%H:%M:%S") << "\"]" << std::endl;
    
    auto str = board.result.reasonString();
    if (!str.empty()) {
        stringStream << "[Termination \t\"" << str << "\"]" << std::endl;
    }
    
    if (!board.fromOriginPosition()) {
        stringStream << "[FEN \t\"" << board.getStartingFen() << "\"]" << std::endl;
    }
    
    // Move text
    auto c = 0;
    for(int i = 0; i < board.histList.size(); i++) {
        if (c) stringStream << " ";
        if ((i & 1) == 0) {
            stringStream << (1 + i / 2) << ". ";
        }
        
        stringStream << board.histList.at(i).moveString;
        
        c++;
        if (c >= 8) {
            c = 0;
            stringStream << std::endl;
        }
    }
    
    if (board.result.result != ResultType::noresult) {
        if (c) stringStream << " ";
        stringStream << board.result.toShortString() << std::endl;
    }
    stringStream << std::endl;
    
    return stringStream.str();
}
