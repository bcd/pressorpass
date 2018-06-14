
#include "pyl.hpp"

namespace pyl {

bool operator== (const Player& p0, const Player& p1)
{
	return p0.hash() == p1.hash();
}

bool operator== (const State& ds0, const State& ds1)
{
	/* TODO - use C++17 range for with zip */
	for (int i=0; i < num_players; ++i)
		if (!(ds0.players[i] == ds1.players[i]))
			return false;
	return true;
}

/**
 * Compute the new state that arises from the application of a spin result.
 *
 * This is 1 of 2 primitive game operations.
 *
 * Note that the spin operator defines three things: 1) how many spins were taken;
 * 2) how much score was added, or removed in the case of whammy; and 3) how
 * many additional spins were earned.  Keeping track of spins taken here
 * allows powers of this operator (repeated application) to be precomputed.
 */
State operator* (const SpinValue& opv, const State& sv)
{
	State res(sv);
	Player& up = res.up();
	up.take_spins (opv.taken());
	if (opv.whammy ())
	{
		up.score = 0;
		up.earned += up.passed;
		up.passed = 0;
		up.whammies++;
		if (up.out ())
			up.earned = 0;
	}
	else
	{
		up.score = std::min (static_cast<int> (up.score + opv.score()), SpinValue::MaxScore);
		up.earned += opv.earned();
	}
	res.change_player ();
	up = res.up();

	/* If the current player cannot possibly whammy out, because total spins is too low,
	then set the whammy count to zero.  This can merge equivalent nodes that differ only
	due to the whammy count.  Only player up need be considered as only that player
	has changed due to spinning. */
	if (up.whammies + res.total_spins() < Player::MaxWhammies)
		up.whammies = 0;

	/* TODO: if only one player left in the game, play against the house. */
	if (up.spins() > 0 && res.const_opponent(0).out() && res.const_opponent(1).out())
	{
	}

	return res;
}

/**
 * Compute the new state that arises when player up chooses to pass spins.
 *
 * This is the 2nd of the 2 primitive game operations.
 *
 * FUTURE: At present, there is no choice of the passee in the case of a tie.
 */
State operator* (const PassOperator& op, const State& sv)
{
	State res(sv);
	res.passee().passed += res.up().earned;
	res.up().earned = 0;
	res.change_player ();
	return res;
}

/**
 * Update player up after spinning/passing if necessary.
 */
void State::change_player ()
{
	if (up().spins() == 0) /* TODO: inline this portion */
	{
		/* Search by player order for next player with non-zero spins */
		for (unsigned int i=0; i < num_players; ++i)
		{
			if (player(i).spins() > 0)
			{
				up(i);
				return;
			}
		}
		/* If no player has spins, up remains unchanged, and this signals
		end of game */
	}
}

/**
 * Compute the composition of two spin results sv1 and sv2.
 *
 * This produces a new, combined spin result which has the effect of sv2
 * followed by sv1.
 *
 * This allows the application of multiple spins to be precomputed into a
 * single operator, due to associativity: P(Q(x)) == (PQ)x.  When the
 * spin results are commutative (neither is a whammy), then the results
 * can be simply added.
 */
SpinValue operator* (const SpinValue& sv1, const SpinValue& sv2)
{
	if (sv2.whammy())
		return sv2;
	else if (sv1.whammy())
		return SpinValue (0, sv2.earned(), 1+sv2.taken());
	else
	{
		return SpinValue (sv1.score(), sv2.score(), sv1.earned()+sv2.earned(), sv1.taken()+sv2.taken());
	}
}

/**
 * Apply the spin operator to a state.
 * TODO
 * This function implements WeightedSet multiplication (operator * state vector).
 * Turn into a template function.
 */
ProbState SpinOperator::operator* (const State& ds) const
{
	ProbState res;
	for (const auto& term : expr.terms)
		res.add (term.second, term.first * ds);
	return res;
}

/**
 * Apply a spin operator to another spin operator (composition).
 */
SpinOperator SpinOperator::operator() (const SpinOperator& sop) const
{
	SpinOperator res;
	for (const auto& t : sop.expr.terms) /* merge with SpinValue operator* above */
		for (const auto& u : expr.terms)
			res.expr.add (u.second * t.second, u.first * t.first);
	return res;
}

/**
 * Compare two spin operators for equality.
 */
bool SpinOperator::operator== (const SpinOperator& other) const
{
	for (const auto& term : expr.terms)
		if (term.second != other.expr.terms.at(term.first))
			return false;
	return true;
}

/*********************************************************************/

ostream& operator<< (ostream& os, const State& d)
{
	if (!d.terminal ())
		os << "[P" << d.players[0].up << ' ';
	for (const auto& p : d.players)
	{
		os << '(';
		os << p.score;
		if (p.earned > 0)
			os << " E" << p.earned;
		if (p.passed > 0)
			os << " P" << p.passed;
		if (p.whammies > 0)
			os << " W" << p.whammies;
		os << ") ";
	}
	os << ']';
	return os;
}

ostream& operator<< (ostream& os, const SpinValue& v)
{
	os << '(' << v.score() << '+' << v.earned() << '+' << v.taken() << ')';
	return os;
}

ostream& operator<< (ostream& os, const SpinOperator *sop)
{
	os << "spin";
	os << sop->expr;
	return os;
}

ostream& operator<< (ostream& os, const PassOperator *pop)
{
	os << "pass[]";
	return os;
}

} // namespace pyl
