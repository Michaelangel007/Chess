struct StateBitBoard_t
{
    bitboard_t _aBoards[ NUM_PIECES ];

    void Dump( int iPlayer )
    {
        for( int iPiece = PIECE_PAWN; iPiece < NUM_PIECES; iPiece++ )
        {
            bitboard_t board  = _aBoards[ iPiece ];
            uint32_t   b1     = board >> 32;
            uint32_t   b0     = board >>  0;

            char       player = aPLAYERS[ iPlayer ];
            char       piece  = aPIECES [ iPiece  ];

            printf( "= %c%c = %08X%08X\n", player, piece, b1, b0 );
            BitBoardPrint( board, piece );
            printf( "\n" );
        }
    }

    // Merge all the bitboards into a single raw board
    // Empty cells are NOT touched so it is safe to call this with both players
    void BitBoardsToRawBoard( int iPlayer, RawBoard_t *board_ )
    {
        for( int iPiece = PIECE_PAWN; iPiece < NUM_PIECES; iPiece++ )
        {
            bitboard_t bits = _aBoards[ iPiece ];
            bitboard_t mask = 0x8000000000000000ull;

            // Enumerate though all bits, filling in the board
            char *p = board_->_cells;

            for( int iCell = 0; iCell < 64; iCell++, p++, mask >>= 1 )
                if( bits & mask )
                    *p = iPiece + (8*iPlayer);
        }
    }

    bitboard_t GetBoardAllPieces()
    {
        bitboard_t board = 0;

        for( int iPiece = PIECE_PAWN; iPiece < NUM_PIECES; iPiece++ )
            board  |= _aBoards[ iPiece ];

        return board;
    }

    uint8_t GetPiece( const uint8_t rankfile )
    {
        bitboard_t board = BitBoardMakeLocation( rankfile );

        for( int iPiece = PIECE_PAWN; iPiece < NUM_PIECES; iPiece++ )
            if( _aBoards[ iPiece ] & board )
                return iPiece;

        return PIECE_EMPTY;
    }
};


struct Move_t
{
    // Location
    uint8_t nSrcRF    ;
    uint8_t nDstRF    ;

    uint8_t iPlayerSrc; // which player
    uint8_t iPlayerDst; // which player

    // Piece
    uint8_t iPieceSrc ;
    uint8_t iPieceDst ;
    uint8_t iEnemySrc ;
    uint8_t iEnemyDst ;
};


enum StateFlags_e
{
     STATE_WHICH_PLAYER      = (1 << 0) // 0=White 1=Black // Optimization: Could encode in _bMoveType, and use 4-bit of Piece type
    ,STATE_CHECK             = (1 << 1)
//    ,STATE_CHECKMATE         = (1 << 2)
//    ,STATE_STALEMATE         = (1 << 3)
    ,STATE_CAN_CASTLE_Q_SIDE = (1 << 3)
    ,STATE_CAN_CASTLE_K_SIDE = (1 << 4)

    ,STATE_CAN_CASTLE_MASK   = STATE_CAN_CASTLE_Q_SIDE | STATE_CAN_CASTLE_K_SIDE
};

enum MoveFlags_e
{
    // 4-bits: which piece was moved
     MOVE_PIECE_MASK        = 0x0F
    ,MOVE_PIECE_SHIFT       = 0

    // 15 different states
    // http://chessprogramming.wikispaces.com/Encoding+Moves
    // 4-bit code: promotion capture special1 special0
    ,MOVE_FLAGS_MASK        = 0xF0
    ,MOVE_FLAGS_SHIFT       = 4
    ,MOVE_NORMAL            = (0 << MOVE_FLAGS_SHIFT) // special0
    ,MOVE_NORMAL_MASK       = (1 << MOVE_FLAGS_SHIFT) // special0 = 0 move without capture
    ,MOVE_PAWN_DOUBLE       = (1 << MOVE_FLAGS_SHIFT) // special0 = 1 pawn moves 2 squares
    ,MOVE_CASTLED_Q_SIDE    = (2 << MOVE_FLAGS_SHIFT) // special1 = Castle
    ,MOVE_CASTLED_K_SIDE    = (3 << MOVE_FLAGS_SHIFT)
    ,MOVE_CAPTURE_ENEMY     = (4 << MOVE_FLAGS_SHIFT) // Capture
    ,MOVE_CAPTURE_EP        = (5 << MOVE_FLAGS_SHIFT)
    ,MOVE_PROMOTE_KNIGHT    = (8 << MOVE_FLAGS_SHIFT) // Promote
    ,MOVE_PROMOTE_BISHOP    = (9 << MOVE_FLAGS_SHIFT) // special0 = piece type
    ,MOVE_PROMOTE_ROOK      = (10<< MOVE_FLAGS_SHIFT) // special1 = piece type
    ,MOVE_PROMOTE_QUEEN     = (11<< MOVE_FLAGS_SHIFT) // special1 = piece type

