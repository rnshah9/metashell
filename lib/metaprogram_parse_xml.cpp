
// Metashell - Interactive C++ template metaprogramming shell
// Copyright (C) 2014, Andras Kucsma (andras.kucsma@gmail.com)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <stack>
#include <tuple>
#include <string>
#include <sstream>
#include <fstream>
#include <iterator>

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include <boost/spirit/include/support_multi_pass.hpp>
#include <boost/fusion/include/adapt_struct.hpp>

#include <metashell/metaprogram.hpp>
#include <metashell/file_location.hpp>

#include "exception.hpp"

BOOST_FUSION_ADAPT_STRUCT(
  metashell::file_location,
  (std::string, name)
  (int, row)
  (int, column)
)

namespace metashell {

namespace spirit = boost::spirit;
namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;
namespace phx = boost::phoenix;

typedef ascii::space_type skipper_t;

struct metaprogram_builder {

  metaprogram_builder();

  void handle_template_begin(
    instantiation_kind kind,
    const std::string& context,
    const file_location& location,
    double timestamp,
    unsigned long long memory_usage);

  void handle_template_end(
    instantiation_kind kind,
    double timestamp,
    unsigned long long memory_usage);

  const metaprogram& get_metaprogram() const;

private:
  typedef metaprogram::vertex_descriptor vertex_descriptor;
  typedef std::map<std::string, vertex_descriptor> element_vertex_map_t;

  vertex_descriptor add_vertex(const std::string& context);

  metaprogram trace;

  std::stack<vertex_descriptor> vertex_stack;

