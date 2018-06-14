#include <ostream>
#include <vector>

#include "pyl.hpp"
#include "pyl_search.hpp"

namespace pyl {

ostream& operator<< (ostream& os, const Node *node)
{
	os.setf(ios::fixed,ios::floatfield);
	os.precision(3);
	os << "(node ";
	node->print (os);
	os << ")";
	return os;
}

ostream& operator<< (ostream& os, const StopCondition& stop)
{
	os << "depth " << stop.depth;
	return os;
}

ostream& operator<< (ostream& os, const Payoff& payoff)
{
	os.setf(ios::fixed,ios::floatfield);
	os.precision(3);
	os << '(';
	if (payoff)
		for_each(payoff.prob.begin(), payoff.prob.end(),
			[&os](Prob p) { os << p << ' ';});
	else
		os << "nil";
	os << ')';
	return os;
}

ostream& operator<< (ostream& os, DecideNode::Decision decision)
{
	switch (decision)
	{
		default:
		case DecideNode::UNDECIDED:
			os << "undecided";
			break;
		case DecideNode::PLAY:
			os << "play";
			break;
		case DecideNode::PASS:
			os << "pass";
			break;
	}
	return os;
}

void TerminalNode::print (ostream& os) const
{
	os << (void *)this << " end" << state << ' ' << payoff_;
}

void SpinNode::print (ostream& os) const
{
	os << (void *)this << " spin " << state << ' ' << payoff_;
}

void DecideNode::print (ostream& os) const
{
	os << (void *)this << " decide " << state << ' ' << payoff_;
	os << ' ' << (void *)if_play;
	os << ' ' << (void *)if_pass;
}


/* Search options */
constexpr bool debug = false;
constexpr bool opt_passed_spin_merge = true;
constexpr bool opt_final_spin = true; /* generalize to self-similar subtree */

Search::Search (const SpinOperator& spin, const SearchOptions& options) :
	spin_op({
		{ spin }, /* not used */
	   { spin },
		{ spin(spin) },
		{ spin(spin(spin)) },
		{ spin(spin(spin(spin))) },
		{ spin(spin(spin(spin(spin)))) },
		{ spin(spin(spin(spin(spin(spin))))) },
	}),
	options_(options)
{
	node_cache_ = new NodeCache();
}

Search::~Search ()
{
	delete node_cache_;
}

DecideNode *Search::run(State init)
{
	init.change_player ();
	clog << "\nSearching " << init << '\n';
	DecideNode *node = node_cache_->create_decide_node (init);

	bool solved = false;
	for (int depth = 4; depth < 64 && !solved; depth += (depth < 32) ? 8 : 4)
	{
		node->scan (*this, StopCondition{depth});
		Payoff payoff = node->payoff ();
		solved = node->solved (result_, options_);

		clog << "depth " << depth << '\n';
		if (node->if_play)
			clog << "   play: " << node->if_play->payoff() << " -> " << result_.play_win << '\n';
		if (node->if_pass)
			clog << "   pass: " << node->if_pass->payoff() << " -> " << result_.pass_win << '\n';
		if (solved)
			clog << "   solved: " << node->decision() << " : " << payoff << '\n';
		clog << "   cache: total " << node_cache_->size() <<
			", final " << node_cache_->final_spin_nodes << '\n';

		node_cache_->apply([] (Node *node) { node->invalidate(); });
#if 0
		if (depth == 4)
		{
			node_cache_->apply([] (Node *node) {
				clog << "> " << node << '\n';
			});
		}
#endif
	}
	return node;
}

TerminalNode *NodeCache::create_terminal_node (const State &ds)
{
	auto& res = terminal_nodes_[ds];
	if (!res)
		res = std::make_unique<TerminalNode> (ds);
	return res.get();
}

SpinNode *NodeCache::create_spin_node (const State &ds)
{
	auto& res = spin_nodes_[ds];
	if (!res)
	{
		res = std::make_unique<SpinNode> (ds);
		if (ds.spins() == 1)
			final_spin_nodes++;
	}
	return res.get();
}

DecideNode *NodeCache::create_decide_node (const State &ds)
{
	auto& res = decide_nodes_[ds];
	if (!res)
		res = std::make_unique<DecideNode> (ds);
	return res.get();
}

/**
 * Create node for an arbitrary state.  The correct node type will be
 * chosen automatically.
 */
Node *NodeCache::create_node (const State &ds)
{
	if (ds.terminal())
		return create_terminal_node (ds);
	else if (ds.can_pass())
		return create_decide_node (ds);
	else
		return create_spin_node (ds);
}

/**
 * Return the payoff for a node.
 */
const Payoff& Node::payoff() const
{
	if (!payoff_)
		calc_payoff ();
	return payoff_;
}


/**
 * Perform a single scan of a tree node.  Construct the entire search tree
 * below it up to the stop condition.  Invalidate the payoff of any node
 * that was modified.
 *
 * Invoking scan on the same node again with the same stop condition is
 * a no-op.  Cached payoffs will not be invalidated in this case.
 */
void Node::scan (const Search& search, const StopCondition& stop)
{
	if (visited())
		return;
	visited(true);

	if (stop.depth == 0)
		return;

	/* If the payoff for this node was calculated in a previous search
	(at a lower total depth) and the uncertainty is low enough, then don't
	scan it any further.  This is the same check that is done in the top
	level search to terminate the entire search at the root node. */
	if (payoff_.uncertainty() <= search.options().max_uncertainty)
		return;

	if (debug)
		clog << "Scanning " << this << " at " << stop << '\n';
	scan_branches(search, stop.deeper());
}

/*********************************************************************/

/* Merge two payoffs when one's winning percentage is not strictly
less than the other.  Be conservative and take the minimum win
percentage of each element. */
/* NOTE: in testing this, it was observed that most of the time,
both parameters are all zero. */
Payoff merge(const Payoff& first, const Payoff& second)
{
	Payoff result;

	for (size_t n = 0; n < num_players; ++n)
		result.prob[n] = std::min(first[n], second[n]);

	return result;
}

/*********************************************************************/

void TerminalNode::scan_branches (const Search& search, const StopCondition& stop)
{
}

void TerminalNode::calc_payoff () const
{
	/* Payoff per player in a final state is 0.0 if you lose, 1.0 if
	you win, and somewhere in between for an unlikely tie.  The sum
	of all components of the payoff vector is always 1.0 for a final
	state, which equates to 0.0 uncertainty. */

	/* TODO - other definitions of payoff could be used.  In particular
	this definition does not account for value won.  We could
	prefer winning with a greater value */

	int max = 0;
	unsigned int count = 0;

	for (const auto& player : state.players)
	{
		if (player.out ())
			;
		else if (player.score == max)
		{
			count++;
		}
		else if (player.score > max)
		{
			max = player.score;
			count = 1;
		}
	}

	for (unsigned int i=0; i < num_players; ++i)
	{
		if (state.players[i].score == max && !state.players[i].out ())
			payoff_.assign (i, 1.0/count);
		else
			payoff_.assign (i, 0);
	}
}

/**
 * Scanning a spinning node means to evaluate all possible outcomes of
 * spinning the board.
 *
 * If the passed spin optimization is enabled, then if the current player
 * has passed spins, more than 1 spin at a time can be computed (up to
 * the point that a whammy is applied, at which point commutativity breaks).
 *
 * This function also supports "partial scan" using the width parameter of
 * the stop condition.  When width==1, only non-plus-a-spin results are
 * scanned; this gives decent coverage for much less processing.
 */
void SpinNode::scan_branches (const Search& search, const StopCondition& stop)
{
	payoff_.invalidate ();

	if (branches.empty())
	{
		unsigned int max_spins;
		if (opt_passed_spin_merge && state.up().passed > 0)
			max_spins = min(static_cast<int> (state.up().passed), 5);
		else
			max_spins = 1;

		ProbState next{ search.spin_op[max_spins] * state };
		Prob coverage = 1.0;
		for (const auto& s : next.terms)
		{
			if (s.first == state)
			{
				//clog << state << " did not change when applying " << search.spin_op + max_spins << '\n';
				coverage -= s.second;
			}
			else
			{
				branches.push_back (Branch{s.second, search.node_cache_->create_node (s.first)});
			}
		}
		if (coverage < 1.0)
		{
			for (auto& branch : branches)
			{
				auto& prob = branch.first;
				prob /= coverage;
			}
		}
	}

	for (auto& branch : branches)
	{
		Node *node = branch.second;
		node->scan (search, stop);
	}
}

/**
 * The payoff for a SpinNode is the weighted sum of the payoffs of
 * each of the spin outcomes.
 */
void SpinNode::calc_payoff () const
{
	payoff_.clear ();
	for (auto& branch : branches)
	{
		auto& node = branch.second;
		auto& prob = branch.first;
		Payoff p = node->payoff();
		p *= prob;
		payoff_ += p;
	}
}

/*********************************************************************/

/**
 * Scanning a decision node means to scan both options (pass or play).
 *
 * As the same node can be scanned more than once, check if the child
 * nodes already exist before creating.
 *
 * If third place spin optimization is enabled (as it should be), then
 * always play.
 */
void DecideNode::scan_branches (const Search& search, const StopCondition& stop)
{
	payoff_.invalidate ();
	SearchResult result;
	const SearchOptions& options = search.options();

	/* if (solved (result, options))
		return; */
	if (!if_pass && !if_play)
	{
		if (options.max_lead && state.lead() > options.max_lead)
			;
		else
			if_play = search.node_cache_->create_spin_node (state);

		if (options.always_spin_third_place && state.third_place())
			;
		else
			if_pass = search.node_cache_->create_node (search.pass_op * state);
	}

	if (if_play)
		if_play->scan (search, stop);
	if (if_pass)
		if_pass->scan (search, stop);
}

/**
 * The payoff for a DecideNode is the payoff of the choice that is
 * more beneficial to the player up.
 *
 * 1. If choices have not been scanned, the payoff is unknown.  This
 *    is due to the search depth limit.
 * 2. If only one choice is valid (must play or must pass), then
 *    that choice's payoff propagates up.
 * 3. If there is a clear better choice, choose the payoff that is
 *    more valuable to the player up.
 * 4. If there is ambiguity, return a merged payoff which makes no
 *    assumptions about which choice would be taken.  The merge
 *    will report the more pessimistic winning probabilities.
 */
void DecideNode::calc_payoff () const
{
	if (!if_play && !if_pass)
		payoff_.clear ();
	else if (!if_play)
		payoff_ = if_pass->payoff();
	else if (!if_pass)
		payoff_ = if_play->payoff();
	else
	{
		auto up = state.up_num();
		auto win_play = if_play->payoff()[up];
		auto win_pass = if_pass->payoff()[up];
		if (win_play > win_pass)
			payoff_ = if_play->payoff();
		else if (win_pass > win_play)
			payoff_ = if_pass->payoff();
		else
			payoff_ = merge(if_pass->payoff(), if_play->payoff());
	}

	if (debug)
	{
		clog << "Decided " << decision();
		if (if_play)
		{
			clog << " between " << if_play->payoff();
			if (if_pass)
				clog << " and " << if_pass->payoff();
		}
		clog << " on " << state << " -> " << payoff_ << '\n';
	}
}

/**
 * Return the decision that was made, after payoff computed.
 */
DecideNode::Decision DecideNode::decision () const
{
	if (if_play && payoff_ == if_play->payoff())
		return DecideNode::PLAY;
	else if (if_pass && payoff_ == if_pass->payoff())
		return DecideNode::PASS;
	else
		return DecideNode::UNDECIDED;
}

bool DecideNode::solved (SearchResult& result, const SearchOptions& options) const
{
	if (!payoff_ || (!if_play && !if_pass))
		return false;

	if (if_play)
		result.play_win = if_play->payoff().range(state.up_num());
	if (if_pass)
		result.pass_win = if_pass->payoff().range(state.up_num());

	if (!if_pass || !if_play)
		return true;

	if (!result.play_win.overlaps (result.pass_win))
		return true;

	if (if_play->payoff().uncertainty() <= options.max_uncertainty &&
		if_pass->payoff().uncertainty() <= options.max_uncertainty)
		return true;

	return false;
}

} // namespace pyl

