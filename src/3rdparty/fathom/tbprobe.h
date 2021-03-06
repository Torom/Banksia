/*
 * tbprobe.h
 * (C) 2015 basil, all rights reserved,
 * Modified by Nguyen Pham
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef TBPROBE_H
#define TBPROBE_H

namespace Tablebase {
    
#ifndef TB_NO_STDINT
#include <stdint.h>
#else
    typedef unsigned char uint8_t;
    typedef unsigned short uint16_t;
    typedef unsigned uint32_t;
    typedef long long unsigned uint64_t;
    typedef char int8_t;
    typedef short int16_t;
    typedef int int32_t;
    typedef long long int64_t;
#endif
    
#define TB_MAX_MOVES                (192+1)
#define TB_MAX_PLY                  256
#define TB_CASTLING_K               0x1     /* White king-side. */
#define TB_CASTLING_Q               0x2     /* White queen-side. */
#define TB_CASTLING_k               0x4     /* Black king-side. */
#define TB_CASTLING_q               0x8     /* Black queen-side. */
    
#define TB_RESULT_WDL_MASK          0x0000000F
#define TB_RESULT_WDL_SHIFT         0
    
#define TB_GET_WDL(_res)                        \
(((_res) & TB_RESULT_WDL_MASK) >> TB_RESULT_WDL_SHIFT)
#define TB_RESULT_FAILED            0xFFFFFFFF
    
    class SyzygyTablebase
    {
    public:
        static int TB_LARGEST;
        
        static std::string toString();
        
        /*
         * Initialize the tablebase.
         *
         * PARAMETERS:
         * - path:
         *   The tablebase PATH string.
         *
         * RETURN:
         * - true=succes, false=failed.  The TB_LARGEST global will also be
         *   initialized.  If no tablebase files are found, then `true' is returned
         *   and TB_LARGEST is set to zero.
         */
        static bool tb_init(const std::string& path);
        
        /*
         * Free any resources allocated by tb_init
         */
        static void tb_free(void);
        
        /*
         * Probe the Distance-To-Zero (DTZ) table.
         *
         * PARAMETERS:
         * - white, black, kings, queens, rooks, bishops, knights, pawns:
         *   The current position (bitboards).
         * - rule50:
         *   The 50-move half-move clock.
         * - castling:
         *   Castling rights.  Set to zero if no castling is possible.
         * - ep:
         *   The en passant square (if exists).  Set to zero if there is no en passant
         *   square.
         * - turn:
         *   true=white, false=black
         * - results (OPTIONAL):
         *   Alternative results, one for each possible legal move.  The passed array
         *   must be TB_MAX_MOVES in size.
         *   If alternative results are not desired then set results=NULL.
         *
         * RETURN:
         * - A TB_RESULT value comprising:
         *   1) The WDL value (TB_GET_WDL)
         *   2) The suggested move (TB_GET_FROM, TB_GET_TO, TB_GET_PROMOTES, TB_GET_EP)
         *   3) The DTZ value (TB_GET_DTZ)
         *   The suggested move is guaranteed to preserved the WDL value.
         *
         *   Otherwise:
         *   1) TB_RESULT_STALEMATE is returned if the position is in stalemate.
         *   2) TB_RESULT_CHECKMATE is returned if the position is in checkmate.
         *   3) TB_RESULT_FAILED is returned if the probe failed.
         *
         *   If results!=NULL, then a TB_RESULT for each legal move will be generated
         *   and stored in the results array.  The results array will be terminated
         *   by TB_RESULT_FAILED.
         *
         * NOTES:
         * - Engines can use this function to probe at the root.  This function should
         *   not be used during search.
         * - DTZ tablebases can suggest unnatural moves, especially for losing
         *   positions.  Engines may prefer to traditional search combined with WDL
         *   move filtering using the alternative results array.
         * - This function is NOT thread safe.  For engines this function should only
         *   be called once at the root per search.
         */
        static inline unsigned tb_probe_root(
                                             uint64_t _white,
                                             uint64_t _black,
                                             uint64_t _kings,
                                             uint64_t _queens,
                                             uint64_t _rooks,
                                             uint64_t _bishops,
                                             uint64_t _knights,
                                             uint64_t _pawns,
                                             unsigned _rule50,
                                             unsigned _castling,
                                             unsigned _ep,
                                             bool     _turn,
                                             unsigned *_results)
        {
            if (_castling != 0)
                return TB_RESULT_FAILED;
            return tb_probe_root_impl(_white, _black, _kings, _queens, _rooks,
                                      _bishops, _knights, _pawns, _rule50, _ep, _turn, _results);
        }
        
    private:
        /*
         * Use the DTZ tables to rank and score all root moves.
         * INPUT: as for tb_probe_root
         * OUTPUT: TbRootMoves structure is filled in. This contains
         * an array of TbRootMove structures.
         * Each structure instance contains a rank, a score, and a
         * predicted principal variation.
         * RETURN VALUE:
         *   non-zero if ok, 0 means not all probes were successful
         *
         */
        int tb_probe_root_dtz(
                              uint64_t _white,
                              uint64_t _black,
                              uint64_t _kings,
                              uint64_t _queens,
                              uint64_t _rooks,
                              uint64_t _bishops,
                              uint64_t _knights,
                              uint64_t _pawns,
                              unsigned _rule50,
                              unsigned _castling,
                              unsigned _ep,
                              bool     _turn,
                              bool hasRepeated,
                              bool useRule50,
                              struct TbRootMoves *_results);
        
        /*
         // Use the WDL tables to rank and score all root moves.
         // This is a fallback for the case that some or all DTZ tables are missing.
         * INPUT: as for tb_probe_root
         * OUTPUT: TbRootMoves structure is filled in. This contains
         * an array of TbRootMove structures.
         * Each structure instance contains a rank, a score, and a
         * predicted principal variation.
         * RETURN VALUE:
         *   non-zero if ok, 0 means not all probes were successful
         *
         */
        int tb_probe_root_wdl(uint64_t _white,
                              uint64_t _black,
                              uint64_t _kings,
                              uint64_t _queens,
                              uint64_t _rooks,
                              uint64_t _bishops,
                              uint64_t _knights,
                              uint64_t _pawns,
                              unsigned _rule50,
                              unsigned _castling,
                              unsigned _ep,
                              bool     _turn,
                              bool useRule50,
                              struct TbRootMoves *_results);
        
        static void init_tb(const std::string& str);
        
        static unsigned tb_probe_wdl_impl(
                                          uint64_t _white,
                                          uint64_t _black,
                                          uint64_t _kings,
                                          uint64_t _queens,
                                          uint64_t _rooks,
                                          uint64_t _bishops,
                                          uint64_t _knights,
                                          uint64_t _pawns,
                                          unsigned _ep,
                                          bool     _turn);
        static unsigned tb_probe_root_impl(
                                           uint64_t _white,
                                           uint64_t _black,
                                           uint64_t _kings,
                                           uint64_t _queens,
                                           uint64_t _rooks,
                                           uint64_t _bishops,
                                           uint64_t _knights,
                                           uint64_t _pawns,
                                           unsigned _rule50,
                                           unsigned _ep,
                                           bool     _turn,
                                           unsigned *_results);
        
        /*
         * Probe the Win-Draw-Loss (WDL) table.
         *
         * PARAMETERS:
         * - white, black, kings, queens, rooks, bishops, knights, pawns:
         *   The current position (bitboards).
         * - rule50:
         *   The 50-move half-move clock.
         * - castling:
         *   Castling rights.  Set to zero if no castling is possible.
         * - ep:
         *   The en passant square (if exists).  Set to zero if there is no en passant
         *   square.
         * - turn:
         *   true=white, false=black
         *
         * RETURN:
         * - One of {TB_LOSS, TB_BLESSED_LOSS, TB_DRAW, TB_CURSED_WIN, TB_WIN}.
         *   Otherwise returns TB_RESULT_FAILED if the probe failed.
         *
         * NOTES:
         * - Engines should use this function during search.
         * - This function is thread safe assuming TB_NO_THREADS is disabled.
         */
        static inline unsigned tb_probe_wdl(
                                            uint64_t _white,
                                            uint64_t _black,
                                            uint64_t _kings,
                                            uint64_t _queens,
                                            uint64_t _rooks,
                                            uint64_t _bishops,
                                            uint64_t _knights,
                                            uint64_t _pawns,
                                            unsigned _rule50,
                                            unsigned _castling,
                                            unsigned _ep,
                                            bool     _turn)
        {
            if (_castling != 0)
                return TB_RESULT_FAILED;
            if (_rule50 != 0)
                return TB_RESULT_FAILED;
            return tb_probe_wdl_impl(_white, _black, _kings, _queens, _rooks,
                                     _bishops, _knights, _pawns, _ep, _turn);
        }
        
    };
    
    
    typedef uint16_t TbMove;
    
    struct TbRootMove {
        TbMove move;
        TbMove pv[TB_MAX_PLY];
        unsigned pvSize;
        int32_t tbScore, tbRank;
    };
    
    struct TbRootMoves {
        unsigned size;
        struct TbRootMove moves[TB_MAX_MOVES];
    };
    
} // namespace Tablebase

#endif



