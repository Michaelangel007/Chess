#include <stdio.h>  // printf
#include <stdint.h> // uint64_t
#include <string.h> // memcpy

// BEGIN OMP
    #include <omp.h>
// END OMP

#include "utility.h"
#include "bitboard.h"
#include "pieces.h"
#include "state.h"
#include "game.h"

// BEGIN OMP
int gnThreadsMaximum = 0 ;
int gnThreadsActive  = 0 ; // 0 = auto detect; > 0 manual # of threads

void InitMulticore()
{
// BEGIN OMP
    gnThreadsMaximum = omp_get_num_procs();

    if(!gnThreadsActive) // user didn't specify how many threads to use, default to all of them
        gnThreadsActive = gnThreadsMaximum;
    else
        omp_set_num_threads( gnThreadsActive );

    printf( "Using: %d / %d cores\n", gnThreadsActive, gnThreadsMaximum );

    for( int iCore = 0; iCore < gnThreadsActive; iCore++ )
    {
    }
}
// END OMP

int main( const int nArg, const char *aArg[] )
{
    (void) nArg;
    (void) aArg;

    InitMulticore();

#if 1
        printf( "= Queen - Print each potential move =\n" );

        uint8_t nRankFile = 0x24;
        bitboard_t board = BitBoardMovesColorQueen( nRankFile );

        uint8_t nMoves;
        uint8_t aMoves[ 32 ];
        BitBoardToRankFileAllMoves( board, nMoves, aMoves );
        printf( "Moves: %d\n", nMoves );
        for( int iMove = 0; iMove < nMoves; iMove++ )
        {
            printf( "[%2d]: 0x%02X\n", iMove, aMoves[ iMove ] );
        }
#endif

    // spin-lock
    // wait for command    

    return 0;
}

