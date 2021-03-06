#include <stdio.h>  // printf
#include <stdint.h> // uint64_t
#include <string.h> // memcpy
#include <stdlib.h> // atoi() // overkill

// BEGIN OMP
    #include <omp.h>
// END OMP

#include "utility.h"
#include "bitboard.h"
#include "pieces.h"
#include "rawboard.h"
#include "state_bitboard.h"
#include "state_normal.h"
#include "state_search.h"
#include "game.h"
#include "eval.h"

    bool gbAnsiOutput = false;

// BEGIN OMP
    int gnThreadsMaximum = 0 ;
    int gnThreadsActive  = 0 ; // 0 = auto detect; > 0 manual # of threads

void StartupCommandLine( const int nArg, const char *aArg[] )
{
#ifdef WIN32
    gbAnsiOutput = false;
#else
    gbAnsiOutput = true;
#endif

    for( int iArg = 1; iArg < nArg; iArg++ )
    {
        if (strcmp( aArg[ iArg ], "-ansi" ) == 0)
            gbAnsiOutput = true;
    }
}

void StartupMulticore()
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
        nMovePool[ iCore ] = 0;
        aMovePool[ iCore ] = new SearchState_t[ MAX_POOL_MOVES ];
omp_init_lock(
       &aLockPool[ iCore ]
);
    }

    size_t nMemState = sizeof( State_t[ MAX_POOL_MOVES ] );
    printf( "Mem/Core: %u bytes (%u KB)\n", (uint32_t) nMemState, (uint32_t) nMemState/1024 );
}

void ShutdownMulticore()
{
    for( int iCore = 0; iCore < gnThreadsActive; iCore++ )
    {
        delete []  aMovePool[ iCore ];
omp_destroy_lock( &aLockPool[ iCore ] );
   }
}
// END OMP

int INVALID_LOCATION = -1;

int GetInputLocation( char *pText, size_t nLen )
{
    char *p   = pText;
    int   nRF = INVALID_LOCATION;

    if (nLen < 2)
        return nRF;

    bool bUpper  = (p[0] >= 'A') && (p[0] <= 'H');
    bool bLower  = (p[0] >= 'a') && (p[0] <= 'h');
    bool bNumber = (p[1] >= '0') && (p[1] <= '7');

    if ((nLen > 1) && (bUpper || bLower) && bNumber)
    {
        nRF = (((p[1] - 1) & 7) << 4) + ((p[0] - 1) & 7);
printf( "RankFile: 0x%02X\n", nRF );
    }

    return nRF;
}


/** Reads input string into arguments
    @Note: Replaces arg seperator (space) with NULL
*/
void GetInputArguments( char *sInput, int nMaxCmds,
    int& nCmds_, char *aCmds_[], int *aLens_ )
{
    char *start = sInput;
    int   iCmds = 0;

    aCmds_[ iCmds ] = sInput;
    aLens_[ iCmds ] = 0;

    for( char *end = sInput; *end; end++ )
    {
        if (*end == ' ' || *end == 0xA || *end == 0xD)
        {
            if ((*end == 0xA) || (*end == 0xD))
                *end = 0;

            aLens_[ iCmds ] = end - start;
            iCmds++;

            if (iCmds < nMaxCmds)
                aCmds_[ iCmds ] = start = end + 1;
            else
                break;

        }
    }

    nCmds_ = iCmds;
}

