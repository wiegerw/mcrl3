// Author(s): Wieger Wesselink
// Copyright: see the accompanying file COPYING or copy at
// https://svn.win.tue.nl/trac/MCRL2/browser/trunk/COPYING
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
/// \file mcrl2/process/eliminate_single_usage_equations.h
/// \brief add your file description here.

#ifndef MCRL2_PROCESS_ELIMINATE_SINGLE_USAGE_EQUATIONS_H
#define MCRL2_PROCESS_ELIMINATE_SINGLE_USAGE_EQUATIONS_H

#include <algorithm>
#include "mcrl2/core/detail/print_utility.h"
#include "mcrl2/data/rewriter.h"
#include "mcrl2/process/builder.h"
#include "mcrl2/process/expand_process_instance_assignments.h"
#include "mcrl2/process/eliminate_unused_equations.h"
#include "mcrl2/process/process_equation_index.h"
#include "mcrl2/process/remove_data_parameters_restricted.h"
#include "mcrl2/process/rewrite.h"
#include "mcrl2/process/traverser.h"
#include "mcrl2/utilities/detail/container_utility.h"
#include "mcrl2/utilities/detail/join.h"

namespace mcrl2 {

namespace process {

namespace detail {

/// Counts count of process instances, and creates a dependency graph
struct eliminate_single_usage_traverser: public process_expression_traverser<eliminate_single_usage_traverser>
{
  typedef process_expression_traverser<eliminate_single_usage_traverser> super;
  using super::enter;
  using super::leave;
  using super::apply;

  std::map<process_identifier, int>& count;
  std::map<process_identifier, std::set<process_identifier> >& dependencies;
  process_identifier current_equation_identifier;

  eliminate_single_usage_traverser(std::map<process_identifier, int>& count_, std::map<process_identifier, std::set<process_identifier> >& dependencies_)
    : count(count_), dependencies(dependencies_)
  {}

  void enter(const process::process_specification& x)
  {
    for (const process_equation& eqn: x.equations())
    {
      count[eqn.identifier()] = 0;
    }
  }

  void enter(const process_equation& x)
  {
    current_equation_identifier = x.identifier();
    dependencies[x.identifier()]; // initialize to empty set
  }

  void apply(const process::process_instance& x)
  {
    count[x.identifier()]++;
    dependencies[current_equation_identifier].insert(x.identifier());
  }

  void apply(const process::process_instance_assignment& x)
  {
    count[x.identifier()]++;
    dependencies[current_equation_identifier].insert(x.identifier());
  }
};

struct expand_process_instances_builder: public process_expression_builder<expand_process_instances_builder>
{
  typedef process_expression_builder<expand_process_instances_builder> super;
  using super::enter;
  using super::leave;
  using super::apply;

  const process_equation_index& equation_index;
  const std::vector<process_identifier>& instances;

  expand_process_instances_builder(process_equation_index& equation_index_, std::vector<process_identifier> instances_)
   : equation_index(equation_index_), instances(instances_)
  {}

  process_expression apply(const process::process_instance& x)
  {
    auto i = std::find(instances.begin(), instances.end(), x.identifier());
    if (i == instances.end())
    {
      return x;
    }
    data::mutable_map_substitution<> sigma;
    const process_equation& eqn = equation_index.equation(x.identifier().name());
    const data::variable_list& d = eqn.formal_parameters();
    const data::data_expression_list& e = x.actual_parameters();
    auto di = d.begin();
    auto ei = e.begin();
    for (; di != d.end(); ++di, ++ei)
    {
      sigma[*di] = *ei;
    }
    auto result = process::replace_variables_capture_avoiding(x, sigma, data::substitution_variables(sigma));
    return result;
  }

  process_expression apply(const process::process_instance_assignment& x)
  {
    auto i = std::find(instances.begin(), instances.end(), x.identifier());
    if (i == instances.end())
    {
      return x;
    }
    data::mutable_map_substitution<> sigma;
    for (const auto& a: x.assignments())
    {
      sigma[a.lhs()] = a.rhs();
    }
    auto result = process::replace_variables_capture_avoiding(x, sigma, data::substitution_variables(sigma));
    return result;
  }
};

} // namespace detail

// Removes trivial equations of the form P() = P1() + ... + Pn()
struct eliminate_single_usage_equations_algorithm
{
  process_specification& procspec;
  data::rewriter R;

  // Contains the number of times each process variable is used
  std::map<process_identifier, int> count;

  // Contains the process variables that appear in the right hand sides of each equation
  std::map<process_identifier, std::set<process_identifier> > dependencies;

  // Contains the equations that will be eliminated. An equation P is eliminated
  // if count[P] == 1 and P is not an element of dependencies[P]
  std::set<process_identifier> to_be_eliminated;
  std::vector<process_identifier> to_be_kept;

