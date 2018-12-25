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
   std::string file;
   std::string format;
   std::string separator;
};

void from_file(std::string const& menu_str)
{
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
        " 1: From file.\n"
        " 2: CSV Fipe file.\n"
      )
      ("indentation,i"
      , po::value<int>(&op.indentation)->default_value(-1)
      , "Used in two situations:\n"
        " - Indentation used in the input file.\n"
        " - Indentation used to output the menu to a file."
        " If -1 will output the number of spaces instead of spaces."
      )
      ("sim-length,l"
      , po::value<int>(&op.sim_length)->default_value(2)
      , "Length of simulated children.")
      ("hash,a", "Output channel codes only."
      )
      ("format,t"
      , po::value<std::string>(&op.format)->default_value("ident")
      , "Input file format. Available options:\n"
        " ident:  Node depth from indentation.\n"
        " number: Node depth from number.\n"
      )
      ("file,f"
      , po::value<std::string>(&op.file)
      , "The file containing the menu. If empty, menu will be simulated.")
      ("separator,s"
      , po::value<std::string>(&op.separator)->default_value("\n")
      , "Separator used in the output file. works with option -i -1 only.")
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
      case 1:
      {
         using iter_type = std::istreambuf_iterator<char>;

         if (std::empty(op.file)) {
            auto const menu_str = gen_sim_menu(op.sim_length);
            from_file(menu_str);
            return 0;
         }

         std::ifstream ifs(op.file);
         std::string menu_str {iter_type {ifs}, {}};
         from_file(menu_str);
      }
      break;
      case 2:
      {
         rt::fipe_dump({op.file, "1", op.indentation});
      }
      break;
      default:
         std::cerr << "Invalid option." << std::endl;
   }

   return 0;
}

