#include <stack>
#include <deque>
#include <vector>
#include <string>
#include <cassert>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <iterator>
#include <iostream>
#include <algorithm>

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

namespace occase
{

struct line_comp {
   int field;
   auto operator()( std::vector<std::string> const& v1
                  , std::vector<std::string> const& v2) const
   { return v1.at(field) < v2.at(field); };
};

struct line_comp_pred {
   std::string s;
   int field;
   auto operator()(std::vector<std::string> const& v) const
   { return s == v.at(field); }
};

struct range {
   using iter_type = std::vector<std::vector<std::string>>::iterator;
   iter_type begin;
   iter_type end;
   int field;
};

// The indentation size returned by this function depends on the order
// configured in next_field. Both function have to be changed
// together.
auto calc_indent( int field
                , std::vector<int> const& perm
                , int first_field)
{
   if (field == first_field)
      return 0;

   int indentation = 1;
   while (perm.at(first_field) != field) {
      first_field = perm.at(first_field);
      ++indentation;
   }

   return indentation;
}

auto next_field(int field, std::vector<int> const& perm)
{
   if (field < std::size(perm))
      return perm[field];

   return -1;
}

auto get_indent_str( int f
                   , int i
                   , std::vector<int> const& perm
                   , int first_field)
{
   auto const n = calc_indent(f, perm, first_field);

   if (i == -1) {
      auto str = std::to_string(n);
      str += " ";
      return str;
   }

   return std::string(n * i, ' ');
}

template <class Iter>
std::string print_partitions(
   Iter begin,
   Iter end,
   int indent_size,
   int first_field,
   std::vector<int> perm)
{
   std::deque<std::deque<range>> st;
   st.push_back({{begin, end, first_field}});

   std::ostringstream oss;
   while (!std::empty(st)) {
      auto r = st.back().back();
      st.back().pop_back();
      if (std::empty(st.back()))
         st.pop_back();

      //assert(!std::empty(r.begin->at(r.field)));
      oss << get_indent_str(r.field, indent_size, perm, first_field)
          << r.begin->at(r.field) << "\n";

      auto const next = next_field(r.field, perm);
      if (next == -1)
         continue;

      std::sort(r.begin, r.end, line_comp {next});

      std::deque<range> foo;
      Iter iter = r.begin;
      while (iter != r.end) {
         auto point =
            std::partition_point( iter
                                , r.end
                                , line_comp_pred { iter->at(next)
                                                 , next});

         foo.push_front({iter, point, next});
         iter = point;
      }

      st.push_back(foo);
   }

   return oss.str();
}

std::string split_field(std::string& model)
{
   // The first naive strategy is to split on the first space found.
   auto const i = model.find(' ');
   if (i == std::string::npos) {
      // The string is composed of only one field. Not much we can do.
      return "Todos";
   }

   if (i == 0) {
      throw std::runtime_error("Invalid field.");
   }

   auto const j = model.find_first_not_of(' ', i + 1);
   auto const model2 = model.substr(j);
   if (std::empty(model2)) {
      // The space character was the last on the line. Not much we can
      // do again.
      return "Todos";
   }

   model.erase(i);
   //std::cout << std::setw(20) << std::left << model << " ===> " << model2 << std::endl;

   assert(!std::empty(model2));
   return model2;
}

std::vector<std::string>
split_line(std::string const& line, char sep)
{
   std::string item;
   std::istringstream iss(line);
   std::vector<std::string> fields;
   while (std::getline(iss, item, sep))
      fields.push_back(item);

   return fields;
}

std::string
csv_dump( std::string const& str
        , int indentation
        , std::string const& filter_value
        , int split_idx
        , int filter_idx
        , char sep
        , int first_field
        , std::vector<int> const& perm)
{
   std::istringstream iss(str);

   std::vector<std::vector<std::string>> table;
   std::string line;
   while (std::getline(iss, line)) {
      line.erase( std::remove(std::begin(line), std::end(line), '"')
                , std::end(line));

      auto fields = split_line(line, sep);

      if (split_idx != -1) {
         auto const modelo2 = split_field(fields[split_idx]);
         fields.push_back(modelo2);
      }

      if (std::size(fields) != std::size(perm)) {
         std::cerr << "Incompatible line size: "
                   << std::size(fields) << " != " << std::size(perm)
                   << std::endl;
         std::cerr << "Line: ";
	 std::copy( std::cbegin(fields)
                  , std::cend(fields)
                  , std::ostream_iterator<std::string>(std::cerr, " "));
	 std::cerr << std::endl;
         return {};
      }

      if (filter_idx == -1)
         table.push_back(std::move(fields));
      else if (fields[filter_idx] == filter_value)
         table.push_back(std::move(fields));
   }

   if (std::empty(table))
      return {};

   return print_partitions(
      std::begin(table),
      std::end(table),
      indentation,
      first_field,
      perm);
}

struct csv_cfg {
   bool help {false};
   std::string file;
   int indentation;
   int split_idx;
   int filter_idx;
   std::string filter_value;
   char separator;
   int first_field;

