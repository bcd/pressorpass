#ifndef __PYL_PYL_H
#define __PYL_PYL_H

#include <iostream>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <algorithm>
#include <map>
#include <numeric>

#include "interval.hpp"

/* TODO - add reserve() to vectors where possible */

//#define MAP_CACHE need to define State operator< first

using namespace std;

namespace pyl {

constexpr int num_players = 3;

typedef float Prob; /* as time efficient as double, and more space efficient */

/**
 * SpinValue - represents the result of spinning the board N times (default is N=1).
 *
 * There are 3 components to a value: score, additional earned spins, and taken spins.
 *
 */
struct SpinValue
{
	/* MinScoreUnit defines the minimum unit of score; all scores are stored
	as multiples of this.  This can improve space efficiency but sacrifices some
	accuracy.  For example, 1400 and 1500 would both be stored as 1500, reducing
	the number of unique values to consider.
	   This also reduces the number of possible outcomes after repeated spins.
	For example, since 700 and 750 are both stored as 750, either 1400/1500
	followed by 700/750 would yield 1 unique outcome called 2250, when in
	reality it could have been 2100, 2150, or 2200 also.
	   TODO: Making this a power of 2 will speed up construction. */
	static constexpr int MinScoreUnit = 250;

	/* MaxScore is used to saturate the score value.
	   MaxScore must be a multiple of MinScoreUnit. */
	static constexpr int MaxScore = 20000;

	/* TODO - storing only the number of units and not the actual score will
	speed construction and save space.  Right now a score only needs 7-bits. */

	/* Constructor */
	SpinValue (int score = 0, int earned = 0, int taken = 1)
	{
		u_.score = std::min (SpinValue::MaxScore,
			((score + (MinScoreUnit/2)) / MinScoreUnit) * MinScoreUnit);
		u_.earned = earned;
		u_.taken = taken;
	}

	SpinValue (int score1, int score2, int earned, int taken)
	{
		u_.score = std::min (SpinValue::MaxScore, score1+score2);
		u_.earned = earned;
		u_.taken = taken;
	}


	int score () const { return u_.score; }
	int earned () const { return u_.earned; }
	int taken () const { return u_.taken; }

	unsigned int intval () const { return u_.all; }

	void print (std::ostream& os) const {
		os << u_.score << ' ' << u_.earned << ' ' << u_.taken;
	}

	bool whammy () const { return score() == 0; }
	bool operator== (const SpinValue& other) const { return intval() == other.intval(); }
private:
	/* For space savings, the 3 values are packed into one 32-bit integer. */
	union {
		struct {
			unsigned int score : 16;
			unsigned int earned : 8;
			unsigned int taken : 8;
		};
		unsigned int all;
	} u_;
};


struct Player
{
	/* By the rules of the game, four whammies and you're out. */
	constexpr static int MaxWhammies = 4;

	/* Per-player data is kept small by using bitfields.  Each player data
	   takes 3 32-bit integers. */
	unsigned int score : 16;
	unsigned int earned : 4;
	unsigned int passed : 4;
	unsigned int whammies : 4;
	unsigned int up : 2;
	unsigned int reserved : 2;

	unsigned int spins () const { return earned+passed; }
	void take_spins (unsigned int count)
	{
		if (passed == count)
		{
			passed = 0;
		}
		else if (passed > count)
		{
			passed -= count;
		}
		else if (passed < count)
		{
			earned -= count - passed;
			passed = 0;
		}
	}
	bool can_pass () const { return earned > 0 && passed == 0; }
	bool out () const { return whammies >= MaxWhammies; }
	unsigned int hash () const { return *(reinterpret_cast<const unsigned int *> (this)); }

	friend bool operator== (const Player& p0, const Player& p1);
};


struct State
{
	/* NOTE : this implementation is specific to 3 players */
	Player players[num_players];

	void up (unsigned int u) { players[0].up = u; }
	unsigned int up_num () const { return players[0].up; }
	unsigned int opponent_num (int n) const { return (up_num()+n+1) % num_players; }

	int count_if_out () const
	{
		return std::count_if(players, players+num_players, [] (const auto& p) {
			return p.out();
		});
	}

	Player& player (int i) { return players[i]; }
	Player& up () { return players[up_num()]; }
	const Player& const_up () const { return players[up_num()]; }
	Player& opponent (int n) { return players[opponent_num(n)]; }
	const Player& const_opponent (int n) const { return players[opponent_num(n)]; }
	Player& passee () { return opponent(0).score >= opponent(1).score ? opponent(0) : opponent(1); }
	const Player& const_passee () const { return const_opponent(0).score >= const_opponent(1).score ? const_opponent(0) : const_opponent(1); }
	Player& standby () { return opponent(0).score >= opponent(1).score ? opponent(1) : opponent(0); }
	const Player& const_standby () const { return const_opponent(0).score >= const_opponent(1).score ? const_opponent(1) : const_opponent(0); }