  // Contains the order in which substitutions will be applied
  std::vector<process_identifier> substitution_order;

  eliminate_single_usage_equations_algorithm(process_specification& procspec_)
    : procspec(procspec_),
      R(procspec.data())
  {}

  std::vector<process_identifier> count_one_dependencies(const process_identifier& x)
  {
    std::vector<process_identifier> result;
    for (const process_identifier& y: dependencies[x])
    {
      if (x != y && count[y] == 1)
      {
        result.push_back(y);
      }
    }
    return result;
  }

  void print_dependencies() const
  {
    mCRL2log(log::debug) << "--- dependencies ---" << std::endl;
    for (const auto& i: dependencies)
    {
      mCRL2log(log::debug) << i.first << " " << core::detail::print_set(i.second) << std::endl;
    }
  }

  void compute_count_dependencies()
  {
    mCRL2log(log::verbose) << "Compute count dependencies" << std::endl;
    detail::eliminate_single_usage_traverser f(count, dependencies);
    f.apply(procspec);
    mCRL2log(log::debug) << "--- process equation count ---" << std::endl;
    for (const auto& i: count)
    {
      mCRL2log(log::debug) << i.first << " -> " << i.second << std::endl;
    }
  }

  void compute_to_be_eliminated()
  {
    mCRL2log(log::verbose) << "Compute to_be_eliminated" << std::endl;
  	using utilities::detail::contains;
    for (const auto& i: dependencies)
    {
      const process_identifier& P = i.first;
      const std::set<process_identifier>& dependencies_P = i.second;
      if (count[P] <= 1 && !contains(dependencies_P, P))
      {
        to_be_eliminated.insert(P);
      }
      else
      {
        to_be_kept.push_back(P);
      }
    }
  }

  std::set<process_identifier> set_intersection(const std::set<process_identifier>& V, const std::set<process_identifier>& W)
  {
    std::set<process_identifier> result;
    std::set_intersection(V.begin(), V.end(), W.begin(), W.end(), std::inserter(result, result.begin()));
    return result;
  }

  void compute_substitution_order()
  {
    mCRL2log(log::verbose) << "Compute substitution order" << std::endl;

  	using utilities::detail::contains;

    // cleanup dependencies
    for (auto& i: dependencies)
    {
      i.second = set_intersection(i.second, to_be_eliminated);
    }

    // add the equations that will be eliminated, in the proper order
    while (true)
    {
      std::set<process_identifier> remove;
      for (auto i = dependencies.cbegin(); i != dependencies.cend();)
      {
        if (i->second.empty())
        {
          substitution_order.push_back(i->first);
          remove.insert(i->first);
          dependencies.erase(i++);
        }
        else
        {
          ++i;
        }
      }
      if (remove.empty())
      {
        break;
      }
      substitution_order.insert(substitution_order.end(), remove.begin(), remove.end());
      for (auto& p: dependencies)
      {
        // p.second := p.second \ remove
        std::set<process_identifier> tmp;
        std::set_difference(std::make_move_iterator(p.second.begin()),
                            std::make_move_iterator(p.second.end()),
                            remove.begin(), remove.end(),
                            std::inserter(tmp, tmp.begin()));
        p.second.swap(tmp);
      }
    }

    // add the equations that will not be eliminated
    for (const process_equation& eqn: procspec.equations())
    {
      const process_identifier& P = eqn.identifier();
      if (!contains(to_be_eliminated, P))
      {
        substitution_order.push_back(P);
      }
    }

    mCRL2log(log::debug) << "substitution order: " << core::detail::print_list(substitution_order) << std::endl;
  }

  void apply_substitutions()
  {
    mCRL2log(log::verbose) << "Apply substitutions" << std::endl;

    process_equation_index equation_index(procspec);

    // apply substitutions to the equation in the order given by substitution_order
    for (const process_identifier& P: substitution_order)
    {
      process_equation& eqn = equation_index.equation(P.name());
      detail::expand_process_instances_builder f(equation_index, count_one_dependencies(P));
      eqn = process::rewrite(f.apply(eqn), R);
    }
  }

  void run()
  {
    compute_count_dependencies();
    print_dependencies();
    compute_to_be_eliminated();
    compute_substitution_order();
    apply_substitutions();
    eliminate_unused_equations(procspec.equations(), procspec.init());
  }
};

/// \brief Eliminates equations that are used only once, using substitution.
void eliminate_single_usage_equations(process_specification& procspec)
{
  eliminate_single_usage_equations_algorithm algorithm(procspec);
  algorithm.run();
}

} // namespace process

} // namespace mcrl2

#endif // MCRL2_PROCESS_ELIMINATE_SINGLE_USAGE_EQUATIONS_H