   std::vector<int> perm;
   //   0, 1,  2,  3,  4, 5,  6, 7,  8,  9,  10, 11, 12
   //{ -1, 5, -1, -1, -1, 7, -1, 8, 12, -1, 11,  -1, 10};
};

int impl(csv_cfg const& cfg)
{
   using iter_type = std::istreambuf_iterator<char>;

   if (std::empty(cfg.file))
      return 1;

   std::ifstream ifs(cfg.file);
   std::string str {iter_type {ifs}, {}};

   auto const out =
      csv_dump( str
              , cfg.indentation
              , cfg.filter_value
              , cfg.split_idx
              , cfg.filter_idx
              , cfg.separator
              , cfg.first_field
              , cfg.perm);

   std::cout << out << std::endl;

   return 0;
}

}

//-----------------------------------------------------------------

namespace po = boost::program_options;
using namespace occase;

auto get_cfg(int argc, char* argv[])
{
   csv_cfg cfg;
   std::string perm_str;
   po::options_description desc("Options");
   desc.add_options()
      ("help,h", "This help message.")

      ("file,f"
      , po::value<std::string>(&cfg.file)
      , "The csv file.")

      ("indentation,i"
      , po::value<int>(&cfg.indentation)->default_value(3)
      , "Indentation used in the output. Must be 3 to use with occase-menu.")

      ("first-field,r"
      , po::value<int>(&cfg.first_field)->default_value(0)
      , "The first field to process in the csv file.")

      ("split-index,s"
      , po::value<int>(&cfg.split_idx)->default_value(-1)
      , "If provided will split the corresponding csv field in two.")

      ("filter-index,f"
      , po::value<int>(&cfg.filter_idx)->default_value(-1)
      , "If provided will filter the corresponding csv field occording to."
        " the value provided in --filter-value.")

      ("filter-value,v"
      , po::value<std::string>(&cfg.filter_value)->default_value("")
      , "See --filter-index.")

      ("separator,p"
      , po::value<char>(&cfg.separator)->default_value(':')
      , "The csv separator.")

      ("permutation,m"
      , po::value<std::string>(&perm_str)
      , "Comma separate integers representing the permutation.")
   ;

   po::positional_options_description pos;
   pos.add("file", -1);

   po::variables_map vm;        
   po::store(po::command_line_parser(argc, argv)
      .options(desc).positional(pos).run(), vm);
   po::notify(vm);    

   if (vm.count("help")) {
      cfg.help = true;
      std::cout << desc << "\n";
      return cfg;
   }

   if (!vm.count("permutation")) {
      cfg.help = true;
      std::cout << "A permutation must be provided." << "\n";
      return cfg;
   }

   auto const fields_str = split_line(perm_str, ',');

   std::transform( std::cbegin(fields_str)
                 , std::cend(fields_str)
                 , std::back_inserter(cfg.perm)
                 , [](auto const& s) {return std::stoi(s);});
   return cfg;
}

int main(int argc, char* argv[])
{
   try {
      auto const cfg = get_cfg(argc, argv);
      if (cfg.help)
         return 0;

      return impl(cfg);
   } catch (std::exception const& e) {
      std::cerr << "Error: " << e.what() << std::endl;
   }

   return 1;
}

