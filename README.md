
# Introduction

pressorpass is a C++ library for evaluating the best move in the TV game show **Press Your Luck**.
This project was developed to investigate the 1983-1986 version of the show,
but could be modified to investigate strategy for the new version as well.

The library is composed of two parts.  The first (pyl) defines the data structures
for the game state (the players' scores, spins, and number of whammies
accumulated) and the game board outcomes (values and extra spins earned).
The second (pyl\_search) defines a tree-based search strategy which evaluates all
possible future outcomes and determines the choices that lead to the best
chances of winning.  All of the data structures are optimized for space.

Analysis assumes round 2, the final round of game play.  The goal is
simply to win the game; no attempt is made to optimize the amount of money
that a player wins.



# Limitations

Perfect strategy is not possible without exact knowledge of the gameboard.
Thus, optimal strategy could vary over time as the board composition
changes (even mid-game, as prize values are rotated; this is not currently
handled).

Also, perfect analysis requires evaluating a large number of future states.
This is both space and time intensive.  In general, each level of lookahead
multiplies the number of states to examine by a factor of (2 \* N), where
N is the number of distinct outcomes of a spin, and the 2 reflects the number
of choices at each decision point (to play or pass).

In order to make the analysis feasible for general-purpose CPUs, score
values are rounded to the nearest $250.  This reduces the value of N.
For example, this merges the $1400 and $1500 outcomes.  The score unit
can be changed if desirable.

Typically, this reduces the number of unique outcomes per spin from 54 (3 x 18
spaces) to between 15-20.

Some special cases are not also handled:

* Ties are not handled perfectly.  The assumption is that each game has
  exactly one winner.  The sum of probabilities of winning for all players
  cannot be greater than 100%.  Internally, a tie between two players
  is treated as a 50% probability of each player winning independently.
* Playing "against the house" is ignored.  With only one player left, that
  player is declared the winner immediately.
* Choice spaces on the gameboard are handled at random; i.e. all
  possibilities are tried with equal probability.  In some cases,
  there is a clear best choice and that could be emphasized.  However,
  this defeats some other optimizations, since the value of a space
  becomes context-dependent.
* The algorithm assumes that all of the players make the optimal move.
  The correct move for player A might be different if it were known
  that player B is aggressive or conservative.



# Implementation

The search module generates a game tree, with one node for each point at which
multiple branches might be taken.  There are 3 classes of nodes:

* decision
nodes, where the branches represent player choices;
* spinning nodes, where
the branches represent random outcomes from a board spin; and
* terminal
nodes, where the game is over and there are no branches to take.

The branches of a spinning node have associated weights, which are the
probabilities of those outcomes occurring.  Decision node branches have no
weights.

The basic algorithm is as follows:

* Create a decision node for the initial state at the root of the decision tree.
* Recursively generate the rest of the tree, from the top down.  For decision nodes,
  generate a node for each choice (play or pass).  For spinning nodes,
  generate a node for each possible outcome.  Terminal nodes become the
  leaves of the tree and are not branched further.
* Once the tree is constructed, assign a payoff to each node.
  The payoff is a 3-tuple of the probabilities of each player winning the game
  from that position.  Payoffs are also defined recursively, from the bottom up,
  as follows:
 * The payoff of a terminal node is 100% for the winning player, and 0% for
   the others.  (For ties, the payoff is distributed equally among all tied
	players.)
 * The payoff of a spinning node is the weighted sum of the payoffs of
   its successor nodes.
 * The payoff of a decision node is the payoff of the choice that is
   higher for the player in control in that position.  Note, this is not
   necessarily the player in control at the root node.

# Lookahead Depth

With "plus a spin" spaces, there is always some non-zero probability of a
game going on forever ("spin battles").  Therefore, tree generation must be
artificially stopped at some point.  The more levels of lookahead, the
more accurate the result will be.  With early termination, some of the
leaves of the tree will not be true terminal states, but decision points
with unknown payoffs.

The maximum tree depth is configurable (default is 64).  When a node is generated at the
maximum depth, that node is not expanded any further,
even if it is not a final state.  In that case, the payoff is defined to be 0% for
all players.  As payoffs propagate upwards from the leaves towards the root,
this means that the sum of the probabilities in a payoff can be less
than 100%, reflecting the incomplete information.

However, it can sometimes still
be discernible whether to pass or play.  For example, if you win 30-50% when
spinning, and 70-90% when passing, clearly you should pass, even though 20% of the
time you don't know what the outcome is.

Also, there is a maximum score that each player is allowed to reach.  By
default, this is $20000.  When this is reached, the option to play is immediately
considered bad strategy and only passing is allowed.



# Other Optimizations

If the player in control is in third space, the choice is always
to play, not pass.  This can be disabled, but this is well-known strategy.

Identical tree nodes are always merged; this can happen when the same game state
can be reached in multiple ways.  For example, any two consecutive
non-whammy spins could be earned in the opposite order.  Because of this,
the tree is actually a directed graph.  It is also acyclic as the
game rules do not permit the same exact game state to happen twice in
the same game.

If the player up has multiple passed spins, the successive nodes can be
computed more efficiently because there is no choice to pass until all
spins are exhausted or a whammy is hit.  For example,
if a player has 3 passed spins, the possible outcomes are: $0 with 2 passed
spins left, $0 with 1 passed spin, or the set of outcomes resulting from
3 non-whammy spins.  This set of outcomes can be precomputed, merging identical
states, and applying commutativity when possible to reduce node count.

If there is only one spin remaining in the game, the choice can be computed
more efficiently, based only on the difference between the player up and
the nearest opponent.  This calculation does not even require recursion due
to the additional spin spaces.  (Not currently implemented.)

Near the end of the game, the possibility of any player whammying out of the
game can become zero.  Then, any two game states which only differ in the
number of whammies per player can be merged into a single state, as there
is no important difference between them.

When there are many spins left in the game to be simulated, it can be helpful
to use a modified gameboard with even fewer distinct outcomes, to reduce the
number of nodes more.  For the fastest evaluation, define a gameboard with
only three (weighted) outcomes: whammy, average value without a spin, and
average value plus a spin.
