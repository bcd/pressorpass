#include <iostream>
#include <iomanip>
#include <cassert>

#include "pyl.hpp"
#include "pyl_search.hpp"

using namespace pyl;


int main (int argc, char *argv[])
{
	SpinFeb85 board;
	SearchOptions options; /* use defaults */
	options.max_uncertainty = 0.01;
	Search search (board, options);

	const int min = 6000;
	unsigned int lead;
	for (lead = -5000; lead <= 5000; lead += 250)
	{
		State s{ {{0}, { min+lead, 1}, { min, 0 }} };
		DecideNode *node = search.run(s);
		DecideNode::Decision decision = node->decision();
		Node *play_node = node->if_play;
		Node *pass_node = node->if_pass;
		const Payoff& play_payoff = play_node->payoff();
		const Payoff& pass_payoff = pass_node->payoff();

		clog << "lead: " << std::right << std::setw(5) << lead << ' ' << decision <<
			" play " << play_payoff << ' ' << play_payoff.range(1) <<
			" pass " << pass_payoff << ' ' << pass_payoff.range(1) <<
			'\n';
	}
	return 0;
}

