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
   int sim_length;
   int input_format;
   int output_format;
   bool hash = false;
   std::string file;
   std::string separator;
};

void from_file(std::string const& menu_str)
{
   menu m {menu_str};

   std::cout << std::endl;
   for (auto const& o: m.get_leaf_codes())
      std::cout << o << "\n";
   std::cout << std::endl;

   //m.dump();
}

auto get_file_as_str(menu_op const& op)
{
   using iter_type = std::istreambuf_iterator<char>;

   if (std::empty(op.file))
      return gen_sim_menu(op.sim_length);

   std::ifstream ifs(op.file);
   return std::string {iter_type {ifs}, {}};
}

namespace po = boost::program_options;

int main(int argc, char* argv[])
{
   menu_op op;
   po::options_description desc("Options");
   desc.add_options()
      ("help,h", "This hep message.")
      ("input-format,i"
      , po::value<int>(&op.input_format)->default_value(1)
      , "Input file format. Available options:\n"
        " 1: Node depth from indentation.\n"
        " 2: Node depth from number.\n"
        " 3: Fipe raw format.\n"
      )
      ("output-format,o"
      , po::value<int>(&op.output_format)->default_value(1)
      , "Indentation used in the output file. Available options:\n"
        " 1: Node depth with indentation.\n"
        " 2: Node depth with number.\n"
        " 3: Only hash codes are output.\n"
      )
      ("sim-length,l"
      , po::value<int>(&op.sim_length)->default_value(2)
      , "Length of simulated children.")
      ("hash,a", "Adds hash code to the output."
      )
      ("file,f"
      , po::value<std::string>(&op.file)
      , "The file containing the menu. If empty, menu will be simulated.")
      ("separator,s"
      , po::value<std::string>(&op.separator)->default_value("\n")
      , "Separator used in the output file.")
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

   auto const raw_menu = get_file_as_str(op);

   switch (op.input_format) {
      case 1:
      {
         menu m {raw_menu};

         auto const str = m.dump(op.output_format, op.hash);
         std::cout << str << std::endl;
      }
      break;
      case 3:
      {
         auto const menu_str = rt::fipe_dump(raw_menu, {"1", 3});
         menu m {menu_str};

         auto const str = m.dump(op.output_format, op.hash);
         std::cout << str << std::endl;
      }
      break;
      default:
         std::cerr << "Invalid option." << std::endl;
   }

   return 0;
}