	void change_player ();
	unsigned int spins () const { return players[0].spins() + players[1].spins() + players[2].spins(); }
	bool can_pass () const { return const_up().can_pass(); }
	bool terminal () const { return const_up().spins() == 0 || (const_opponent(0).out() && const_opponent(1).out()); }
	bool third_place () const { return const_up().score < const_opponent(0).score && const_up().score < const_opponent(1).score; }
	bool at_max() const { return const_up().score >= SpinValue::MaxScore; }
	int total_spins () const { return players[0].spins() + players[1].spins() + players[2].spins(); }
	int total_whammies () const { return players[0].whammies + players[1].whammies + players[2].whammies; }
	int lead () const { return const_up().score - const_passee().score; }

	friend bool operator== (const State& ds0, const State& ds1);
};


} // namespace pyl


/* Define the hashing functions to be used for SpinValue and State,
   so they can be inserted into unordered_map.

   hash<SpinValue> is just the identity function, since it is
	stored as a single 32-bit integer.  hash<State> is 3 ints, so
	these are just XORed together.
*/
namespace std {

using pyl::SpinValue;
using pyl::State;

template<>
struct hash<SpinValue>
{
	size_t operator() (const SpinValue& sv) const
	{
		return std::hash<unsigned int>{}(sv.intval());
	}
};

template<>
struct hash<State>
{
	size_t operator() (const State& ds) const
	{
		const unsigned int *dsp = reinterpret_cast<const unsigned int *>(&ds);
		size_t res = std::hash<unsigned int>{}(dsp[0]);
		res ^= (std::hash<unsigned int>{}(dsp[1]) << 1);
		res ^= (std::hash<unsigned int>{}(dsp[2]) << 2);
		return res;
	}
};

} // namespace std


/*
 * WeightedSet - container for a set of items each with associated probability.
 * TODO - move to separate header.
 */
template <class T, class K>
struct WeightedSet
{
	/* std container used to hold the elements.  We use an unordered_map, i.e.
	hash table that maps the element of type T to its probability of type K. */
	typedef unordered_map<T,K> Container;

	Container terms;

	WeightedSet () {}
	explicit WeightedSet (T v) { add (1.0, v); }

	void add (K scalar, T term)
	{
		terms[term] += scalar;
	}

	void spread (const T& value, const T& min, const T& max)
	{
		/* Optimize for space by eliminating an element and replacing it
		with two other elements with half the probability.  This will produce
		less accurate results but the space savings may allow for a greater
		depth of search. */
		K weight = terms[value];
		terms[min] += weight/2;
		terms[max] += weight/2;
		terms.erase(value);
	}

	K weight () const {
		/* Return the total of the weights of all elements */
		K res{0};
		for (auto it = terms.begin(); it != terms.end(); ++it)
			res += (*it).second;
		return res;
	}

	void normalize () {
		/* Normalize the weights so that the total is 1.0. */
		K w = weight();
		for (auto it = terms.begin(); it != terms.end(); ++it)
			(*it).second /= w;
	}

	void print_sorted() const {
		typedef std::pair<T,K> element_type;
		std::vector<element_type> vec(terms.begin(), terms.end());
		std::sort (vec.begin(), vec.end(), []
			(const element_type& e1, const element_type& e2) -> bool
			{
				return e1.second < e2.second;
			}
		);
		for (const auto& elem : vec)
		{
			clog << elem.second << ' ';
			elem.first.print(clog);
			clog << '\n';
		}
	}

	size_t size() const { return terms.size(); }
};

template <typename T, typename K>
ostream& operator<< (ostream& os, const WeightedSet<T,K>& lc)
{
	os << '[';
	for (const auto& term : lc.terms)
	{
		os.precision(3);
		os << term.second << ':' << term.first << ' ';
	}
	os << ']';
	return os;
}



namespace pyl {

typedef WeightedSet<State, Prob> ProbState;

/*
 * Operator - base class for operators, which are action that can be taken
 * on a State.
 */
struct Operator
{
	virtual ~Operator () = default; /* Operator is polymorphic */
};

/*
 * SpinOperator - represents the action of taken 1 or more spins.
 * This is implemented via a weighted set of SpinValues.
 * A SpinOperator can be thought of as "the board".
 */
struct SpinOperator : public Operator
{
	WeightedSet<SpinValue, Prob> expr;

	ProbState operator* (const State& ds) const;
	SpinOperator operator() (const SpinOperator& in) const;
	bool operator== (const SpinOperator& other) const;

protected:
	/* The protected methods are helpers for derived classes to
	   construct the board values efficiently. */

