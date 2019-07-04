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


#ifndef wbengineplayer_hpp
#define wbengineplayer_hpp

#include <stdio.h>

#include "engine.h"

namespace banksia {
    
    class WbEngine : public Engine
    {
    public:
        WbEngine() : Engine() {}
        WbEngine(const Config& config) : Engine(config) {}
        
        virtual const char* className() const override { return "WbEngine"; }
        
        virtual std::string protocolString() const override;
        
        virtual void newGame() override;
        
        virtual void prepareToDeattach() override;

        virtual bool sendQuit() override;
        
        virtual bool sendPing() override;
        virtual bool sendPong(const std::string&);
        
        virtual bool goPonder(const Move& pondermove) override;
        virtual bool go() override;
        
        virtual void parseLine(const std::string&) override;
        
        virtual bool stop() override;
        virtual void tickWork() override;
        
    protected:
        virtual bool sendOptions();
        
    private:
        std::string timeControlString() const;
        
        int pingCnt = 0;
    };
    
} // namespace banksia

#endif /* WbEngineplayer_hpp */
