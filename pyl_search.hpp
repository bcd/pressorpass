#ifndef __PYL_SEARCH_H
#define __PYL_SEARCH_H

#include <ostream>
#include <vector>
#ifdef MAP_CACHE
#include <map>
#else
#include <unordered_map>
#endif

#include "pyl.hpp"
#include "interval.hpp"

namespace pyl {

struct Payoff
{
	/* Internally, a Payoff is a set of per-player probabilities, stored as
	a std:array.  A Payoff may also be nullptr-like, to denote when the
	payoff has not yet been computed.  As probabilities are always positive,
	the null case is implemented by storing a negative number in the first
	element. */
	array<Prob, num_players> prob;

	static constexpr Prob null_value = -1.0;

	/* The default constructor initializes to the null value. */
	Payoff () : prob{ null_value, } {}

	/* Constructing with an integer N initializes the win percentage
	for player N to 1, and all other players to 0. */
	Payoff (size_t n) { prob[n] = 1.0; }

	void invalidate () { clear(); prob[0] = null_value; }

	void clear () { fill (prob.begin(), prob.end(), 0.0); }

	bool is_null () const { return prob[0] <= null_value; }
	operator bool () const { return !is_null(); }

	Prob uncertainty () const {
		if (is_null())
			return 1.0;
		else
			return 1.0 - std::accumulate(prob.begin(), prob.end(), Prob(0.0));
	}

	Prob operator[] (size_t n) const { return prob[n]; }

	void assign (size_t n, Prob value) { prob[n] = value; }

	Payoff& operator+= (const Payoff& other) {
		for (size_t i=0; i < prob.size(); ++i) {
			prob[i] += other.prob[i];
		}
		return *this;
	}

	Payoff& operator*= (Prob p) {
		for (size_t i=0; i < prob.size(); ++i) {
			prob[i] *= p;
		}
		return *this;
	}

	bool operator== (const Payoff& other) {
		for (size_t i=0; i < prob.size(); ++i)
			if (prob[i] != other.prob[i])
				return false;
		return true;
	}

	/* range(N) returns the probability of player N winning, expressed
	as an interval */
	Interval<Prob> range(size_t n) const
	{
		return Interval<Prob>(prob[n], prob[n] + uncertainty());
	}
};

struct SearchOptions
{
	Prob max_uncertainty;
	unsigned int max_lead : 16;
	unsigned int max_depth : 8;
	unsigned int max_passed_spins_optimized : 4;
	unsigned int debug : 1;
	unsigned int always_spin_third_place : 1;
	unsigned int merge_passed_spins : 1;
	unsigned int optimize_final_spin : 1;

	SearchOptions() : max_uncertainty(0.03), max_lead(15000), max_depth(50),
		max_passed_spins_optimized(7), debug(false), always_spin_third_place(true),
		merge_passed_spins(true), optimize_final_spin(false)
	{
	}
};

struct SearchResult
{
	Interval<Prob> play_win = Interval<Prob> (0.0, 1.0);
	Interval<Prob> pass_win = Interval<Prob> (0.0, 1.0);
};

struct StopCondition
{
	int depth;
	StopCondition deeper () const { return StopCondition{depth-1}; }
};

struct NodeCache;
struct DecideNode;

struct Search
{
	static const int MaxPassedSpins = 7;

	Search (const SpinOperator& spin, const SearchOptions& options);
	~Search ();

	const SearchOptions& options() const { return options_; }
	SearchResult& result() { return result_; }

	DecideNode *run(State init);

	const SpinOperator spin_op[MaxPassedSpins];
	const PassOperator pass_op;
	mutable NodeCache *node_cache_;
private:
	const SearchOptions options_;
	SearchResult result_;
};

struct Node
{
	State state;
	mutable Payoff payoff_;

	Node (State ds) : state(ds), payoff_(), visited_(false) {}
	virtual ~Node () {}
	void scan (const Search& search, const StopCondition& stop);
	const Payoff& payoff () const;

