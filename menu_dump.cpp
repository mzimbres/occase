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
   unsigned depth;
   std::string file;
   std::string separator;
   std::string fipe_tipo;
   bool validate = false;
};

auto get_file_as_str(menu_op const& op)
{
   using iter_type = std::istreambuf_iterator<char>;

   if (std::empty(op.file))
      return gen_sim_menu(op.sim_length);

   std::ifstream ifs(op.file);
   return std::string {iter_type {ifs}, {}};
}

namespace po = boost::program_options;

menu::oformat convert_to_menu_oformat(int i)
{
   if (i == 1) return menu::oformat::spaces;
   if (i == 2) return menu::oformat::counter;
   if (i == 3) return menu::oformat::info;
   if (i == 4) return menu::oformat::hashes;

   throw std::runtime_error("convert_to_menu_oformat: Invalid input.");

   return menu::oformat::hashes;
}

int main(int argc, char* argv[])
{
   menu_op op;
   po::options_description desc("Options");
   desc.add_options()
      ("help,h", "This help message.")
      ("input-format,i"
      , po::value<int>(&op.input_format)->default_value(1)
      , "Input file format. Available options:\n"
        " 1: Node depth from indentation.\n"
        " 2: Node depth from line first digit.\n"
        " 3: Fipe raw file.\n"
      )
      ("output-format,o"
      , po::value<int>(&op.output_format)->default_value(1)
      , "Format used in the output file. Available options:\n"
        " 1: Node depth with indentation.\n"
        " 2: Node depth from line first digit.\n"
        " 3: Hash code plus name.\n"
        " 4: Output all hash codes.\n"
        " 5: Output hash codes at certain depth (see -d option).\n"
      )
      ("sim-length,l"
      , po::value<int>(&op.sim_length)->default_value(2)
      , "Length of simulated children. Used only if -f is not provided."
      )
      ("depth,d"
      , po::value<unsigned>(&op.depth)->default_value(2)
      , "Outputs all codes at a certain depth."
      )
      ("file,f"
      , po::value<std::string>(&op.file)
      , "The file containing the menu. If empty, the menu will be simulated.")
      ("separator,s"
      , po::value<std::string>(&op.separator)->default_value("\n")
      , "Separator used for each node entry.")
      ("fipe-tipo,k"
      , po::value<std::string>(&op.fipe_tipo)->default_value("1")
      , "Controls which field of the fipe table is read:\n"
        " 1: Cars.\n"
        " 2: Motorcycles.\n"
        " 3: Trucks.\n"
      )
      ("validate,v",
       "Checks whether all leaf nodes have at least the depth "
       " specified in --depth."
      )
   ;

   po::variables_map vm;        
   po::store(po::parse_command_line(argc, argv, desc), vm);
   po::notify(vm);    

   if (vm.count("help")) {
      std::cout << desc << "\n";
      return 0;
   }

   if (vm.count("validate"))
      op.validate = true;

   auto const raw_menu = get_file_as_str(op);
   auto menu_str = raw_menu;
   if (op.input_format == 3)
      menu_str = fipe_dump(raw_menu, menu::sep, op.fipe_tipo);

   menu m {menu_str, menu::iformat::spaces};

   if (op.output_format == 5) {
      auto const codes = m.get_codes_at_depth(op.depth);
      for (auto const& o : codes)
         std::cout << o << "\n";
   } else {
      auto const oformat = convert_to_menu_oformat(op.output_format);

      auto const str = m.dump(oformat, op.separator);
      std::cout << str;
   }

   if (op.validate)
      std::cout << "Validate: " << m.check_leaf_min_depths(op.depth)
                << std::endl;

   //std::cout << std::endl;
   //std::cout << "Menu max depth:      " << m.get_max_depth() << std::endl;
   //std::cout << "Menu original size:  " << std::size(menu_str) << std::endl;
   //std::cout << "Menu converted size: " << std::size(str) << std::endl;
   return 0;
}