    ,MOVE_HAS_CASTLED_MASK  = MOVE_CASTLED_Q_SIDE | MOVE_CASTLED_K_SIDE
};

struct State_t
{
    StateBitBoard_t _player[ NUM_PLAYERS ]; // 96 bytes if not storing board[ PIECE_EMPTY ]
    //                            //      96  112 bytes
    uint8_t         _bFlags     ; // +1   97  113
    uint8_t         _bMoveType  ; // +1   98  114  4-bites: Piece move 4-bits: Type of move or capture
    uint16_t        _iParent    ; // +2  100  116  i-node of parent
    uint16_t        _iFirstChild; // +2  102  118  i-node of first child
    uint8_t         _nChildren  ; // +1  103  119  total children
    uint8_t         _nBestMoveRF; // +1  104  120
     int16_t        _nEval      ; // +2  106  122
    uint8_t         _iFrom      ; // +1  107  123  RankFile (0xROWCOL)
    uint8_t         _iTo        ; // +1  108  124  RankFile (0xROWCOL)
    uint8_t         _bPawnsMoved; // +1  109  125  8-bits: Bitflags if pawn[col] has moved
    uint8_t         _iGamePhase ; // +1  110  126  0x00=early game, 0x80=mid-game, 0xFF=end-game
    uint8_t         _pad1       ; // ============
    uint8_t         _pad2       ; //     112  128

    void Zero()
    {
        memset( this, 0, sizeof( *this ) );
    }

    void Init()
    {
printf( "INFO: Boards[]: %u bytes\n", (uint32_t) sizeof( _player ) );
printf( "INFO: State   : %u bytes\n", (uint32_t) sizeof( *this    ) );

        _nEval  = 0;
        _bFlags = 0 | STATE_CAN_CASTLE_Q_SIDE | STATE_CAN_CASTLE_K_SIDE;
        _iFrom  = 0;
        _iTo    = 0;

        _bPawnsMoved = 0;
        _iParent     = 0;
        _iFirstChild = 1;
        _nChildren   = 0;

        StateBitBoard_t *pState;

        pState = &_player[ PLAYER_WHITE ];
        // use board[ PIECE_EMPTY ] as Current Best Search ?
        //pState->_aBoards[ PIECE_EMPTY  ] = 0; // BitBoardMakeWhiteSquares();
        pState->_aBoards[ PIECE_PAWN   ] = BitBoardInitWhitePawn  ();
        pState->_aBoards[ PIECE_ROOK   ] = BitBoardInitWhiteRook  ();
        pState->_aBoards[ PIECE_KNIGHT ] = BitBoardInitWhiteKnight();
        pState->_aBoards[ PIECE_BISHOP ] = BitBoardInitWhiteBishop();
        pState->_aBoards[ PIECE_QUEEN  ] = BitBoardInitWhiteQueen ();
        pState->_aBoards[ PIECE_KING   ] = BitBoardInitWhiteKing  ();

        pState = &_player[ PLAYER_BLACK ];
        //pState->_aBoards[ PIECE_EMPTY  ] = 0; // BitBoardMakeBlackSquares();
        pState->_aBoards[ PIECE_PAWN   ] = BitBoardInitBlackPawn  ();
        pState->_aBoards[ PIECE_ROOK   ] = BitBoardInitBlackRook  ();
        pState->_aBoards[ PIECE_KNIGHT ] = BitBoardInitBlackKnight();
        pState->_aBoards[ PIECE_BISHOP ] = BitBoardInitBlackBishop();
        pState->_aBoards[ PIECE_QUEEN  ] = BitBoardInitBlackQueen ();
        pState->_aBoards[ PIECE_KING   ] = BitBoardInitBlackKing  ();
    }

