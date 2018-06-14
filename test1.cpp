#include <iostream>
#include <cassert>

#include "pyl.hpp"
#include "pyl_search.hpp"

using namespace pyl;

SearchOptions options; /* use defaults */

void size_check ()
{
	clog << "sizeof Payoff = " << sizeof(Payoff) << '\n';
	clog << "sizeof StopCondition = " << sizeof(Payoff) << '\n';
	clog << "sizeof unsigned short = " << sizeof(unsigned short) << '\n';
	clog << "sizeof unsigned int = " << sizeof(unsigned int) << '\n';
	clog << "sizeof void * = " << sizeof(void *) << '\n';
	clog << "sizeof State = " << sizeof(State) << '\n';
	clog << "sizeof Node = " << sizeof(Node) << '\n';
	clog << "sizeof SpinNode = " << sizeof(SpinNode) << '\n';
	clog << "sizeof DecideNode = " << sizeof(DecideNode) << '\n';
	clog << "sizeof TerminalNode = " << sizeof(TerminalNode) << '\n';
	clog << "sizeof SearchOptions = " << sizeof(SearchOptions) << '\n';
}


void run_search (SpinOperator& board, State init)
{
	Search search (board, options);
	search.run(init);
}

void stat_board (const Search& search)
{
	for (unsigned int n=1; n < Search::MaxPassedSpins; ++n)
	{
		clog << search.spin_op+n << '\n';
		clog << search.spin_op[n].expr.size() << '\n';
	}
}

void test_associativity (const SpinOperator& board)
{
	SpinOperator spin2 { board(board) };
	SpinOperator spin3_1 { spin2(board) };
	SpinOperator spin3_2 { board(spin2) };
	clog << &spin3_1 << '\n';
	clog << &spin3_2 << '\n';
	assert (spin3_1 == spin3_2);
}

int main (int argc, char *argv[])
{
	//SpinTest board;
	SpinFeb85 board;
	stat_board (Search (board, options));
	size_check ();

	assert (0 < 1);
	assert (Interval<double>(1.0, 1.1) < Interval<double>(1.2, 1.3));
	assert (Interval<double>(1.0, 1.1).overlaps (Interval<double>(1.1, 1.3)));

	//test_associativity(board);

	run_search (board, State{ {{0}, { 2000, 3}, { 3500, 2 }} }); // expect play
	run_search (board, State{ {{0, 3, 0, 2}, { 2000, 2}, { 3500, 1 }} }); // third place - must play
	run_search (board, State{ {{2000}, { 3000, 3}, { 6000 }} });
	run_search (board, State{ {{0}, { 1000, 10, 0, 3}, { 0, 0, 0, 3 }} });
	run_search (board, State{ {{0}, { 10000, 2}, { 7000, 1 }} });
	run_search (board, State{ {{0}, { 10000, 1}, { 7000, 0 }} });
	return 0;

	//run_search (board, State{ {{0}, { 3000, 5}, { 6000, 4 }} }); // too many spins
	//run_search (board, State{ {{10500}, { 5000, 2}, { 1500, 2 }} }); // never stops!
	//run_search (board, State{ {{7500}, { 2000, 2}, { 1500, 2 }} }); // was nonconvergent
	return 0;
}

