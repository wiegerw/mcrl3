// Author(s): Wieger Wesselink
//            Ruud Koolen
// Copyright: see the accompanying file COPYING or copy at
// https://svn.win.tue.nl/trac/MCRL2/browser/trunk/COPYING
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
/// \file next_state_generator_test.cpp
/// \brief Test for next_state_generator class.

#include "mcrl2/data/rewriter.h"
#include "mcrl2/lps/next_state_generator.h"
#include "mcrl2/lps/parse.h"
#include "mcrl2/lps/state.h"
#include <boost/test/included/unit_test_framework.hpp>
#include <queue>

#include "test_specifications.h"

using namespace mcrl2;
using namespace mcrl2::data;
using namespace mcrl2::data::detail;
using namespace mcrl2::lps;
using namespace mcrl2::lps::detail;

void test_initial_state_successors(const specification& lpsspec)
{
  data::rewriter rewriter(lpsspec.data());
  next_state_generator generator(lpsspec, rewriter);
  next_state_generator::enumerator_queue enumeration_queue;
  for (auto it = generator.begin(generator.initial_state(), &enumeration_queue); it != generator.end(); it++)
  {
    std::cout << atermpp::pp(it->target_state) << std::endl;
  }
}

template <typename NextStateGenerator>
void test_next_state_generator(NextStateGenerator& generator, const specification& lpsspec, std::size_t expected_states, std::size_t expected_transitions, std::size_t expected_transition_labels, bool per_summand)
{
  state initial_state = generator.initial_state();

  std::set<state> visited;
  std::set<state> seen;
  std::set<multi_action> transition_labels;
  std::size_t transitions = 0;

  std::queue<state, std::deque<state> > q;
  q.push(initial_state);
  seen.insert(initial_state);

  while (!q.empty())
  {
    visited.insert(q.front());

    if (per_summand)
    {
      next_state_generator::enumerator_queue enumeration_queue;
      for (std::size_t i = 0; i < lpsspec.process().action_summands().size(); ++i)
      {
        for (auto it = generator.begin(q.front(), i, &enumeration_queue); it != generator.end(); it++)
        {
          transition_labels.insert(it->action);
          ++transitions;
          if (seen.find(it->target_state) == seen.end())
          {
            q.push(it->target_state);
            seen.insert(it->target_state);
          }
        }
      }
    }
    else
    {
      next_state_generator::enumerator_queue enumeration_queue;
      for (auto it = generator.begin(q.front(), &enumeration_queue); it != generator.end(); it++)
      {
        transition_labels.insert(it->action);
        ++transitions;
        if (seen.find(it->target_state) == seen.end())
        {
          q.push(it->target_state);
          seen.insert(it->target_state);
        }
      }
    }
    q.pop();
  }

  BOOST_CHECK(seen.size() == visited.size());
  BOOST_CHECK(seen.size() == expected_states);
  BOOST_CHECK(transitions == expected_transitions);
  BOOST_CHECK(transition_labels.size() == expected_transition_labels);
}

void test_next_state_generator(const specification& lpsspec, std::size_t expected_states, std::size_t expected_transitions, std::size_t expected_transition_labels, bool enumeration_caching, bool per_summand)
{
  data::rewriter rewriter(lpsspec.data());

  if (enumeration_caching)
  {
    cached_next_state_generator generator(lpsspec, rewriter);
    test_next_state_generator(generator, lpsspec, expected_states, expected_transitions, expected_transition_labels, per_summand);
  }
  else
  {
    next_state_generator generator(lpsspec, rewriter);
    test_next_state_generator(generator, lpsspec, expected_states, expected_transitions, expected_transition_labels, per_summand);
  }
}

BOOST_AUTO_TEST_CASE(single_state_test)
{
  std::string text(
    "act  a;\n"
    "proc P(s3: Pos) =\n"
    "       (s3 == 1) ->\n"
    "        a .\n"
    "        P(s3 = 2)\n"
    "     + delta;\n"
    "init P(1);\n"
  );

  specification spec;
  parse_lps(text,spec);
  test_initial_state_successors(spec);
  // Test all 2**3 possible feature flags
  for (std::size_t i = 0; i < 4; i++)
  {
    test_next_state_generator(spec, 2, 1, 1, i & 1, i & 2);
  }
}

BOOST_AUTO_TEST_CASE(test_abp)
{
  specification spec;
  parse_lps(LINEAR_ABP,spec);

  test_initial_state_successors(spec);
  // Test all 2**3 possible feature flags
  for (std::size_t i = 0; i < 4; i++)
  {
    test_next_state_generator(spec, 74, 92, 19, i & 1, i & 2);
  }
}

BOOST_AUTO_TEST_CASE(test_non_true_condition)
{
  std::string text(
    "map  b: Bool;\n"
    "act  a;\n"
    "proc P(s3: Pos) =\n"
    "       (s3 == 1 && b) ->\n"
    "         a .\n"
    "         P(s3 = 2)\n"
    "     + delta;\n"
    "init P(1);\n"
  );
  specification spec;
  parse_lps(text,spec);
  for (std::size_t i = 0; i < 4; i++)
  {
    // The respective sizes do not matter here, since we should get an exception in the nextstate
    // generator.
    BOOST_CHECK_THROW(test_next_state_generator(spec, 0, 0, 0, i&1, i&2), mcrl2::runtime_error);
  }
}

boost::unit_test::test_suite* init_unit_test_suite(int argc, char* argv[])
{
  return nullptr;
}