    void Dump()
    {
        for( int iPlayer = 0; iPlayer < NUM_PLAYERS; iPlayer++ )
        {
            StateBitBoard_t *pState = &_player[ iPlayer ];
            pState->Dump( iPlayer );
        }
    }

    void _GetRawBoard( RawBoard_t& board )
    {
        board.Init();

        for( int iPlayer = 0; iPlayer < NUM_PLAYERS; iPlayer++ )
        {
            StateBitBoard_t *pState = &_player[ iPlayer ];
            pState->BitBoardsToRawBoard( iPlayer, &board );
        }
    }

    void CompactPrintBoard( bool bPrintRankFile = true )
    {
        RawBoard_t board;
        _GetRawBoard( board );
        board.PrintCompact( bPrintRankFile );
    }

    void PrettyPrintBoard( bool bPrintRankFile = true )
    {
        RawBoard_t board;
        _GetRawBoard( board );
        board.PrintPretty( bPrintRankFile );
    }

inline uint8_t GetColorPlayer() { return  _bFlags &  STATE_WHICH_PLAYER; }
inline uint8_t GetColorEnemy () { return ~_bFlags &  STATE_WHICH_PLAYER; }
inline void    TogglePlayer  () {         _bFlags ^= STATE_WHICH_PLAYER; }

    bitboard_t GetPlayerAllPieces( int iPlayer )
    {
        return _player[ iPlayer ].GetBoardAllPieces();
    }

    bitboard_t GetAllPieces()
    {
        bitboard_t board = 0;

        for( int iPlayer = 0; iPlayer < NUM_PLAYERS; iPlayer++ )
            board |= GetPlayerAllPieces( iPlayer );

        return board;
    }

    bool IsCheck() // TODO: uint8_t nKingRankFile = 0xFF
    {
        uint8_t iPlayer = GetColorPlayer();
        uint8_t iEnemy  = GetColorEnemy ();

//        StateBitBoard_t *pStateUs   = &_player[ iPlayer ];
//        StateBitBoard_t *pStateThem = &_player[ iEnemy  ];

        bitboard_t origin           = _player[ iPlayer ]._aBoards[ PIECE_KING ];
        uint8_t    kingRankFile     = BitBoardToRankFile( origin );

        // From the King's location
        // see if any of the enemy's pieces have Line-of-Sight to us

        for( int iPiece = PIECE_QUEEN; iPiece > PIECE_PAWN; iPiece-- )
        {
            bitboard_t movesAll       = 0;
            bitboard_t movesPotential = 0;
            bitboard_t movesValid     = 0;

(void) movesPotential;
(void) movesValid;

            switch( iPiece )
            {
                case PIECE_QUEEN : movesAll = BitBoardMovesColorQueen ( kingRankFile ); break;
                case PIECE_BISHOP: movesAll = BitBoardMovesColorBishop( kingRankFile ); break;
                case PIECE_KNIGHT: movesAll = BitBoardMovesColorKnight( kingRankFile ); break;
                case PIECE_ROOK  : movesAll = BitBoardMovesColorRook  ( kingRankFile ); break;
                case PIECE_PAWN  :
                    if( iEnemy == PLAYER_BLACK )
                        movesAll = BitBoardMovesWhitePawn( kingRankFile );
                    else
                        movesAll = BitBoardMovesBlackPawn( kingRankFile );
                    break;

                movesPotential = movesAll & movesPotential;
            }
        } // for piece

        return false; // FIXME:
    }