int main( const int nArg, const char *aArg[] )
{
    StartupCommandLine( nArg, aArg );
    StartupMulticore();

    ChessGame_t game;
    game.Reset();

    bool    bQuit = false;
    int     iNextPlayer = 0;
    int     nSrcRF;
    int     nDstRF;
    int     iSrcPiece;
    int     iDstPiece;

    const int MAX_COMMANDS = 4;
    int     nCmds;
    char   *aCmds[ MAX_COMMANDS ];
    int     aLens[ MAX_COMMANDS ]; // length of each command
    bool    bBadCommand;

(void) iSrcPiece;
(void) iDstPiece;

    // spin-lock
    // wait for command
    while( !bQuit )
    {
        // TODO: Print move
        //   #. ???
        //   #. <lastmove> ???
        // if (gbBoardValid)
        /*         */ game.Print( gbAnsiOutput );
        iNextPlayer = game.PlayerToPlayNext();
        printf( "%c>", aPLAYERS[ iNextPlayer ] );

        fflush( stdin );

        const size_t MAX_INPUT = 64;
        char   sInputRaw[ MAX_INPUT ];
        fgets( sInputRaw, sizeof( sInputRaw ), stdin );
        sInputRaw[ MAX_INPUT-1 ] = 0;

        char   sInputCooked[ MAX_INPUT ];
        strcpy( sInputCooked, sInputRaw );
        GetInputArguments( sInputCooked, MAX_COMMANDS, nCmds, aCmds, aLens );

        bBadCommand = false;
        if (1 == aLens[0])
        {
            switch( aCmds[0][0] )
            {
                case 'e':
                    // <Piece>Location
                    printf( "Edit: %s\n", aCmds[1] );

                    for( int iPiece = 0; iPiece < (int) sizeof( aPIECES ); iPiece++ )
                    {
                        if (aPIECES[ iPiece ] == aCmds[1][0])
                            iSrcPiece = iPiece;
                    }

                    iDstPiece = iSrcPiece & 7;
                    if ((iDstPiece >= PIECE_PAWN ) && (iDstPiece <= PIECE_KING))
                    {
                        int color = PLAYER_WHITE;
                        if (iSrcPiece > NUM_PIECES)
                            color = PLAYER_BLACK;

                        nDstRF = GetInputLocation( aCmds[1]+1, aLens[1]-1 );
                        if (nDstRF == INVALID_LOCATION)
                            printf( "Error. Invalid location\n" );
                        else
                        {
                            game.Edit( color, iDstPiece, nDstRF );
                        }
                    }
                    else
                            printf( "Error. Invalid piece. Valid: PRNBQKprnbqk\n" );
                    break;

                case 'm':
                    printf( "Param: %s\n", aCmds[1] );
                    nSrcRF = GetInputLocation( aCmds[1]+0, aLens[1]-0 );
                    nDstRF = GetInputLocation( aCmds[1]+2, aLens[1]-2 );
                    if ((nSrcRF == INVALID_LOCATION) || (nDstRF == INVALID_LOCATION))
                        printf( "Error. Invalid location\n" );
                    else
                    {
                        //iSrcPiece = BoardGetPiece( nSrcRF );
                        //iDstPiece = BoardGetPiece( nDsrRF );
                        //bool bDirectCaptures  = // Player x Enemy
                        //bool bIndirectCapture = // pawn: en-passant
                        if (game.MoveOrCapture( nSrcRF, nDstRF ))
                            game.NextTurn();
                    }
                    break;

                case 'q':
                    printf( "Quiting...\n" );
                    // TODO: Send STOP SHUTDOWN to search threads
                    bQuit = true;
                    break;

                default:
                    bBadCommand = true;
                    break;
            }
        }
        else
        if (2 == aLens[0])
        {
            const int iLen = 2;
            if (strncmp( aCmds[0], "ng", iLen ) == 0)
                game.Init();
            else
                bBadCommand = true;
        }
        else
        if (3 == aLens[0])
        {
            const int iLen = 3;
            if (strncmp( aCmds[0], "cls", iLen ) == 0)
                game.Clear();
            else
            if (strncmp( aCmds[0], "fen", iLen ) == 0)
                game.InputFEN( sInputRaw+iLen );
            else
            if (strncmp( aCmds[0], "new",iLen ) == 0)
                game.Init();
            else
                bBadCommand = true;
        }
        else
        if (4 == aLens[0])
        {
            const int iLen = 4;
            if (strncmp( aCmds[0], "quit", iLen ) == 0)
                bQuit = true;
            else
            if (strncmp( aCmds[0], "FEN:", iLen) == 0)
            {
                // https://en.wikipedia.org/wiki/Forsyth%E2%80%93Edwards_Notation
                // http://www.fam-petzke.de/cp_fen_en.shtml
                // Starting position:
                // FEN: rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1
                // http://www.talkchess.com/forum/viewtopic.php?topic_view=threads&p=315993&t=31521&sid=5387b12f25a59c9036c16e6ac900b015
                //   1. e4 e5   ->  1. e2e4 e7e5
                //   2. d3 *    ->  2. d2d3
                // And:
                //   1. d3 e5   ->  1. d2d4 e7e5
                //   2. e4 *    ->  2. e2e4
                // rnbqkbnr/pppp1ppp/8/4p3/4P3/3P4/PPP2PPP/RNBQKBNR b KQkq - 0 2
                game.InputFEN( sInputRaw+4 );
            }
        }
        else
        {
            if (strncmp( aCmds[0], "clear", 5) == 0) // clear board
                game.Clear();
            else
            if (strncmp( aCmds[0], "random", 6) == 0)
            {
                ; // ignore xboard command
            }
            else
                 bBadCommand = true;
        }

        if ( bBadCommand )
            printf( "Ignored command: %s\n", aCmds[0] );
    }; // while bGameRunning

    ShutdownMulticore();

    return 0;
}

