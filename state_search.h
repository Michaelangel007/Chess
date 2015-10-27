/*

http://www.chessbin.com/post/Quiescence-Search-and-Extensions.aspx
http://www.chessbin.com/post/Move-Searching-and-Alpha-Beta.aspx

*/

const int MAX_SEARCH_MOVES = 63; // Don't need to search moving into our current position

struct SearchMove_t
{
    uint8_t _nMoves;
    uint8_t _aMoves[ 63 ]; // 0xRankFile
};

/* To search we need:
      initial board layout
      list of possible moves to search and evaluate their strength
*/
struct SearchState_t
{
    State_t      _state;
    SearchMove_t _moves;
};

