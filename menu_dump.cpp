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
   "Brasil\n"
   "   Sao Paulo\n"
   "      Atibaia\n"
   "         Vila Santista\n"
   "         Jardim Siriema\n"
   "      Braganca\n"
   "      Piracaia\n"
   "      Sao Paulo\n"
   "         Mooca\n"
   "         Bixiga\n"
   "         Vila Leopoldina\n"
   "   Rio de Janeiro\n"
   "      Teresópolis\n"
   "      Niteroi\n"
   "   Amazonas\n"
   "      Manaus\n"
   "   Paraiba\n"
   "   Bahia\n"
   ;

   std::cout << menu_str;

   constexpr auto sep = 3;
   std::stringstream ss(menu_str);
   std::string line;
   menu_node root;
   std::stack<menu_node*> stack;
   unsigned last_depth = 0;
   bool root_found = false;
   while (std::getline(ss, line)) {
      if (line.front() != ' ') {
         // This is the root node since it is the only one with zero
         // indentation.
         if (root_found) {
            std::cerr << "Invalid input data." << std::endl;
            return;
         }

         // From now on no other node will be allowed to have zero
         // indentation.
         root_found = true;
         root.name = line;
         stack.push(&root);
         std::cout << root.name << std::flush;
         continue;
      }

      auto const pos = line.find_first_not_of(" ");
      if (pos == std::string::npos) {
         std::cout << "Invalid line." << std::endl;
         return;
      }

      if (pos % sep != 0) {
         std::cout << "Invalid indentation." << std::endl;
         return;
      }

      auto const dist = pos;
      auto const last_dist = sep * last_depth;
      if (dist > last_dist) {
         if (last_dist + sep != dist) {
            // For increasing depths we allow only one step at time.
            std::cerr << "Jump(1) in indentation not allowed."
                      << std::endl;
            std::cerr << dist << " " << last_dist << std::endl;
            return;
         }

         // We found the child of the last node pushed on the stack.
         auto* p = new menu_node {line, {}};
         stack.top()->children.push_back(p);
         stack.push(p);
         ++last_depth;
      } else if (dist < last_dist) {
         // We do not know how may indentations back we jumped. Let us
         // calculate this.
         auto const new_depth = dist / sep;
         // Now we have to pop that number of nodes from the stack
         // until we get to the node that is should be the parent of
         // the current line.
         stack.pop();
         auto const delta_depth = last_depth - new_depth;
         for (unsigned i = 0; i < delta_depth; ++i)
            stack.pop();

         // Now we can add the new node.
         auto* p = new menu_node {line, {}};
         stack.top()->children.push_back(p);
         stack.push(p);

         last_depth = new_depth;
      } else {
         stack.pop();
         auto* p = new menu_node {line, {}};
         stack.top()->children.push_back(p);
         stack.push(p);
         // Last depth stays equal.
      }
   }
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