	bool visited() const { return visited_; }
	void visited(bool v) { visited_ = v; }
	void invalidate() { visited(false); }

	virtual void print (ostream& os) const = 0;
	virtual void scan_branches (const Search& search, const StopCondition& stop) = 0;
	virtual void calc_payoff () const = 0;

private:
	bool visited_;
};

struct TerminalNode : public Node
{
	TerminalNode (State ds) : Node(ds) {}
	virtual ~TerminalNode() {}

	virtual void print (ostream& os) const override ;
	virtual void scan_branches (const Search& search, const StopCondition& stop) override;
	virtual void calc_payoff () const override;
};

struct DecideNode : public Node
{
	Node *if_play;
	Node *if_pass;

	enum Decision { UNDECIDED, PLAY, PASS };

	DecideNode (State ds) : Node(ds), if_play(nullptr), if_pass(nullptr) {}
	virtual ~DecideNode() {}

	virtual void print (ostream& os) const override;
	virtual void scan_branches (const Search& search, const StopCondition& stop) override;
	virtual void calc_payoff () const override;
	Decision decision() const;
	bool solved (SearchResult&, const SearchOptions&) const;

};

struct SpinNode : public Node
{
	typedef pair<Prob, Node *> Branch;
	vector<Branch> branches;

	SpinNode (State ds) : Node(ds) {}
	virtual ~SpinNode() {}
	virtual void print (ostream& os) const override;
	virtual void scan_branches (const Search& search, const StopCondition& stop) override;
	virtual void calc_payoff () const override;
};

struct NodeCache
{
	Node *create_node (const State& ds);
	SpinNode *create_spin_node (const State& ds);
	DecideNode *create_decide_node (const State& ds);
	TerminalNode *create_terminal_node (const State& ds);

	unsigned int final_spin_nodes = 0;
	explicit NodeCache () {
#ifdef MAP_CACHE
#else
		//spin_nodes_.reserve (250000);
		//decide_nodes_.reserve (250000);
#endif
	}
	~NodeCache () = default;
	NodeCache (const NodeCache&) = delete;
	NodeCache& operator= (const NodeCache&) = delete;

	size_t size() const { return spin_nodes_.size() + decide_nodes_.size(); }

	void apply(std::function<void(Node *)> f)
	{
		for (auto& elem : spin_nodes_)
			f(elem.second.get());
		for (auto& elem : decide_nodes_)
			f(elem.second.get());
		for (auto& elem : terminal_nodes_)
			f(elem.second.get());
	}

	void print()
	{
		clog << "Node cache:\n";
		for (auto& elem : spin_nodes_)
		{
			auto& node = elem.second;
			node->print (clog);
			clog << '\n';
		}
		for (auto& elem : decide_nodes_)
		{
			auto& node = elem.second;
			node->print (clog);
			clog << '\n';
		}
	}

private:
#ifdef MAP_CACHE
	map<State, std::unique_ptr<SpinNode>> spin_nodes_;
	map<State, std::unique_ptr<DecideNode>> decide_nodes_;
	map<State, std::unique_ptr<TerminalNode>> terminal_nodes_;
#else
	unordered_map<State, std::unique_ptr<SpinNode>> spin_nodes_;
	unordered_map<State, std::unique_ptr<DecideNode>> decide_nodes_;
	unordered_map<State, std::unique_ptr<TerminalNode>> terminal_nodes_;
#endif
};

template<class T>
ostream& operator<< (ostream& os, const Interval<T>& interval)
{
	os << '[' << interval.min() << ',' << interval.max() << ')';
	return os;
}

ostream& operator<< (ostream& os, const Node *node);
ostream& operator<< (ostream& os, const StopCondition& stop);
ostream& operator<< (ostream& os, const Payoff& payoff);

} // namespace pyl

#endif /* __PYL_SEARCH_H */

