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
{ id, tipo, id_modelo_ano, fipe_codigo, id_marca, marca
, id_modelo, modelo, ano, name, combustivel, preco
};

struct comp_helper {
   std::string s;
   fipe_fields f;
   auto operator()(std::vector<std::string> const& v) const
   {
      return s == v.at(f);
   }
};

struct line_comp {
   fipe_fields f;
   auto operator()( std::vector<std::string> const& v1
                  , std::vector<std::string> const& v2) const
   { return v1.at(f) < v2.at(f); };
};

struct helper {
   using iter_type = std::vector<std::vector<std::string>>::iterator;
   iter_type begin;
   iter_type end;
   fipe_fields f;
};

template <class Iter>
void print_partitions(Iter begin, Iter end, fipe_fields f)
{
   std::stack<helper> st;
   st.push({begin, end, fipe_fields::marca});

   while (!std::empty(st)) {
      auto const h = st.top();
      st.pop();
      std::sort(h.begin, h.end, line_comp {h.f});

      auto tot_partitions = 0;
      auto tor_items = 0;
      while (begin != end) {
         auto old_begin = begin;
         begin = std::partition_point( begin, end
                                     , comp_helper {begin->at(h.f), h.f});

         auto const k = std::distance(old_begin, begin);
         tor_items += k;
         ++tot_partitions;
         std::cout << old_begin->at(f) << ": " << k << std::endl;
      }

      std::cout << std::endl;
      std::cout << "Number of models: " << tor_items << std::endl;
      std::cout << "Number of partitions: " << tot_partitions << std::endl;
   }
}

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

   //for (auto const& o : table)
   //   std::cout << o.at(fipe_fields::marca) << " ===> " << o.at(fipe_fields::modelo) << std::endl;

   print_partitions( std::begin(table), std::end(table)
                   , fipe_fields::marca);
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