  element_vertex_map_t element_vertex_map;
};

metaprogram_builder::metaprogram_builder() {
  // Add root vertex
  vertex_stack.push(add_vertex("<root>"));
}

void metaprogram_builder::handle_template_begin(
  instantiation_kind kind,
  const std::string& context,
  const file_location& location,
  double timestamp,
  unsigned long long memory_usage)
{
  vertex_descriptor vertex = add_vertex(context);
  if (!vertex_stack.empty()) {
    vertex_descriptor top_vertex = vertex_stack.top();
    trace.add_edge(top_vertex, vertex, kind);
  }
  vertex_stack.push(vertex);
}

void metaprogram_builder::handle_template_end(
  instantiation_kind kind,
  double timestamp,
  unsigned long long memory_usage)
{
  //TODO this check is not enough.
  //The root vertex should always be in the stack
  if (vertex_stack.empty()) {
    throw exception("Bad templight trace format");
  }
  vertex_stack.pop();
}

const metaprogram& metaprogram_builder::get_metaprogram() const {
  return trace;
}

metaprogram_builder::vertex_descriptor metaprogram_builder::add_vertex(
    const std::string& context)
{
  element_vertex_map_t::iterator pos;
  bool inserted;

  std::tie(pos, inserted) = element_vertex_map.insert(
      std::make_pair(context, vertex_descriptor()));

  if (inserted) {
    pos->second = trace.add_vertex(context);
  }
  return pos->second;
}

template<class Iterator>
struct templight_grammar :
  qi::grammar<Iterator, skipper_t> {

  templight_grammar() : templight_grammar::base_type(start) {

    using qi::_val;
    using qi::_1;
    using qi::_2;
    using qi::_3;
    using qi::_4;
    using qi::_5;
    using qi::lit;
    using qi::int_;
    using qi::double_;
    using qi::ulong_long;

    unescaped_characters.add
      ("&lt;", '<')
      ("&gt;", '>')
      ("&apos;", '\'')
      ("&quot;", '"')
      ("&amp;", '&')
    ;

    instantiation_kinds.add
      ("TemplateInstantiation", template_instantiation)
      ("DefaultTemplateArgumentInstantiation", default_template_argument_instantiation)
      ("DefaultFunctionArgumentInstantiation", default_function_argument_instantiation)
      ("ExplicitTemplateArgumentSubstitution", explicit_template_argument_substitution)
      ("DeducedTemplateArgumentSubstitution", deduced_template_argument_substitution)
      ("PriorTemplateArgumentSubstitution", prior_template_argument_substitution)
      ("DefaultTemplateArgumentChecking", default_template_argument_checking)
      ("ExceptionSpecInstantiation", exception_spec_instantiation)
      ("Memoization", memoization)
    ;

    const std::string prologue = "<?xml version=\"1.0\" standalone=\"yes\"?>";

    start %= lit(prologue) >> "<Trace>" >> body >> "</Trace>";

    body = *template_instantiation_rule;

    template_instantiation_rule =
      template_begin >> body >> template_end;


    //<TemplateBegin>
    //    <Kind>TemplateInstantiation</Kind>
    //    <Context context = "fib&lt;7&gt;"/>
    //    <PointOfInstantiation><stdin>|1|50</PointOfInstantiation>
    //    <TimeStamp time = "456168594.581344"/>
    //    <MemoryUsage bytes = "0"/>
    //</TemplateBegin>

    template_begin =
      (
      lit("<TemplateBegin>") >>
        "<Kind>" >> instantiation_kinds >> "</Kind>" >>
        "<Context" >> "context" >> "=" >> context >> "/>" >>
        "<PointOfInstantiation>" >> file_location_rule >> "</PointOfInstantiation>" >>
        "<TimeStamp" >> "time" >> "=" >> timestamp >> "/>" >>
        "<MemoryUsage" >> "bytes" >> "=" >> memory_usage >> "/>" >>
      "</TemplateBegin>"
      )
      [phx::bind(
          &metaprogram_builder::handle_template_begin,
          phx::ref(builder), _1, _2, _3, _4, _5)];

    //<TemplateEnd>
    //    <Kind>TemplateInstantiation</Kind>
    //    <TimeStamp time = "456168594.586666"/>
    //    <MemoryUsage bytes = "0"/>
    //</TemplateEnd>

    template_end =
      (
      lit("<TemplateEnd>") >>
        "<Kind>" >> instantiation_kinds >> "</Kind>" >>
        "<TimeStamp" >> "time" >> "=" >> timestamp >> "/>" >>
        "<MemoryUsage" >> "bytes" >> "=" >> memory_usage >> "/>" >>
      "</TemplateEnd>"
      )
      [phx::bind(
          &metaprogram_builder::handle_template_end,
          phx::ref(builder), _1, _2, _3)];

    unescaped_string %=
      '"' >> *(unescaped_characters | (ascii::print - '"')) >> '"';

    context %= unescaped_string;

    file_location_rule %= *(ascii::print - '|') >> '|' >> int_ >> '|' >> int_;

    memory_usage = '"' >> ulong_long >> '"';
    timestamp = '"' >> double_ >> '"';
  }

  metaprogram_builder builder;

  qi::rule<Iterator, skipper_t> start;
  qi::rule<Iterator, skipper_t> body;
  qi::rule<Iterator, skipper_t> template_instantiation_rule;
  qi::rule<Iterator, skipper_t> template_begin;
  qi::rule<Iterator, skipper_t> template_end;
  qi::rule<Iterator, std::string(), skipper_t> context;
  qi::rule<Iterator, file_location(), skipper_t> file_location_rule;
  qi::rule<Iterator, unsigned long long(), skipper_t> memory_usage;
  qi::rule<Iterator, double(), skipper_t> timestamp;
  qi::rule<Iterator, std::string()> unescaped_string;

  qi::symbols<char, char> unescaped_characters;
  qi::symbols<char, instantiation_kind> instantiation_kinds;
};

metaprogram metaprogram::create_from_xml_stream(std::istream& stream) {

  using boost::spirit::multi_pass;
  using boost::spirit::make_default_multi_pass;

  typedef std::istreambuf_iterator<char> base_iterator;

  templight_grammar<multi_pass<base_iterator>> grammar;

  multi_pass<base_iterator>
    begin(make_default_multi_pass(base_iterator(stream))),
    end(make_default_multi_pass(base_iterator()));

  bool success = qi::phrase_parse(
      begin, end, grammar,
      skipper_t());

  if (!success || begin != end) {
    throw exception("Failed to parse templight file");
  }

  return grammar.builder.get_metaprogram();
}

metaprogram metaprogram::create_from_xml_file(const std::string& file) {
  std::ifstream in(file);
  if (!in) {
    throw exception("Can't open templight file");
  }
  return create_from_xml_stream(in);
}

metaprogram metaprogram::create_from_xml_string(const std::string& string) {
  std::istringstream ss(string);
  return create_from_xml_stream(ss);
}

}