    bool IsValidMove( uint8_t fromRankFile, uint8_t toRankFile )
    {
        bitboard_t origin = BitBoardMakeLocation( fromRankFile );
        bitboard_t dest   = BitBoardMakeLocation( toRankFile );

        // get the piece type
        uint8_t iPlayer    = GetColorPlayer();
        uint8_t iEnemy     = GetColorEnemy ();

        uint8_t iPieceSrc  = _player[ iPlayer ].GetPiece( fromRankFile );
        uint8_t iPieceDst  = _player[ iPlayer ].GetPiece( toRankFile );
        uint8_t iEnemyDst  = _player[ iEnemy  ].GetPiece( toRankFile ); // check for capture

        if( iPieceSrc == PIECE_EMPTY )
            return false;

(void) iPieceDst;
(void) iEnemyDst;
(void) origin;
(void) dest;

        // If dest is same color as player mark invalid

        // If new state IsCheck() not a valid move

        // If dest is enemy color

        // Except if King (this color) captures Rook (this color)
        // And King hasn't moved
        // And Rook hasn't moved

        // Check Line-of-Sight from piece

        return false; // FIXME:
    }

    void Move( uint8_t fromRankFile, uint8_t toRankFile )
    {
        uint8_t iPlayer    = GetColorPlayer();
        uint8_t iPieceSrc  = _player[ iPlayer ].GetPiece( fromRankFile );
        uint8_t iPieceDst  = _player[ iPlayer ].GetPiece( toRankFile   );

(void) iPlayer;

        // Get which player the destination is

        int bCanCastleQ = _bFlags & STATE_CAN_CASTLE_Q_SIDE;
        int bCanCastleK = _bFlags & STATE_CAN_CASTLE_K_SIDE;
        int bCanCastle  = _bFlags & STATE_CAN_CASTLE_MASK  ;

        if( iPieceSrc == PIECE_ROOK )
        {
            if( iPlayer == PLAYER_WHITE )
            {
                if( bCanCastleQ && (fromRankFile == 0x00) )
                    _bFlags &= ~STATE_CAN_CASTLE_Q_SIDE; // mark can't castle

                if( bCanCastleK && (fromRankFile == 0x07) )
                    _bFlags &= ~STATE_CAN_CASTLE_K_SIDE; // mark can't castle
            }
            else // PLAYER_BLACK
            {
                if( bCanCastleQ && (fromRankFile == 0x70) )
                    _bFlags &= ~STATE_CAN_CASTLE_Q_SIDE; // mark can't castle

                if( bCanCastleK && (fromRankFile == 0x77) )
                    _bFlags &= ~STATE_CAN_CASTLE_K_SIDE; // mark can't castle
            }
        }
        else
        if( iPieceSrc == PIECE_KING )
        {
            if( bCanCastle )
            {
                bool bJustCastled = false;
                if( iPlayer == PLAYER_WHITE )
                {
                    if( bCanCastleQ && (fromRankFile == 0x04) && (toRankFile == 0x00) )
                    {
                        // Verify Player's rook on toRankFile!
                        if (iPieceDst == PIECE_ROOK)
                        {
                            // Place King 0x02 C1 -- IsCheck()?
                            // Place King 0x03 D1 -- IsCheck()?
                            bool bPassThroughCheck = false;
                            if( !bPassThroughCheck )
                                bJustCastled = SetCastledFlags( MOVE_CASTLED_Q_SIDE );
                        }
                    }

                    if( bCanCastleK && (fromRankFile == 0x05) && (toRankFile == 0x07) )
                    {
                        // Verify Player's rook on toRankFile!
                        if (iPieceDst == PIECE_ROOK)
                        {
                            // Place King 0x05 F1 -- IsCheck()?
                            // Place King 0x06 G1 -- IsCheck()?
                            bool bPassThroughCheck = false;
                            if( !bPassThroughCheck )
                                bJustCastled = SetCastledFlags( MOVE_CASTLED_K_SIDE );
                        }
                    }
                }

                if( iPlayer == PLAYER_BLACK )
                {
                    if( bCanCastleQ && (fromRankFile == 0x74) && (toRankFile == 0x70) )
                    {
                        // Verify Player's rook on toRankFile!
                        if (iPieceDst == PIECE_ROOK)
                        {
                            // Place King 0x72 C8 -- IsCheck()?
                            // Place King 0x73 D8 -- IsCheck()?
                            bool bPassThroughCheck = false;
                            if( !bPassThroughCheck )
                                bJustCastled = SetCastledFlags( MOVE_CASTLED_Q_SIDE );
                        }
                    }

                    if( bCanCastleK && (fromRankFile == 0x75) && (toRankFile == 0x77) )
                    {
                        // Verify Player's rook on toRankFile!
                        if (iPieceDst == PIECE_ROOK)
                        {
                            // Place King 0x75 F8 -- IsCheck()?
                            // Place King 0x76 G8 -- IsCheck()?
                            bool bPassThroughCheck = false;
                            if( !bPassThroughCheck )
                                bJustCastled = SetCastledFlags( MOVE_CASTLED_K_SIDE );
                        }
                    }
                }

                if( bJustCastled )
                {
                    // _bFlags |=  STATE_GAME_MID;

                    bitboard_t oldKing = BitBoardMakeLocation( fromRankFile );
                    bitboard_t oldRook = BitBoardMakeLocation( toRankFile   );

                    // Remove old king, old rook
                    _player[ iPlayer ]._aBoards[ PIECE_KING ] &= ~oldKing;
                    _player[ iPlayer ]._aBoards[ PIECE_ROOK ] &= ~oldRook;

                    bitboard_t newKing = 0;
                    bitboard_t newRook = 0;

                    if( _bMoveType & MOVE_CASTLED_K_SIDE )
                    {
                        newKing = BitBoardMakeLocation( 0x06 + 0x70*iPlayer );
                        newRook = BitBoardMakeLocation( 0x05 + 0x70*iPlayer );
                    }
                    else
                    {
                        newKing = BitBoardMakeLocation( 0x20 + 0x70*iPlayer );
                        newRook = BitBoardMakeLocation( 0x50 + 0x70*iPlayer );
                    }

                    // Place new king, new rook
                    _player[ iPlayer ]._aBoards[ PIECE_KING ] |= newKing;
                    _player[ iPlayer ]._aBoards[ PIECE_ROOK ] |= newRook;
                } else {
                    // If king moving and not castling ...
                    _bFlags &= ~STATE_CAN_CASTLE_MASK;
                }
            }
        }
    }

