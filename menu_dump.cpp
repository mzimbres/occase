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
#include "json_utils.hpp"

using namespace rt;

struct menu_op {
   int sim_length;
   int oformat;
   int depth;
   std::vector<std::string> files;
   std::string fipe_tipo;
   bool validate = false;
   bool fipe = false;
};

std::string gen_sim_menu(int l)
{
   std::string const sep = "   ";
   std::string str;
   str += "Root\n";
   for (auto i = 0; i < l; ++i) {
      str += sep + "foo\n";
      for (auto j = 0; j < l; ++j) {
         str += sep + sep + "bar\n";
         for (auto j = 0; j < l; ++j)
            str += sep + sep + sep + "foobar\n";
      }
   }

   return str;
}

auto get_file_as_str(std::string const& file, int length)
{
   using iter_type = std::istreambuf_iterator<char>;

   if (std::empty(file))
      return gen_sim_menu(length);

   std::ifstream ifs(file);
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

struct menu_info {
   std::string file;
   int depth = 0;
   int version = 0;
};

/*  Converts the input in the form
 *
 *    file:depth:version
 *
 *  to the struct menu_info.
 */
menu_info convert_to_menu_info(std::string const& data)
{
   if (std::empty(data))
      throw std::runtime_error("Invalid menu info.");

   if (data.front() == ':')
      throw std::runtime_error("Invalid menu info.");

   if (data.back() == ':')
      throw std::runtime_error("Invalid menu info.");

   if (std::size(data) < 2)
      throw std::runtime_error("Invalid menu info.");

   auto const p1 = data.find_first_of(':');

   if (p1 == std::string::npos) {
      // The user did not pass any version or depth.
      return {data, menu::max_supported_depth, 0};
   }

   auto const p2 = data.find_first_of(':', p1 + 1);

   if (p2 == p1 + 1)
      throw std::runtime_error("Invalid menu info.");

   if (p2 == std::string::npos) {
      // The user passed only the depth, not the version.
      return { data.substr(0, p1)
             , std::stoi(data.substr(p1 + 1))
             , 0};
   }
   
   // User passed both the depth and the version.
   return { data.substr(0, p1)
          , std::stoi(data.substr(p1 + 1))
          , std::stoi(data.substr(p2 + 1))};
}

menu_elem
convert_to_menu_elem(std::string const& file_info_raw)
{
   using iter_type = std::istreambuf_iterator<char>;

   auto const info = convert_to_menu_info(file_info_raw);

   std::ifstream ifs(info.file);
   std::string menu_str {iter_type {ifs}, {}};

   menu m {menu_str};

   // TODO: Should we check this. Depends on the app being able to deal
   // with menus whose ranges have different depths.
   //if (!check_leaf_min_depths(m, info.depth))
   //   throw std::runtime_error("Invalid menu.");

   return {m.dump(menu::oformat::counter, '='), info.depth, info.version};
}

int impl(menu_op const& op)
{
   if (op.oformat == 7) {
      std::vector<menu_elem> elems;
      for (auto const& e : op.files)
         elems.push_back(convert_to_menu_elem(e));

      auto const hash_codes = menu_elems_to_codes(elems);
      auto const channels = channel_codes(hash_codes, elems);
      for (auto const& c : channels)
         std::cout << convert_to_hash_code(c, elems) << std::endl;

      return 0;
   }

   if (op.oformat == 6) {
      std::vector<menu_elem> elems;
      for (auto const& e : op.files) {
         auto elem = convert_to_menu_elem(e);
         elems.push_back(std::move(elem));
      }

      json j;
      j["menus"] = elems;
      std::cout << j.dump() << std::flush;
      return 0;
   }

   if (std::empty(op.files))
      return 0;

   menu_info minfo = convert_to_menu_info(op.files.front());

   auto const raw_menu = get_file_as_str(minfo.file, op.sim_length);
   auto menu_str = raw_menu;
   if (op.fipe)
      menu_str = fipe_dump(raw_menu, menu::sep, op.fipe_tipo, '\n');

   menu m {menu_str};

   if (op.oformat == 5) {
      menu_view<0> view {m, op.depth};
      for (auto const& o : view)
         std::cout << get_code_as_str(o.code) << "\n";
   } else {
      auto const oformat = convert_to_menu_oformat(op.oformat);
      auto const line_sep = op.oformat == 2 ? '=' : '\n';
      auto const str = m.dump(oformat, line_sep, op.depth);
      std::cout << str << std::flush;
   }

   if (op.validate)
      std::cout << "Validate: " << check_leaf_min_depths(m, op.depth)
                << std::endl;

   //std::cout << std::endl;
   //std::cout << "Menu max depth:      " << m.get_max_depth() << std::endl;
   //std::cout << "Menu original size:  " << std::size(menu_str) << std::endl;
   //std::cout << "Menu converted size: " << std::size(str) << std::endl;
   return 0;
}

int main(int argc, char* argv[])
{
   menu_op op;
   po::options_description desc("Options");
   desc.add_options()
      ("help,h", "This help message.")
      ("output-format,o"
      , po::value<int>(&op.oformat)->default_value(1)
      , "Format used in the output file. Available options:\n"
        " 1: \tNode depth with indentation.\n"
        " 2: \tNode depth from line first digit.\n"
        " 3: \tHash code plus name.\n"
        " 4: \tOutput all hash codes.\n"
        " 5: \tOutput hash codes at certain depth (see -d option).\n"
        " 6: \tPacks the output in json format ready to be loaded on redis. "
        "Will automatically use field-separator ';' and --validate.\n"
        " 7: \tSimilar to 6 but outputs the combined codes that will "
        "become the channels hash codes."
      )
      ("sim-length,l"
      , po::value<int>(&op.sim_length)->default_value(2)
      , "Length of simulated children. Used only if -f is not provided."
      )
      ("depth,d"
      , po::value<int>(&op.depth)->default_value(std::numeric_limits<int>::max())
      , "Influences the output."
      )
      ("files,f"
      , po::value<std::vector<std::string>>(&op.files)
      , "The file containing the menu. If empty, the menu will be simulated.")
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
      ("fipe,g",
       "The input file is in fipe format."
      )
   ;

   po::positional_options_description pos;
   pos.add("files", -1);

   po::variables_map vm;        
   po::store(po::command_line_parser(argc, argv).
         options(desc).positional(pos).run(), vm);
   po::notify(vm);    

   if (vm.count("help")) {
      std::cout << desc << "\n";
      return 0;
   }

   if (vm.count("validate"))
      op.validate = true;

   if (vm.count("fipe"))
      op.fipe = true;

   return impl(op);
}