	/* Add a whammy to the board */
	void W () { expr.add (1.0, SpinValue(0, 0)); }
	/* Add a score only space to the board */
	void S (int s, Prob p=0.0) { expr.add (1.0+p, SpinValue(s, 0)); }
	/* Add score plus a spin */
	void SE (int s, Prob p=0.0) { expr.add (1.0+p, SpinValue(s, 1)); }
	/* Add a prize */
	void P (Prob p=0.0) { S(2500, p); }

	SpinOperator spread () const
	{
		SpinOperator res(*this);
		res.expr.spread (SpinValue(4000, 1), SpinValue(3000, 1), SpinValue(5000,1));
		res.expr.spread (SpinValue(1750), SpinValue(1500), SpinValue(2000));
		res.expr.spread (SpinValue(2250), SpinValue(2000), SpinValue(2500));
		return res;
	}

};

struct Spin1 : public SpinOperator
{
	Spin1() : SpinOperator()
	{
		expr.add (0.1, SpinValue(0, 0));     /* Whammy */
		expr.add (0.1, SpinValue(1000, 1));  /* 1000+SPIN */
		expr.add (0.1, SpinValue(4000, 1));  /* 4000+SPIN */
		expr.add (0.2, SpinValue(2000, 0));  /* 2000 */
		expr.add (0.2, SpinValue(500, 0));   /* 500 */
		expr.add (0.1, SpinValue(1000, 0));  /* 1000 */
		expr.add (0.2, SpinValue(2500, 0));  /* 2500 */
	}
};

struct SpinFeb85 : public SpinOperator
{
	/* One of the canonical boards from the 1983-86 series, this one from
	February 1985. */

	constexpr static Prob PC = 1 / 9.0;
	constexpr static Prob B2 = 1 / 3.0;
	constexpr static Prob M1 = 1 / 6.0;
	constexpr static Prob A2 = 1 / 3.0;
	constexpr static Prob BB = 1 / 3.0;

	SpinFeb85() : SpinOperator()
	{
		/* Each call to S, SE, P, or W can pass an additional probability value,
		which is added to the normal probability of 1.0, for cases when that
		space might be awarded due to another "movement space" such as Big Bucks,
		Go Back 2 Spaces, etc. */
		/* 1 */ S(1400,PC); S(1750,PC); S(2250,PC);
		/* 2 */ S(500); S(1250); P();
		/* 3 */ S(500), S(2000), W();
		/* 4 */ SE(3000,B2+BB); SE(4000,B2+BB); SE(5000,B2+BB);
		/* 5 */ S(750); P(); W();
		/* 6 */ SE(700); /* PC=PickACorner; B2=GoBack2; */
		/* 7 */ S(750); P(); W();
		/* 8 */ SE(500,M1); SE(750,M1); SE(1000,M1);
		/* 9 */ S(800); W(); /* Move1(); */
		/* 10 */ P(PC+M1); P(PC+M1); P(PC+M1);
		/* 11 */ S(1500); W(); /* Advance2(); */
		/* 12 */ S(500); W(); /* BB=BigBucks; */
		/* 13 */ S(1500,A2+M1); S(2500,A2+M1); P(A2+M1);
		/* 14 */ S(2000); W(); /* Move1(); */
		/* 15 */ SE(1000,PC+M1); S(2000,PC+M1); P(PC+M1);
		/* 16 */ SE(750); SE(1500); W();
		/* 17 */ S(600); SE(700); P();
		/* 18 */ SE(750); SE(1000); W();

		/* Always normalize at the end of constructor so that the sum of all
		probabilities is 1.0 */
		expr.normalize();
	}
};

struct SpinTest : public SpinOperator
{
	SpinTest()
	{
		expr.add (0.20, SpinValue(0, 0));     /* Whammy */
		expr.add (0.30, SpinValue(1000, 1));  /* 1000+SPIN */
		expr.add (0.50, SpinValue(2000, 0));  /* 2000 */
		/* already normalized */
	}
};

struct PassOperator : public Operator
{
	ProbState operator() (const State& in) const;
};

ostream& operator<< (ostream& os, const State& d);
ostream& operator<< (ostream& os, const SpinValue& v);
ostream& operator<< (ostream& os, const SpinOperator *sop);
ostream& operator<< (ostream& os, const PassOperator *pop);


bool operator== (const Player& p0, const Player& p1);
bool operator== (const State& ds0, const State& ds1);
State operator* (const SpinValue& opv, const State& sv);
State operator* (const PassOperator& op, const State& sv);
SpinValue operator* (const SpinValue& sv1, const SpinValue& sv2);

} // namespace pyl

#endif /* __PYL_PYL_H */