    void Capture( uint8_t fromRankFile, uint8_t toRankFile )
    {
(void) fromRankFile;
(void) toRankFile ;
    }

    Move_t MakeMove( uint8_t fromRankFile, uint8_t toRankFile )
    {
        uint8_t iPlayer    = GetColorPlayer();
        uint8_t iEnemy     = GetColorEnemy ();

        uint8_t iPieceSrc  = _player[ iPlayer ].GetPiece( fromRankFile );
        uint8_t iPieceDst  = _player[ iPlayer ].GetPiece( toRankFile   );
        uint8_t iEnemySrc  = _player[ iEnemy  ].GetPiece( fromRankFile );
        uint8_t iEnemyDst  = _player[ iEnemy  ].GetPiece( toRankFile   ); // check for capture

        Move_t move;

        move.nSrcRF = fromRankFile;
        move.nDstRF = toRankFile  ;

        move.iPieceSrc = iPieceSrc;
        move.iPieceDst = iPieceDst;
        move.iEnemySrc = iEnemySrc;
        move.iEnemyDst = iEnemyDst;

        return move;
    }

    void MoveOrCapture( uint8_t fromRankFile, uint8_t toRankFile )
    {
        Move_t move = MakeMove( fromRankFile, toRankFile );

        // Move should already be verified:
        // iPieceSrc != PIECE_EMPTY
        // iPieceDst != PIECE_EMPTY
        if ((move.iPieceDst == PIECE_EMPTY) && (move.iEnemyDst == PIECE_EMPTY))
            return;

        // if dst square is empty, the IsMove()
        // Exception: If King tries to capture same color rook
        if ((move.iPieceDst == PIECE_EMPTY) && (move.iEnemyDst == PIECE_EMPTY))
            Move( fromRankFile, toRankFile );
        else
            Capture( fromRankFile, toRankFile );
    }

    float Eval()
    {
        _nEval = 0;

        return _nEval;
    }

    bool SetCastledFlags( int bWhichCastleSide )
    {
        _bFlags     &= ~STATE_CAN_CASTLE_MASK;
        _bMoveType  |=  bWhichCastleSide     ;
        return true;
    }
};

