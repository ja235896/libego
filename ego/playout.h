#ifndef _PLAYOUT_H_
#define _PLAYOUT_H_

#include "utils.h"
#include "board.h"

#include <cmath>

// mercy rule

const bool use_mercy_rule      = false;
const uint mercy_threshold     = 25;
const uint max_playout_length  = board_area * 2;

enum playout_status_t { pass_pass, mercy, too_long };

template <typename Policy> class Playout {
public:
  Policy*  policy;
  Board*   board;
  Move*    move_history;
  uint     move_history_length;

  Playout (Policy* policy_, Board*  board_) : policy (policy_), board (board_) {}

  all_inline
  playout_status_t run () {
    uint begin_move_no = board->move_no;
    move_history = board->move_history + board->move_no;
    
    while (true) {
      if (board->both_player_pass ()) {
        move_history_length = board->move_no - begin_move_no;
        return pass_pass;
      }
      
      if (board->move_no >= max_playout_length) {
        move_history_length = board->move_no - begin_move_no;
        return too_long;
      }
      
      if (use_mercy_rule &&
          uint (abs (float(board->approx_score ()))) > mercy_threshold) {
        move_history_length = board->move_no - begin_move_no;
        return mercy;
      }

      policy->play_move (board);
    }
  }
  
};

// -----------------------------------------------------------------------------

class SimplePolicy {
public:
  void play_move (Board* board);
private:
  static FastRandom random; // TODO make it non-static
};

typedef Playout<SimplePolicy> SimplePlayout;

// -----------------------------------------------------------------------------

class AtariPolicy {
public:
  flatten all_inline
  bool try_play (Board* board, Player pl, Vertex v) {
    if (v != Vertex::any() &&
        !board->is_eyelike (pl, v) &&
        board->is_pseudo_legal (pl, v)) { 
      board->play_legal(pl, v);
      // cerr << "bad threat : " << endl;
      // board->print_cerr(board->move_history[board->move_no-1].get_vertex());
      // cerr << board->last_empty_v_cnt << " -> " << board->empty_v_cnt << endl;
      return true;
    }
    return false;
  }

  flatten all_inline
  void play_move (Board* board) {
    Player act_player  = board->act_player();
    Player last_player = board->last_player();

    FastMap<Player, Vertex> atari_v;
    board->pseudo_atari(&atari_v);

    if (try_play(board, act_player, atari_v[last_player])) {
      assertc (playout_ac, board->last_capture_size() > 0);
      return;
    }

    if (try_play(board, act_player, atari_v[act_player])) return;

    simple_policy_.play_move(board);
  }

private:
  SimplePolicy simple_policy_;
};

#endif
