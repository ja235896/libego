#ifndef _PLAYOUT_H_
#define _PLAYOUT_H_

#include "utils.h"
#include "board.h"

#include <cmath>
#include <iostream>

// mercy rule

const bool use_mercy_rule      = false;
const uint mercy_threshold     = 25;
const uint max_playout_length  = board_area * 2;

enum PlayoutStatus { pass_pass, mercy, too_long};

template <typename Policy> class Playout {
public:
  Policy*  policy;
  Board*   board;
  Move*    move_history;
  uint     move_history_length;
  PlayoutStatus status;

  Playout (Policy* policy_, Board*  board_) : policy (policy_), board (board_) {}

  all_inline flatten
  bool playout_end() {
    if (board->both_player_pass ()) {
      status = pass_pass;
      return true;
    }

    if (board->move_no >= max_playout_length) {
      status = too_long;
      return true;
    }

    if (use_mercy_rule &&
        uint (abs (float(board->approx_score ()))) > mercy_threshold) {
      status = mercy;
      return true;
    }
    return false;
  }

  all_inline
  PlayoutStatus run () {
    uint begin_move_no = board->move_no;
    move_history = board->move_history + board->move_no;

    while (!playout_end()) {
      policy->play_move (board);
    }

    move_history_length = board->move_no - begin_move_no;
    return status;
  }
  
};

// -----------------------------------------------------------------------------

class SimplePolicy {
public:
  SimplePolicy(FastRandom& random_) : random(random_) { 
  }

  all_inline
  void play_move (Board* board) {
    uint ii_start = random.rand_int (board->empty_v_cnt); 
    uint ii = ii_start;
    Player act_player = board->act_player ();

    Vertex v;
    while (true) {
      v = board->empty_v [ii];
      if (!board->is_eyelike (act_player, v) &&
          board->is_pseudo_legal (act_player, v)) { 
        board->play_legal(act_player, v);
        return;
      }
      ii += 1;
      ii &= ~(-(ii == board->empty_v_cnt)); // if (ii==board->empty_v_cnt) ii=0;
      if (ii == ii_start) {
        board->play_legal(act_player, Vertex::pass());
        return;
      }
    }
  }

protected:
  FastRandom& random;
};

typedef Playout<SimplePolicy> SimplePlayout;

// -----------------------------------------------------------------------------

// TODO simplify and test performance
class LocalPolicy : protected SimplePolicy {
public:

  LocalPolicy(FastRandom& random_) : SimplePolicy(random_) { 
  }

  all_inline
  void play_move (Board* board) {
    if (play_local(board)) return;
    SimplePolicy::play_move(board);
  }

private:
  all_inline
  bool play_local(Board *board) {
    // No local move when game begins.
    if (board->move_no == 0) return false;
    // P(local) = 1/3
    if (random.rand_int(100) >= 65) return false;
    
    //if (board->is_panic_mode()) return false;

    Vertex center = board->move_history[board->move_no-1].get_vertex();

    Vertex legal[24];
    //group A - dist 1 diag or horiz
    legal[ 0] = center.N();
    legal[ 1] = center.E();
    legal[ 2] = center.S();
    legal[ 3] = center.W();

    //group B - dist 1
    legal[ 4] = center.NE();
    legal[ 5] = center.SE();
    legal[ 6] = center.SW();
    legal[ 7] = center.NW();

    //group C - dist 2
    legal[ 8] = center.NW().NW();
    reps(i,  9, 12) legal[i] = legal[i-1].E();
    legal[12] = center.NE().NE();
    reps(i, 13, 16) legal[i] = legal[i-1].S();
    legal[16] = center.SE().SE();
    reps(i, 17, 20) legal[i] = legal[i-1].W();
    legal[20] = center.SW().SW();
    reps(i, 21, 24) legal[i] = legal[i-1].N();
    
    // (4/7 - A B C)
    // (2/7 - B A C)
    // (1/7 - C A B)
    uint order = random.rand_int(7);
    if (order == 4)
      if (play_local_group(board, &legal[8], 16)) return true; //C
    if (order >= 5)
      if (play_local_group(board, &legal[4], 4)) return true; //B

    if (play_local_group(board, &legal[0], 4)) return true; //A
    if (order < 5)
      if (play_local_group(board, &legal[4], 4)) return true; //B
    if (order != 4)
      if (play_local_group(board, &legal[8], 16)) return true; //C
    
    return false;
  }

  bool play_local_group(Board *board, Vertex* legal, uint legal_size) {
    Player act_player = board->act_player ();

    uint i_start = random.rand_int(legal_size), i = i_start;
    do {
      if (legal[i].is_on_board() &&
          board->color_at[legal[i]] == Color::empty() &&
          !board->is_eyelike(act_player, legal[i]) &&
          board->is_pseudo_legal(act_player, legal[i])) {
        board->play_legal(act_player, legal[i]);
        return true;
      }
      i = (i + 1) & (legal_size - 1);
    } while (i != i_start);

    // No possibility of a local move
    return false;
  }


};

#endif
