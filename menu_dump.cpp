#include <stack>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <iterator>

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

#include "menu.hpp"
#include "fipe.hpp"

using namespace rt;

struct menu_op {
   int menu;
   int indentation;
   int sim_length;
   bool hash = false;
   std::string file {"menus/cidades"};
};

void from_file(menu_op op)
{
   std::ifstream ifs(op.file);

   using iter_type = std::istreambuf_iterator<char>;
   std::string menu_str {iter_type {ifs}, {}};

   menu m {menu_str};

   for (auto const& o: m.get_codes())
      std::cout << o << "\n";
   std::cout << std::endl;

   //m.dump();
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

void gen_sim_menu(menu_op op)
{
   auto const menu = gen_sim_menu(op.sim_length);
   std::cout << menu << std::endl;
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
        " 1: Simulated.\n"
        " 2: From file.\n"
        " 3: CSV Fipe file.\n"
      )
      ("indentation,i"
      , po::value<int>(&op.indentation)->default_value(-1)
      , "Indentation of the menu output. If -1 will output the"
        " indentation size instead of spaces."
      )
      ("sim-length,l"
      , po::value<int>(&op.sim_length)->default_value(2)
      , "Length of simulated children.")
      ("hash,a", "Output channel codes only.")
      ("file,f"
      , po::value<std::string>(&op.file)
      , "The file containing the menu.")
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
      case 1: gen_sim_menu(op); break;
      case 2: from_file(op); break;
      case 3:
      {
         rt::fipe_dump({op.file, "1", op.indentation});
      }
      break;
      default:
         op0(op);
   }

   return 0;
}

