/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 *                                                                           *
 *  This file is part of Library of Effective GO routines - EGO library      *
 *                                                                           *
 *  Copyright 2006 and onwards, Lukasz Lew                                   *
 *                                                                           *
 *  EGO library is free software; you can redistribute it and/or modify      *
 *  it under the terms of the GNU General Public License as published by     *
 *  the Free Software Foundation; either version 2 of the License, or        *
 *  (at your option) any later version.                                      *
 *                                                                           *
 *  EGO library is distributed in the hope that it will be useful,           *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            *
 *  GNU General Public License for more details.                             *
 *                                                                           *
 *  You should have received a copy of the GNU General Public License        *
 *  along with EGO library; if not, write to the Free Software               *
 *  Foundation, Inc., 51 Franklin St, Fifth Floor,                           *
 *  Boston, MA  02110-1301  USA                                              *
 *                                                                           *
\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "stat.h"

// ----------------------------------------------------------------------

class AafStats {
public:
  Stat unconditional;
  FastMap<Move, Stat> given_move;
  FastMap<Vertex, float> bmw_ownage; // black minus white 
  float bmw_count;

  void reset (float prior = 1.0) {
    unconditional.reset (prior);
    move_for_each_all (m)
      given_move [m].reset (prior);
    bmw_count = 0;
    vertex_for_each_all(v) bmw_ownage[v] = 0;
  }

  void ownage_update(Board& board) {
    bmw_count += 1;
    vertex_for_each_all(v) {
      if (board.color_at[v] != Color::off_board()) {
        bmw_ownage[v] += board.vertex_score(v);
      }
    }
  }

  float ownage(Vertex v) {
    return bmw_ownage[v] / bmw_count;
  }

  void update (Move* move_history, uint move_count, float score) {
    unconditional.update (score);
    rep (ii, move_count)
      given_move [move_history [ii]].update (score);
  }

  float norm_mean_given_move (const Move& m) {
    return given_move[m].mean () - unconditional.mean ();   // ineffective in loop
  }
};

// ----------------------------------------------------------------------

class AllAsFirst : public GtpCommand {
public:
  Board*      board;
  AafStats    aaf_stats;
  uint        playout_no;
  float       aaf_fraction;
  float       influence_scale;
  float       prior;
  bool        progress_dots;

public:
  AllAsFirst (Gtp& gtp, Board& board_) : board (&board_) { 
    playout_no       = 50000;
    aaf_fraction     = 0.5;
    influence_scale  = 6.0;
    prior            = 1.0;
    progress_dots    = false;

    gtp.add_gogui_command (this, "dboard", "AAF.move_value", "black light");
    gtp.add_gogui_command (this, "dboard", "AAF.move_value", "white light");

    gtp.add_gogui_command (this, "dboard", "AAF.move_value", "black atari");
    gtp.add_gogui_command (this, "dboard", "AAF.move_value", "white atari");

    gtp.add_gogui_command (this, "dboard", "AAF.ownage","");
    gtp.add_gogui_command (this, "dboard", "AAF.ownage", "atari");

    gtp.add_gogui_command (this, "var", "show_playout", "");

    gtp.add_gogui_param_float ("AAF.params", "prior",            &prior);
    gtp.add_gogui_param_float ("AAF.params", "aaf_fraction",     &aaf_fraction);
    gtp.add_gogui_param_float ("AAF.params", "influence_scale",  &influence_scale);
    gtp.add_gogui_param_uint  ("AAF.params", "playout_number",   &playout_no);
    gtp.add_gogui_param_bool  ("AAF.params", "20_progress_dots", &progress_dots);
  }

  template<typename Policy>
  void do_playout (const Board* base_board) {
    Board mc_board [1];
    mc_board->load (base_board);

    Policy policy;
    Playout<Policy> playout(&policy, mc_board);
    playout.run ();
    uint aaf_move_count = uint (float(playout.move_history_length)*aaf_fraction);
    float score = mc_board->score ();
    aaf_stats.update (playout.move_history, aaf_move_count, score);
    aaf_stats.ownage_update (*mc_board);
  }

  string show_playout() {
    Board mc_board [1];
    mc_board->load (board);

    AtariPolicy policy;
    Playout<AtariPolicy> playout(&policy, mc_board);
    playout.run ();
    uint aaf_move_count = uint (float(playout.move_history_length)*aaf_fraction);

    stringstream s;
    s << "gogui-gfx: VAR ";
    rep (ii, aaf_move_count) {
      s << playout.move_history [ii].to_string() << " ";
    }
    s << endl << mc_board->score () << endl;
    return s.str();
  }

  void train (bool atari) {
    aaf_stats.reset (prior);
    rep (ii, playout_no) {
      if (progress_dots && (ii * 20) % playout_no == 0) cerr << "." << flush;
      if (atari) {
        do_playout<AtariPolicy> (board);
      } else {
        do_playout<SimplePolicy> (board);
      }
    }
    if (progress_dots) cerr << endl;
  }

  virtual GtpResult exec_command (const string& command, istream& params) {
    if (command == "AAF.move_value") {
      Player player;
      string atari;
      if (!(params >> player)) return GtpResult::syntax_error ();
      params >> atari;
      train(atari == "atari");
      
      FastMap<Vertex, float> means;
      vertex_for_each_all (v) {
        means [v] = aaf_stats.norm_mean_given_move (Move(player, v)) / influence_scale;;
        if (board->color_at [v] != Color::empty ()) 
          means [v] = 0.0;
      }
      return GtpResult::success (to_string_2d (means));
    }

    if (command == "AAF.ownage") {
      string atari;
      params >> atari;

      train(atari == "atari");

      FastMap<Vertex, float> means;
      vertex_for_each_all (v) {
        means [v] = aaf_stats.ownage (v);
      }
      return GtpResult::success (to_string_2d (means));
    }

    if (command == "show_playout") {
      show_playout ();
      return GtpResult::success ();
    }

    assert (false);
  }
};
