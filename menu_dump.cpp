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

using namespace rt;

struct menu_op {
   int menu;
   int indentation;
   int sim_length;
   bool hash = false;
   std::string file {"menus/cidades"};
};

void bar(menu_op op)
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

   std::cout << menu_str << std::endl;
   menu m {menu_str};

   auto const codes = m.get_codes();
   for (auto const& c : codes)
      std::cout << c << std::endl;
   std::cout << std::endl;

   m.dump();

   for (auto const& o: m.get_codes())
      std::cout << o << "\n";
   std::cout << std::endl;

}

void from_file(menu_op op)
{
   std::ifstream ifs(op.file);

   using iter_type = std::istreambuf_iterator<char>;
   std::string menu_str {iter_type {ifs}, {}};

   menu m {menu_str};

   for (auto const& o: m.get_codes())
      std::cout << o << "\n";
   std::cout << std::endl;

   m.dump();
}

enum fipe_fields
{ id, tipo, id_modelo_ano, fipe_codigo, id_marca, marca, id_modelo, modelo, ano, name, combustivel, preco
};

void fipe_csv(menu_op op)
{
   std::ifstream ifs(op.file);

   std::vector<std::vector<std::string>> table;
   std::string line;
   while (std::getline(ifs, line)) {
      std::string item;
      std::istringstream iss(line);
      std::vector<std::string> fields;
      while (std::getline(iss, item, ';')) {
         item.erase( std::remove(std::begin(item), std::end(item), '"')
                   , std::end(item));
         fields.push_back(item);
      }

      assert(std::size(fields) == 12);

      if (fields[fipe_fields::tipo] == "1")
         table.push_back(std::move(fields));
   }

   std::cout << "Table size: " << std::size(table) << std::endl;

   auto const comp_marca = [](auto const& m1, auto const& m2)
   { return m1.at(fipe_fields::marca) < m2.at(fipe_fields::marca); };

   std::sort(std::begin(table), std::end(table), comp_marca);

   //for (auto const& o : table)
   //   std::cout << o.at(fipe_fields::marca) << " ===> " << o.at(fipe_fields::modelo) << std::endl;

   auto begin = std::begin(table);
   auto n = 0;
   while (begin != std::end(table)) {
      auto const m1 = begin->at(fipe_fields::marca);
      auto const comp1 = [&m1](auto const& m2)
      { return m1 == m2.at(fipe_fields::marca); };
      auto old_begin = begin;
      begin = std::partition_point(begin, std::end(table), comp1);
      std::cout << m1 << ": " <<std::distance(old_begin, begin) << std::endl;
      ++n;
   }

   std::cout << "Number of partitions: " << n << std::endl;
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
   std::cout << menu << std::endl;
   //foo(menu, op);
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
        " 3: From file.\n"
        " 4: CSV Fipe file.\n"
      )
      ("indentation,i"
      , po::value<int>(&op.indentation)->default_value(-1)
      , "Indentation of the menu output.")
      ("sim-length,l"
      , po::value<int>(&op.sim_length)->default_value(2)
      , "Length of simulated children.")
      ("hash,a", "Output channel codes only.")
      ("fipe-csv-file,r"
      , po::value<std::string>(&op.file)
      , "Fipe CSV file containing all vehicles.")
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
      case 3: from_file(op); break;
      case 4: fipe_csv(op); break;
      default:
         op0(op);
   }

   return 0;
}

