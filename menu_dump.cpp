#include <stack>
#include <string>
#include <sstream>
#include <iostream>

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

#include "menu_parser.hpp"

using namespace rt;

struct menu_op {
   int menu;
   int indentation;
   int sim_length;
   bool hash = false;
};

struct menu_node {
   std::string name;
   std::vector<menu_node*> children;
};

void bar(menu_op)
{
   std::string menu_str =
   "Brasil\r\n"
   "   Sao Paulo\r\n"
   "      Atibaia\r\n"
   "         Vila Santista\r\n"
   "         Jardim Siriema\r\n"
   "      Braganca\r\n"
   "      Piracaia\r\n"
   "      Sao Paulo\r\n"
   "         Mooca\r\n"
   "         Bixiga\r\n"
   "         Vila Leopoldina\r\n"
   "   Rio de Janeiro\r\n"
   "      Teresópolis\r\n"
   "      Niteroi\r\n"
   "   Amazonas\r\n"
   "      Manaus\r\n"
   "   Paraiba\r\n"
   "   Bahia\r\n"
   ;

   std::cout << menu_str << std::endl;

   //std::string sep = "   ";
   //std::stringstream ss(menu_str);
   //std::string line;
   //menu_node root;
   //std::stack<menu_node*> stack;
   //std::string next_sep = "";
   //while (std::getline(ss, line)) {
   //   if (line.compare(0, std::size(next_sep), next_set) == 0)
   //      next_set += sep;
   //   std::cout << line << std::endl;
   //}
}

void foo(json menu, menu_op op)
{
   if (op.hash) {
      // TODO: Output codes with indentation.
      auto const hashes = get_hashes(menu);
      for (auto const& o : hashes)
         std::cout << o << "\n";
      return;
   }

   std::cout << menu.dump(op.indentation) << std::endl;
}

void op0(menu_op op)
{
   auto const menu = gen_location_menu();
   foo(menu, op);
}

void op1(menu_op op)
{
   auto const menu = gen_sim_menu(op.sim_length);
   foo(menu, op);
}

json gen_location_menu1()
{
   std::vector<json> j3 =
   { {{"hash", ""}, {"name", "Atibaia"},   {"sub", {}}}
   , {{"hash", ""}, {"name", "Sao Paulo"}, {"sub", {}}}
   };

   json j;
   j["name"] = "SP";
   j["sub_desc"] = "Cidades";
   j["sub"] = j3;

   return j;
}

json gen_location_menu3()
{
   std::vector<json> j1 =
   { {{"hash", ""}, {"sub", {}}, {"name", "Centro"}}};

   std::vector<json> j2 =
   { {{"hash", ""}, {"sub", {}}, {"name", "Vila Leopoldina"}, } };

   std::vector<json> j3 =
   { {{"name", "Atibaia"},  {"sub_desc", "Bairros"}, {"sub", j1}}
   , {{"name", "Sao Paulo"},{"sub_desc", "Bairros"}, {"sub", j2}}
   };

   json j;
   j["name"] = "SP";
   j["sub_desc"] = "Cidades";
   j["sub"] = j3;

   return j;
}

json gen_location_menu0()
{
   json j;
   j["name"] = "SP";
   j["sub_desc"] = "Cidades";
   j["sub"] = {};
   j["hash"] = "";

   return j;
}

namespace po = boost::program_options;

int main(int argc, char* argv[])
{
   menu_op op;
   po::options_description desc("Options");
   desc.add_options()
      ("help,h", "produce help message")
      ("menu,m"
      , po::value<int>(&op.menu)->default_value(0)
      , "Choose the menu. Available options:\n"
        " 0: Atibaia - Sao Paulo.\n"
        " 1: Simulated.\n"
        " 2: New menu.\n"
      )
      ("indentation,i"
      , po::value<int>(&op.indentation)->default_value(-1)
      , "Indentation of the menu output.")
      ("sim-length,l"
      , po::value<int>(&op.sim_length)->default_value(2)
      , "Length of simulated children.")
      ("hash,a", "Output channel codes only.")
   ;

   po::variables_map vm;        
   po::store(po::parse_command_line(argc, argv, desc), vm);
   po::notify(vm);    

   if (vm.count("help")) {
      std::cout << desc << "\n";
      return 0;
   }

   if (vm.count("hash"))
      op.hash = true;

   switch (op.menu) {
      case 1: op1(op); break;
      case 2: bar(op); break;
      default:
         op0(op);
   }

   return 0;
}

